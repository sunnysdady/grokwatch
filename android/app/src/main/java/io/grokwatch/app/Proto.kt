package io.grokwatch.app

import java.io.ByteArrayOutputStream
import java.util.UUID

/** GrokWatch BLE protocol v1. Frame = [type u8][len u16 LE][payload]. One frame per GATT write (<=240 B). */
object Proto {
    val SVC: UUID = UUID.fromString("7a6e0001-4772-6f6b-5761-746368000001")
    val RX: UUID = UUID.fromString("7a6e0002-4772-6f6b-5761-746368000001")   // phone -> watch (write)
    val TX: UUID = UUID.fromString("7a6e0003-4772-6f6b-5761-746368000001")   // watch -> phone (notify)
    val CCCD: UUID = UUID.fromString("00002902-0000-1000-8000-00805f9b34fb")
    const val HELLO = 0x01; const val NAV_STATE = 0x10; const val NAV_DATA = 0x11; const val NAV_ICON = 0x12
    const val SPEED = 0x20; const val NOTIFY = 0x30; const val TIME = 0x40

    fun frame(type: Int, payload: ByteArray): ByteArray {
        val o = ByteArray(3 + payload.size); o[0] = type.toByte()
        o[1] = (payload.size and 0xFF).toByte(); o[2] = (payload.size shr 8).toByte()
        System.arraycopy(payload, 0, o, 3, payload.size); return o
    }
    private fun strs(vararg s: String, max: Int = 230): ByteArray {
        val b = ByteArrayOutputStream()
        for (x in s) { var bytes = x.toByteArray(Charsets.UTF_8); if (bytes.size > 90) bytes = cut(x, 90); b.write(bytes); b.write(0) }
        val r = b.toByteArray(); return if (r.size > max) r.copyOf(max) else r
    }
    private fun cut(s: String, n: Int): ByteArray { var t = s; while (t.toByteArray().size > n) t = t.dropLast(1); return t.toByteArray() }
    fun hello(phoneBatt: Int, flags: Int) = frame(HELLO, byteArrayOf(phoneBatt.toByte(), flags.toByte()))
    fun navState(active: Boolean) = frame(NAV_STATE, byteArrayOf(if (active) 1 else 0))
    /** 0x11 = dist\0 road\0 dur\0 eta\0 [ext\0]. ext (0.6.2+, optional) = "k=v;k=v", e.g. "L=23" traffic light seconds. Old firmware reads 4 strings and ignores the rest. */
    fun navData(dist: String, road: String, dur: String, eta: String, ext: String = "") =
        frame(NAV_DATA, if (ext.isEmpty()) strs(dist, road, dur, eta) else strs(dist, road.take(30), dur, eta, ext))
    fun navIcon(part: Int, bits: ByteArray) = frame(NAV_ICON, byteArrayOf(part.toByte()) + bits.copyOfRange(part * 96, part * 96 + 96))
    fun speed(kmh10: Int) = frame(SPEED, byteArrayOf((kmh10 and 0xFF).toByte(), (kmh10 shr 8).toByte()))
    fun notify(app: String, title: String, body: String) = frame(NOTIFY, strs(app, title, body))
    fun time(unix: Long, tzMin: Int) = frame(TIME, byteArrayOf(
        unix.toByte(), (unix shr 8).toByte(), (unix shr 16).toByte(), (unix shr 24).toByte(), tzMin.toByte(), (tzMin shr 8).toByte()))
}
