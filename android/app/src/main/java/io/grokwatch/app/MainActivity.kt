package io.grokwatch.app

import android.Manifest
import android.content.ComponentName
import android.content.Intent
import android.content.pm.PackageManager
import android.net.Uri
import android.os.Bundle
import android.os.PowerManager
import android.provider.Settings
import androidx.activity.ComponentActivity
import androidx.activity.compose.setContent
import androidx.activity.result.contract.ActivityResultContracts
import androidx.compose.animation.animateColorAsState
import androidx.compose.animation.core.*
import androidx.compose.foundation.*
import androidx.compose.foundation.layout.*
import androidx.compose.foundation.shape.CircleShape
import androidx.compose.foundation.shape.RoundedCornerShape
import androidx.compose.material3.*
import androidx.compose.runtime.*
import androidx.compose.ui.Alignment
import androidx.compose.ui.Modifier
import androidx.compose.ui.draw.clip
import androidx.compose.ui.graphics.Color
import androidx.compose.ui.graphics.StrokeCap
import androidx.compose.ui.graphics.drawscope.Stroke
import androidx.compose.ui.text.font.FontFamily
import androidx.compose.ui.text.font.FontWeight
import androidx.compose.ui.unit.dp
import androidx.compose.ui.unit.sp
import androidx.core.app.NotificationManagerCompat
import androidx.lifecycle.compose.collectAsStateWithLifecycle
import kotlinx.coroutines.launch

private val ORG = Color(0xFFFF6A13); private val CARD = Color(0xFF1C1C1E); private val SEP = Color(0xFF2C2C2E)
private val SEC = Color(0xFF8E8E93); private val GRN = Color(0xFF30D158); private val RED = Color(0xFFFF453A)
private val Num = FontFamily.Default

class MainActivity : ComponentActivity() {
    private val perms = registerForActivityResult(ActivityResultContracts.RequestMultiplePermissions()) { LinkService.start(this); tick++ }
    private var tick by mutableIntStateOf(0)
    override fun onResume() { super.onResume(); tick++ }
    override fun onCreate(s: Bundle?) {
        super.onCreate(s)
        val need = arrayOf(Manifest.permission.BLUETOOTH_SCAN, Manifest.permission.BLUETOOTH_CONNECT, Manifest.permission.ACCESS_FINE_LOCATION, Manifest.permission.POST_NOTIFICATIONS)
        if (need.any { checkSelfPermission(it) != PackageManager.PERMISSION_GRANTED }) perms.launch(need) else LinkService.start(this)
        setContent { MaterialTheme(colorScheme = darkColorScheme(primary = ORG, background = Color.Black, surface = CARD)) { Screen(tick) } }
    }
    private fun ok(p: String) = checkSelfPermission(p) == PackageManager.PERMISSION_GRANTED
    private fun notiOk() = NotificationManagerCompat.getEnabledListenerPackages(this).contains(packageName)
    private fun batteryOk() = getSystemService(PowerManager::class.java).isIgnoringBatteryOptimizations(packageName)

