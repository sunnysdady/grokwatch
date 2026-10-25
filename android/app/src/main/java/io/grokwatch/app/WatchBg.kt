package io.grokwatch.app

import android.content.Context
import android.graphics.ImageDecoder
import android.net.Uri
import kotlinx.coroutines.flow.MutableStateFlow
import kotlin.math.roundToInt

/** Watch-face background: centre-crop to 466x466, dim to 60 % so the white hands stay readable, RGB565 LE, BLE 0x60-0x62. */
object WatchBg {
    private const val W = 466
    val progress = MutableStateFlow(-1)   // 0..100 while sending, -1 idle
    fun pixels(ctx: Context, uri: Uri): ByteArray {
        val bm = ImageDecoder.decodeBitmap(ImageDecoder.createSource(ctx.contentResolver, uri)) { d, info, _ ->
            d.allocator = ImageDecoder.ALLOCATOR_SOFTWARE; val s = W.toFloat() / minOf(info.size.width, info.size.height)
            d.setTargetSize((info.size.width * s).roundToInt().coerceAtLeast(W), (info.size.height * s).roundToInt().coerceAtLeast(W)) }
        val px = IntArray(W * W); bm.getPixels(px, 0, W, (bm.width - W) / 2, (bm.height - W) / 2, W, W)
        val out = ByteArray(W * W * 2)
        for (i in px.indices) { val c = px[i]; val r = (c shr 16 and 255) * 6 / 10; val g = (c shr 8 and 255) * 6 / 10; val b = (c and 255) * 6 / 10
            val v = (r shr 3 shl 11) or (g shr 2 shl 5) or (b shr 3); out[2 * i] = v.toByte(); out[2 * i + 1] = (v shr 8).toByte() }
        return out
    }
    private fun fnv(b: ByteArray): Int { var h = -2128831035; for (x in b) { h = h xor (x.toInt() and 255); h *= 16777619 }; return h }
    private fun le32(v: Int) = byteArrayOf(v.toByte(), (v shr 8).toByte(), (v shr 16).toByte(), (v shr 24).toByte())
    /** Blocking, ~15-30 s: call off the main thread. Returns false if the link dropped. */
    fun send(px: ByteArray): Boolean {
        val ble = Hub.service?.ble ?: return false
        ble.fast(true); ble.send(Proto.frame(Proto.BG_BEGIN, ByteArray(0)))
        try {
            var off = 0
            while (off < px.size) {
                while (ble.pending() > 20) { if (Hub.link.value != Link.CONNECTED) return false; Thread.sleep(5) }
                val n = minOf(232, px.size - off)
                ble.send(Proto.frame(Proto.BG_DATA, le32(off) + px.copyOfRange(off, off + n)))
                off += n; progress.value = off * 100 / px.size
            }
            ble.send(Proto.frame(Proto.BG_END, le32(fnv(px)))); return true
        } finally { progress.value = -1; ble.fast(false) }
    }
    fun clear() { Hub.send(Proto.frame(Proto.BG_CLEAR, ByteArray(0))) }
}
