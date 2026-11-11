package io.grokwatch.app

import android.graphics.Paint
import android.graphics.RectF
import android.net.Uri
import androidx.activity.compose.BackHandler
import androidx.compose.foundation.Canvas
import androidx.compose.foundation.background
import androidx.compose.foundation.gestures.detectTransformGestures
import androidx.compose.foundation.layout.*
import androidx.compose.foundation.rememberScrollState
import androidx.compose.foundation.verticalScroll
import androidx.compose.material3.*
import androidx.compose.runtime.*
import androidx.compose.runtime.saveable.rememberSaveable
import androidx.compose.ui.Alignment
import androidx.compose.ui.Modifier
import androidx.compose.ui.geometry.Rect
import androidx.compose.ui.graphics.Color
import androidx.compose.ui.graphics.Path
import androidx.compose.ui.graphics.drawscope.Stroke
import androidx.compose.ui.graphics.drawscope.clipPath
import androidx.compose.ui.graphics.nativeCanvas
import androidx.compose.ui.input.pointer.pointerInput
import androidx.compose.ui.platform.LocalContext
import androidx.compose.ui.semantics.contentDescription
import androidx.compose.ui.semantics.semantics
import androidx.compose.ui.unit.dp
import kotlinx.coroutines.CancellationException
import kotlinx.coroutines.Dispatchers
import kotlinx.coroutines.withContext

@Composable
fun WatchBgCropScreen(uri: Uri, busy: Boolean, progress: Int, connected: Boolean,
                      onCancel: () -> Unit, onReselect: () -> Unit,
                      onConfirm: (Double, Double, Double) -> Unit) {
    val context = LocalContext.current
    var preview by remember(uri) { mutableStateOf<WatchBg.Preview?>(null) }
    var error by remember(uri) { mutableStateOf<String?>(null) }
    var zoom by rememberSaveable(uri.toString()) { mutableDoubleStateOf(1.0) }
    var centerX by rememberSaveable(uri.toString()) { mutableDoubleStateOf(0.5) }
    var centerY by rememberSaveable(uri.toString()) { mutableDoubleStateOf(0.5) }
    val paint = remember { Paint(Paint.ANTI_ALIAS_FLAG or Paint.FILTER_BITMAP_FLAG) }
    LaunchedEffect(uri) {
        try { preview = withContext(Dispatchers.IO) { WatchBg.preview(context, uri) } }
        catch (e: CancellationException) { throw e }
        catch (_: Exception) { error = "无法读取这张图片，请重新选择。" }
    }
    BackHandler { if (!busy) onCancel() }
    Surface(color = Color.Black, contentColor = Color.White) {
    Column(Modifier.fillMaxSize().background(Color.Black).systemBarsPadding()
        .verticalScroll(rememberScrollState()).padding(24.dp), horizontalAlignment = Alignment.CenterHorizontally) {
        Text("裁切表盘背景", style = MaterialTheme.typography.headlineMedium)
        Spacer(Modifier.height(8.dp))
        Text("圆框内就是手表可见范围", color = Color.LightGray)
        Spacer(Modifier.height(24.dp))
        val photo = preview
        if (photo == null) {
            if (error == null) CircularProgressIndicator() else Text(error!!, color = MaterialTheme.colorScheme.error)
        } else {
            val bitmap = photo.bitmap
            fun update(newZoom: Double, x: Double, y: Double) {
                val crop = WatchBgCrop(photo.sourceWidth, photo.sourceHeight, newZoom.coerceIn(1.0, 4.0), x, y)
                zoom = newZoom.coerceIn(1.0, 4.0); centerX = crop.centerX; centerY = crop.centerY
            }
            Canvas(Modifier.fillMaxWidth().widthIn(max = 420.dp).aspectRatio(1f)
                .semantics { contentDescription = "圆形表盘裁切区域，可拖动图片或双指缩放" }
                .pointerInput(uri, busy, photo) {
                    if (!busy) detectTransformGestures { _, pan, scale, _ ->
                        val crop = WatchBgCrop(photo.sourceWidth, photo.sourceHeight, zoom, centerX, centerY)
                        val sourcePerPixel = crop.side / size.width.coerceAtLeast(1)
                        update(zoom * scale, centerX - pan.x * sourcePerPixel / photo.sourceWidth,
                            centerY - pan.y * sourcePerPixel / photo.sourceHeight)
                    }
                }) {
                val crop = WatchBgCrop(photo.sourceWidth, photo.sourceHeight, zoom, centerX, centerY)
                val scale = size.width / crop.side
                val circle = Path().apply { addOval(Rect(0f, 0f, size.width, size.height)) }
                clipPath(circle) {
                    drawRect(Color.Black)
                    drawContext.canvas.nativeCanvas.drawBitmap(bitmap, null, RectF(
                        (-crop.left * scale).toFloat(), (-crop.top * scale).toFloat(),
                        ((photo.sourceWidth - crop.left) * scale).toFloat(),
                        ((photo.sourceHeight - crop.top) * scale).toFloat()), paint)
                }
                drawCircle(Color.White.copy(alpha = 0.75f), radius = size.width / 2 - 1.dp.toPx(), style = Stroke(2.dp.toPx()))
            }
            Spacer(Modifier.height(16.dp))
            Text("拖动调整位置 · 双指或滑杆缩放", color = Color.LightGray)
            Slider(value = zoom.toFloat(), onValueChange = { update(it.toDouble(), centerX, centerY) },
                valueRange = 1f..4f, enabled = !busy,
                modifier = Modifier.semantics { contentDescription = "背景缩放" })
            val side = minOf(photo.sourceWidth, photo.sourceHeight) / zoom
            if (side < WatchBgPixels.SIZE) Text("当前区域不足 466 像素，放大后可能模糊。请减小缩放或换用原图。", color = MaterialTheme.colorScheme.error)
            Row {
                TextButton(onClick = { update(1.0, 0.5, 0.5) }, enabled = !busy) { Text("重置位置") }
                TextButton(onClick = onReselect, enabled = !busy) { Text("重新选图") }
            }
        }
        if (photo == null) TextButton(onClick = onReselect, enabled = !busy) { Text("重新选图") }
        Spacer(Modifier.height(16.dp))
        if (!connected) Text("连接手表后即可传输，裁切位置会保留。", color = Color.LightGray)
        Button(onClick = { onConfirm(zoom, centerX, centerY) }, enabled = photo != null && connected && !busy,
            modifier = Modifier.fillMaxWidth()) { Text(if (busy) "正在处理 / 传输 ${progress.coerceAtLeast(0)}%" else "确认裁切并传到手表") }
        TextButton(onClick = onCancel, enabled = !busy) { Text("取消") }
    }
    }
}