    @Composable fun Screen(t: Int) {
        val link by Hub.link.collectAsStateWithLifecycle(); val nav by Hub.nav.collectAsStateWithLifecycle()
        val spd by Hub.speed.collectAsStateWithLifecycle(); val sent by Hub.sent.collectAsStateWithLifecycle()
        val dev by Hub.device.collectAsStateWithLifecycle(); val note by Hub.lastNote.collectAsStateWithLifecycle()
        val p = Hub.prefs(this)
        Column(Modifier.fillMaxSize().background(Color.Black).verticalScroll(rememberScrollState()).systemBarsPadding().padding(horizontal = 20.dp)) {
            Spacer(Modifier.height(18.dp))
            Text("GROKWATCH", color = ORG, fontSize = 13.sp, fontWeight = FontWeight.Bold, letterSpacing = 3.sp)
            Text("手表连接", color = Color.White, fontSize = 34.sp, fontWeight = FontWeight.Bold)
            Spacer(Modifier.height(20.dp))
            Hero(link, dev, nav, spd)
            Spacer(Modifier.height(24.dp)); Section("准备")
            Card {
                Row_("通知使用权", "读取高德 / Google 地图导航和消息", notiOk()) { startActivity(Intent(Settings.ACTION_NOTIFICATION_LISTENER_SETTINGS)) }
                Sep()
                Row_("蓝牙与定位", "连接手表、发送车速", ok(Manifest.permission.BLUETOOTH_CONNECT) && ok(Manifest.permission.ACCESS_FINE_LOCATION)) {
                    perms.launch(arrayOf(Manifest.permission.BLUETOOTH_SCAN, Manifest.permission.BLUETOOTH_CONNECT, Manifest.permission.ACCESS_FINE_LOCATION, Manifest.permission.POST_NOTIFICATIONS)) }
                Sep()
                Row_("后台常驻", "关闭电池优化，防止 ColorOS 杀后台", batteryOk()) {
                    try { startActivity(Intent(Settings.ACTION_REQUEST_IGNORE_BATTERY_OPTIMIZATIONS, Uri.parse("package:$packageName"))) }
                    catch (_: Exception) { startActivity(Intent(Settings.ACTION_APPLICATION_DETAILS_SETTINGS, Uri.parse("package:$packageName"))) } }
            }
            Spacer(Modifier.height(24.dp)); Section("转发")
            Card {
                Toggle("导航", "转弯箭头、距离、道路、到达时间", p.getBoolean("nav", true)) { p.edit().putBoolean("nav", it).apply() }
                Sep(); Toggle("消息通知", "微信、短信等普通通知", p.getBoolean("notify", true)) { p.edit().putBoolean("notify", it).apply() }
                Sep(); Toggle("车速", "手机 GPS，每秒一次", p.getBoolean("speed", true)) { p.edit().putBoolean("speed", it).apply() }
            }
            Spacer(Modifier.height(24.dp)); Section("调试")
            Card {
                Action("发送测试导航", "手表导航页应显示 350 m 左转") {
                    Hub.testNav = Nav(true, "350 m", "科技园南路", "12 分钟", "19:55", Turn.LEFT, "测试"); Hub.testIcon = NavParser.iconBits(null, Turn.LEFT); Hub.applyNav() }
                Sep(); Action("结束测试导航", "恢复真实导航（若高德/地图正在导航）") { Hub.testNav = null; Hub.testIcon = null; Hub.applyNav() }
                Sep(); Action("重新配对手表", "忘记当前手表并重新扫描") { LinkService.forget(this@MainActivity) }
            }
            Spacer(Modifier.height(24.dp)); Section("更新")
            val ust by Updater.status.collectAsStateWithLifecycle(); val scope = rememberCoroutineScope()
            var uurl by remember { mutableStateOf(Updater.url(this@MainActivity)) }
            val ver = remember { packageManager.getPackageInfo(packageName, 0).versionName }
            Card {
                Column(Modifier.padding(horizontal = 20.dp, vertical = 14.dp)) {
                    Text("当前版本 $ver", color = Color.White, fontSize = 17.sp, fontWeight = FontWeight.SemiBold)
                    Spacer(Modifier.height(8.dp))
                    OutlinedTextField(uurl, { uurl = it; Updater.setUrl(this@MainActivity, it) }, singleLine = true, label = { Text("更新清单地址 (manifest.json)") },
                        textStyle = androidx.compose.ui.text.TextStyle(fontSize = 13.sp, color = Color.White), modifier = Modifier.fillMaxWidth(),
                        colors = OutlinedTextFieldDefaults.colors(focusedBorderColor = ORG, unfocusedBorderColor = SEP, focusedLabelColor = ORG, unfocusedLabelColor = SEC, cursorColor = ORG))
                    if (ust.isNotEmpty()) Text(ust, color = ORG, fontSize = 13.sp, modifier = Modifier.padding(top = 6.dp))
                }
                Sep(); Action("检查更新", "下载并安装新版本（需同一签名）") {
                    scope.launch { Updater.check(this@MainActivity)?.let { i ->
                        if (!packageManager.canRequestPackageInstalls()) startActivity(Intent(Settings.ACTION_MANAGE_UNKNOWN_APP_SOURCES, Uri.parse("package:$packageName")))
                        else Updater.install(this@MainActivity, i) } } }
            }
            Spacer(Modifier.height(24.dp)); Section("导航解析日志")
            val log by Hub.navLog.collectAsStateWithLifecycle()
            Card { Column(Modifier.padding(16.dp)) {
                if (log.isEmpty()) Text("开始高德/谷歌地图导航后，这里会显示读取到的原始字段和解析结果", color = SEC, fontSize = 13.sp)
                log.forEach { Text(it, color = Color(0xFFD1D1D6), fontSize = 11.sp, fontFamily = FontFamily.Monospace, modifier = Modifier.padding(vertical = 3.dp)) } }
                Sep(); Action("分享完整日志", "导出本次骑行所有导航通知原始数据") {
                    val txt = NavDumper.read(this@MainActivity).takeLast(400_000).ifEmpty { "（暂无导航日志）" }
                    startActivity(Intent.createChooser(Intent(Intent.ACTION_SEND).setType("text/plain").putExtra(Intent.EXTRA_SUBJECT, "GrokWatch 导航日志").putExtra(Intent.EXTRA_TEXT, txt), "分享日志")) }
                Sep(); Action("复制最近一条原始数据", "复制到剪贴板") {
                    getSystemService(android.content.ClipboardManager::class.java).setPrimaryClip(android.content.ClipData.newPlainText("nav", Hub.navDump.value))
                    android.widget.Toast.makeText(this@MainActivity, "已复制", android.widget.Toast.LENGTH_SHORT).show() }
                Sep(); Action("清空日志", "开始新一次骑行前可清空") { NavDumper.clear(this@MainActivity); Hub.navLog.value = emptyList(); Hub.navDump.value = "" }
            }
            val dump by Hub.navDump.collectAsStateWithLifecycle()
            if (dump.isNotEmpty()) { Spacer(Modifier.height(12.dp)); Card { Text(dump.take(6000), color = SEC, fontSize = 10.sp, fontFamily = FontFamily.Monospace, modifier = Modifier.padding(16.dp)) } }
            Spacer(Modifier.height(14.dp))
            Text("已发送 $sent 帧" + (if (note.isNotEmpty()) " · 最近：$note" else ""), color = SEC, fontSize = 12.sp, maxLines = 1)
            Spacer(Modifier.height(30.dp))
        }
    }
    @Composable fun Hero(link: Link, dev: String, nav: Nav, spd: Float) {
        val col by animateColorAsState(when (link) { Link.CONNECTED -> ORG; Link.OFF -> RED; else -> SEC }, label = "c")
        val inf = rememberInfiniteTransition(label = "s")
        val rot by inf.animateFloat(0f, 360f, infiniteRepeatable(tween(1400, easing = LinearEasing)), label = "r")
        Box(Modifier.fillMaxWidth().clip(RoundedCornerShape(28.dp)).background(CARD).padding(22.dp)) {
            Row(verticalAlignment = Alignment.CenterVertically) {
                Box(Modifier.size(92.dp), contentAlignment = Alignment.Center) {
                    Canvas(Modifier.fillMaxSize()) {
                        drawArc(SEP, 0f, 360f, false, style = Stroke(9.dp.toPx()))
                        if (link == Link.CONNECTED) drawArc(col, -90f, 360f, false, style = Stroke(9.dp.toPx(), cap = StrokeCap.Round))
                        else if (link != Link.OFF) drawArc(col, rot, 80f, false, style = Stroke(9.dp.toPx(), cap = StrokeCap.Round))
                        else drawArc(col, -90f, 40f, false, style = Stroke(9.dp.toPx(), cap = StrokeCap.Round))
                    }
                    Text(if (spd >= 0) "%.0f".format(spd) else "--", color = Color.White, fontSize = 30.sp, fontWeight = FontWeight.Bold, fontFamily = Num)
                }
                Spacer(Modifier.width(18.dp))
                Column {
                    Text(when (link) { Link.CONNECTED -> "已连接"; Link.SCANNING -> "正在寻找手表"; Link.CONNECTING -> "正在连接"; Link.OFF -> "未连接" },
                        color = Color.White, fontSize = 22.sp, fontWeight = FontWeight.Bold)
                    Text(if (link == Link.CONNECTED) dev else "请靠近手表，保持蓝牙开启", color = SEC, fontSize = 14.sp)
                    Spacer(Modifier.height(8.dp))
                    Text(if (nav.active) "${nav.app} · ${nav.dist} ${nav.road}" else "未在导航 · km/h", color = if (nav.active) ORG else SEC, fontSize = 13.sp, fontWeight = FontWeight.SemiBold, maxLines = 1)
                }
            }
        }
    }
    @Composable fun Section(t: String) { Text(t, color = SEC, fontSize = 13.sp, fontWeight = FontWeight.SemiBold, modifier = Modifier.padding(start = 6.dp, bottom = 8.dp)) }
    @Composable fun Card(c: @Composable ColumnScope.() -> Unit) { Column(Modifier.fillMaxWidth().clip(RoundedCornerShape(28.dp)).background(CARD), content = c) }
    @Composable fun Sep() { Box(Modifier.padding(start = 20.dp).fillMaxWidth().height(0.6.dp).background(SEP)) }
    @Composable fun Row_(t: String, s: String, done: Boolean, onClick: () -> Unit) {
        Row(Modifier.fillMaxWidth().clickable(onClick = onClick).padding(horizontal = 20.dp, vertical = 16.dp), verticalAlignment = Alignment.CenterVertically) {
            Box(Modifier.size(26.dp).clip(CircleShape).background(if (done) GRN else SEP), contentAlignment = Alignment.Center) {
                Text(if (done) "✓" else "!", color = if (done) Color.Black else ORG, fontSize = 14.sp, fontWeight = FontWeight.Bold) }
            Spacer(Modifier.width(14.dp))
            Column(Modifier.weight(1f)) { Text(t, color = Color.White, fontSize = 17.sp, fontWeight = FontWeight.SemiBold); Text(s, color = SEC, fontSize = 13.sp) }
            Text(if (done) "已开启" else "去开启", color = if (done) SEC else ORG, fontSize = 14.sp, fontWeight = FontWeight.SemiBold)
        }
    }
    @Composable fun Toggle(t: String, s: String, init: Boolean, on: (Boolean) -> Unit) {
        var v by remember { mutableStateOf(init) }
        Row(Modifier.fillMaxWidth().padding(horizontal = 20.dp, vertical = 12.dp), verticalAlignment = Alignment.CenterVertically) {
            Column(Modifier.weight(1f)) { Text(t, color = Color.White, fontSize = 17.sp, fontWeight = FontWeight.SemiBold); Text(s, color = SEC, fontSize = 13.sp) }
            Switch(v, { v = it; on(it) }, colors = SwitchDefaults.colors(checkedTrackColor = ORG, checkedThumbColor = Color.White, uncheckedTrackColor = SEP, uncheckedBorderColor = SEP, uncheckedThumbColor = SEC))
        }
    }
    @Composable fun Action(t: String, s: String, onClick: () -> Unit) {
        Row(Modifier.fillMaxWidth().clickable(onClick = onClick).padding(horizontal = 20.dp, vertical = 16.dp), verticalAlignment = Alignment.CenterVertically) {
            Column(Modifier.weight(1f)) { Text(t, color = ORG, fontSize = 17.sp, fontWeight = FontWeight.SemiBold); Text(s, color = SEC, fontSize = 13.sp) }
            Text("›", color = SEC, fontSize = 22.sp)
        }
    }
}
