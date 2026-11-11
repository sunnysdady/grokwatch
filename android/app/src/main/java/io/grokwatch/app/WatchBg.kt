package io.grokwatch.app

import android.content.Context
import android.graphics.ColorSpace
import android.graphics.Bitmap
import android.graphics.ImageDecoder
import android.graphics.Rect
import android.net.Uri
import kotlinx.coroutines.flow.MutableStateFlow
import kotlin.math.roundToInt

/** Watch-face background: selected crop to native 466x466 sRGB, dithered RGB565 LE, BLE 0x60-0x62. */
object WatchBg {
    private const val W = WatchBgPixels.SIZE
    val progress = MutableStateFlow(-1)   // 0..100 while sending, -1 idle
    data class Preview(val bitmap: Bitmap, val sourceWidth: Int, val sourceHeight: Int)
    fun preview(ctx: Context, uri: Uri): Preview {
        var width = 0; var height = 0
        val bitmap = ImageDecoder.decodeBitmap(ImageDecoder.createSource(ctx.contentResolver, uri)) { d, info, _ ->
            width = info.size.width; height = info.size.height
            d.allocator = ImageDecoder.ALLOCATOR_SOFTWARE
            d.setTargetColorSpace(ColorSpace.get(ColorSpace.Named.SRGB))
            val scale = minOf(1.0, 1600.0 / maxOf(width, height))
            d.setTargetSize((width * scale).roundToInt().coerceAtLeast(1), (height * scale).roundToInt().coerceAtLeast(1))
        }
        return Preview(bitmap, width, height)
    }
    fun pixels(ctx: Context, uri: Uri, zoom: Double = 1.0, centerX: Double = 0.5, centerY: Double = 0.5): ByteArray {
        val bm = ImageDecoder.decodeBitmap(ImageDecoder.createSource(ctx.contentResolver, uri)) { d, info, _ ->
            d.allocator = ImageDecoder.ALLOCATOR_SOFTWARE
            d.setTargetColorSpace(ColorSpace.get(ColorSpace.Named.SRGB))
            val crop = WatchBgCrop(info.size.width, info.size.height, zoom, centerX, centerY)
            val s = W / crop.side
            val width = (info.size.width * s).roundToInt().coerceAtLeast(W)
            val height = (info.size.height * s).roundToInt().coerceAtLeast(W)
            require(width.toLong() * height <= 32_000_000) { "图片比例过长，请先裁剪原图" }
            d.setTargetSize(width, height)
            val x = (crop.left * s).roundToInt().coerceIn(0, width - W)
            val y = (crop.top * s).roundToInt().coerceIn(0, height - W)
            d.setCrop(Rect(x, y, x + W, y + W)) }
        val px = IntArray(W * W)
        try { bm.getPixels(px, 0, W, 0, 0, W, W) }
        finally { bm.recycle() }
        return WatchBgPixels.encode(px)
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
