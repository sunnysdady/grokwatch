package io.grokwatch.app

import android.graphics.Bitmap
import android.graphics.Color
import android.net.Uri
import androidx.compose.material3.MaterialTheme
import androidx.compose.ui.test.*
import androidx.compose.ui.graphics.asAndroidBitmap
import androidx.compose.ui.test.junit4.createComposeRule
import androidx.test.platform.app.InstrumentationRegistry
import org.junit.Assert.*
import org.junit.Rule
import org.junit.Test
import java.io.File

class WatchBgCropScreenTest {
    @get:Rule val rule = createComposeRule()
    private fun fixture(size: Int = 1000): Uri {
        val ctx = InstrumentationRegistry.getInstrumentation().targetContext
        val file = File(ctx.cacheDir, "crop-fixture-$size.png")
        val bitmap = Bitmap.createBitmap(size * 2, size, Bitmap.Config.ARGB_8888)
        for (y in 0 until size) for (x in 0 until size * 2)
            bitmap.setPixel(x, y, if (x < size) Color.RED else Color.BLUE)
        file.outputStream().use { bitmap.compress(Bitmap.CompressFormat.PNG, 100, it) }
        bitmap.recycle()
        return Uri.fromFile(file)
    }
    private fun loaded() = rule.waitUntil(15000) {
        rule.onAllNodesWithContentDescription("背景缩放").fetchSemanticsNodes().isNotEmpty()
    }
    @Test fun cropZoomResetConfirmAndCancel() {
        var result: Triple<Double, Double, Double>? = null
        var cancelled = false
        var reselected = false
        val uri = fixture()
        rule.setContent { MaterialTheme { WatchBgCropScreen(uri, false, -1, true,
            { cancelled = true }, { reselected = true }, { z, x, y -> result = Triple(z, x, y) }) } }
        loaded()
        val ctx = InstrumentationRegistry.getInstrumentation().targetContext
        File(ctx.getExternalFilesDir(null), "crop-preview.png").outputStream().use {
            rule.onRoot().captureToImage().asAndroidBitmap().compress(Bitmap.CompressFormat.PNG, 100, it)
        }
        rule.onNodeWithContentDescription("背景缩放").performSemanticsAction(androidx.compose.ui.semantics.SemanticsActions.SetProgress) { it(2f) }
        rule.onNodeWithText("确认裁切并传到手表").performScrollTo().performClick()
        rule.runOnIdle { assertEquals(2.0, result!!.first, 0.001) }
        rule.onNodeWithText("重置位置").performScrollTo().performClick()
        rule.onNodeWithText("确认裁切并传到手表").performScrollTo().performClick()
        rule.runOnIdle { assertEquals(Triple(1.0, 0.5, 0.5), result) }
        rule.onNodeWithText("重新选图").performScrollTo().performClick()
        rule.onNodeWithText("取消").performScrollTo().performClick()
        rule.runOnIdle { assertTrue(cancelled); assertTrue(reselected) }
    }
    @Test fun offlinePreviewDoesNotSend() {
        val uri = fixture(200)
        rule.setContent { MaterialTheme { WatchBgCropScreen(uri, false, -1, false, {}, {}, { _, _, _ -> fail("offline send") }) } }
        loaded()
        rule.onNodeWithText("确认裁切并传到手表").assertIsNotEnabled()
        rule.onNodeWithText("当前区域不足 466 像素，放大后可能模糊。请减小缩放或换用原图。").assertExists()
    }
    @Test fun busyLocksControls() {
        val uri = fixture()
        rule.setContent { MaterialTheme { WatchBgCropScreen(uri, true, 40, true, { fail("busy cancel") }, {}, { _, _, _ -> fail("duplicate send") }) } }
        loaded()
        rule.onNodeWithText("取消").assertIsNotEnabled()
        rule.onNodeWithText("重新选图").assertIsNotEnabled()
        rule.onNodeWithContentDescription("背景缩放").assertIsNotEnabled()
    }
    @Test fun unreadableImageCanBeReselected() {
        rule.setContent { MaterialTheme { WatchBgCropScreen(Uri.parse("file:///missing-image.png"), false, -1, true, {}, {}, { _, _, _ -> fail("invalid send") }) } }
        rule.waitUntil(15000) { rule.onAllNodesWithText("无法读取这张图片，请重新选择。").fetchSemanticsNodes().isNotEmpty() }
        rule.onNodeWithText("确认裁切并传到手表").assertIsNotEnabled()
        rule.onNodeWithText("重新选图").assertIsEnabled()
    }
    @Test fun originalCropProducesExpectedPixels() {
        val ctx = InstrumentationRegistry.getInstrumentation().targetContext
        val uri = fixture()
        val bytes = WatchBg.pixels(ctx, uri, 1.0, 0.25, 0.5)
        assertEquals(466 * 466 * 2, bytes.size)
        for (row in 0 until 466) for (col in 0 until 465) {
            val i = (row * 466 + col) * 2
            assertEquals(0, bytes[i].toInt() and 255)
            assertEquals(248, bytes[i + 1].toInt() and 255)
        }
    }
    @Test fun importedMidtonesKeepOriginalBrightness() {
        val ctx = InstrumentationRegistry.getInstrumentation().targetContext
        val file = File(ctx.cacheDir, "brightness.png")
        val image = Bitmap.createBitmap(932, 932, Bitmap.Config.ARGB_8888)
        image.eraseColor(Color.rgb(201, 145, 77))
        file.outputStream().use { image.compress(Bitmap.CompressFormat.PNG, 100, it) }
        image.recycle()
        val bytes = WatchBg.pixels(ctx, Uri.fromFile(file))
        var red = 0.0; var green = 0.0; var blue = 0.0
        for (i in bytes.indices step 2) {
            val rgb = (bytes[i].toInt() and 255) or ((bytes[i + 1].toInt() and 255) shl 8)
            red += (rgb shr 11) * 255.0 / 31
            green += ((rgb shr 5) and 63) * 255.0 / 63
            blue += (rgb and 31) * 255.0 / 31
        }
        val count = bytes.size / 2
        assertEquals(201.0, red / count, 1.0)
        assertEquals(145.0, green / count, 1.0)
        assertEquals(77.0, blue / count, 1.0)
    }

}
