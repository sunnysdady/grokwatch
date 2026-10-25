package io.grokwatch.app

import android.accessibilityservice.AccessibilityService
import android.content.Context
import android.os.SystemClock
import android.provider.Settings
import android.view.accessibility.AccessibilityEvent
import android.view.accessibility.AccessibilityNodeInfo

/**
 * Read-only accessibility channel: reads the visible text of the 高德 / Google Maps navigation screen
 * (turn, distance to turn, road, remaining time/distance, arrival time, traffic-light countdown).
 * Never clicks, types or scrolls. Throttled to at most one parse per second; only pushes on change.
 */
class NavA11y : AccessibilityService() {
    private var lastParse = 0L; private var lastSig = ""; private var lastRawSig = ""
    override fun onServiceConnected() { Hub.log("无障碍读取已连接") }
    override fun onInterrupt() {}
    override fun onAccessibilityEvent(e: AccessibilityEvent?) {
        val pkg = e?.packageName?.toString() ?: return
        if (pkg !in NavParser.NAV_APPS || !Hub.flag(this, "nav") || !Hub.flag(this, "nav_a11y")) return
        val now = SystemClock.elapsedRealtime(); if (now - lastParse < 1000) return; lastParse = now
        val root = try { rootInActiveWindow } catch (_: Throwable) { null } ?: return
        if (root.packageName?.toString() != pkg) return
        val nodes = ArrayList<A11yText>(); collect(root, 0, nodes)
        val nav = A11yNavParser.parse(pkg, nodes)
        val raw = nodes.joinToString("|") { it.text }
        if (nav == null) { if (raw != lastRawSig) { lastRawSig = raw; logRaw(nodes, "无障碍·未识别") }; return }
        Hub.accNav = nav; Hub.accT = now
        val sig = "${nav.dist}|${nav.road}|${nav.dur}|${nav.eta}|${nav.turn}|${nav.light}|${nav.rest}"
        if (sig != lastSig) { lastSig = sig
            Hub.accIcon = NavParser.iconBits(null, nav.turn)
            Hub.log("无障碍 ${nav.turn} ${nav.dist} | ${nav.road} | 剩${nav.rest} ${nav.dur} | ${nav.eta}" + (if (nav.light.isNotEmpty()) " | 红绿灯${nav.light}s" else ""))
            if (raw != lastRawSig) { lastRawSig = raw; logRaw(nodes, "无障碍·解析=${nav.turn} 距离=${nav.dist} 道路=${nav.road} 剩余=${nav.rest}/${nav.dur} 到达=${nav.eta} 红绿灯=${nav.light}") }
            Hub.applyNav() }
    }
    private fun collect(n: AccessibilityNodeInfo, d: Int, out: MutableList<A11yText>) {
        if (d > 40 || out.size > 400) return
        val id = n.viewIdResourceName?.substringAfter(":id/") ?: ""
        val t = A11yNavParser.strip(n.text?.toString() ?: ""); val desc = A11yNavParser.strip(n.contentDescription?.toString() ?: "")
        if (t.isNotEmpty() || desc.isNotEmpty()) out += A11yText(t.ifEmpty { desc }, desc, id, d)
        for (i in 0 until n.childCount) { val c = try { n.getChild(i) } catch (_: Throwable) { null } ?: continue; collect(c, d + 1, out) }
    }
    private fun logRaw(nodes: List<A11yText>, head: String) {
        val t = java.text.SimpleDateFormat("HH:mm:ss", java.util.Locale.US).format(java.util.Date())
        val body = nodes.take(120).joinToString("\n") { "  ".repeat((it.depth / 3).coerceAtMost(6)) + "[${it.id}] \"${A11yNavParser.redact(it.text)}\"" + (if (it.desc.isNotEmpty() && it.desc != it.text) " desc=\"${A11yNavParser.redact(it.desc)}\"" else "") }
        val entry = "===== $t $head\n$body"; Hub.navDump.value = entry; NavDumper.append(this, entry)
    }
    companion object {
        fun enabled(c: Context): Boolean = (Settings.Secure.getString(c.contentResolver, Settings.Secure.ENABLED_ACCESSIBILITY_SERVICES) ?: "")
            .split(':').any { it.equals("${c.packageName}/${NavA11y::class.java.name}", true) || it.equals("${c.packageName}/.NavA11y", true) }
    }
}

data class A11yText(val text: String, val desc: String, val id: String, val depth: Int)

/** Pure parser (no Android deps beyond data) so it can be unit-tested. Returns null when nothing trustworthy is visible. */
object A11yNavParser {
    private val TAG = Regex("<[^>]*>"); private val ENT = mapOf("&nbsp;" to " ", "&amp;" to "&", "&lt;" to "<", "&gt;" to ">", "&quot;" to "\"", "&#39;" to "'")
    fun strip(s: String): String { var r = TAG.replace(s, ""); for ((k, v) in ENT) r = r.replace(k, v); return r.replace(Regex("\\s+"), " ").trim() }
    /** drop anything that looks like a coordinate */
    fun redact(s: String) = s.replace(Regex("""-?\d{1,3}\.\d{4,}"""), "<坐标>")
    private val DIST_ONLY = Regex("""^(\d+(?:\.\d+)?)\s*(公里|千米|米|km|m)$""", RegexOption.IGNORE_CASE)
    private val DIST = Regex("""(\d+(?:\.\d+)?)\s*(公里|千米|米|km|m)(?![a-z])""", RegexOption.IGNORE_CASE)
    private val DUR = Regex("""(\d+\s*小时\s*(?:\d+\s*分钟?)?|\d+\s*分钟|\d+\s*min)""", RegexOption.IGNORE_CASE)
    private val CLOCK = Regex("""^(\d{1,2}:\d{2})$"""); private val CLOCK_IN = Regex("""(\d{1,2}:\d{2})""")
    private val NUM = Regex("""^\d{1,3}$""")
    private val AFTER = Regex("""(\d+(?:\.\d+)?)\s*(公里|米)\s*后""")
    private val ROAD = Regex("""(?:进入|驶入|汇入|上|沿|朝|往)\s*([^\s，,。·]{2,24})""")
    private val NOISE = setOf("高德地图", "正在导航", "到达", "剩余", "退出", "设置", "更多", "全览", "继续导航", "请先行驶到出口", "手机卫星信号弱")

    fun parse(pkg: String, nodes: List<A11yText>): Nav? {
        if (nodes.isEmpty()) return null
        val texts = nodes.map { it.text }
        fun idHas(n: A11yText, vararg k: String) = k.any { n.id.contains(it, true) }
        // remaining time + distance (may be one node "19分钟 5.1公里" or neighbours)
        var dur = ""; var rest = ""
        val durIdx = texts.indexOfFirst { DUR.containsMatchIn(it) && !CLOCK_IN.containsMatchIn(it) }
        if (durIdx >= 0) { dur = DUR.find(texts[durIdx])!!.value.replace(" ", "")
            rest = DIST.find(texts[durIdx])?.value?.replace(" ", "")
                ?: (maxOf(0, durIdx - 2)..minOf(texts.lastIndex, durIdx + 3)).firstNotNullOfOrNull { i -> if (i == durIdx) null else DIST_ONLY.find(texts[i])?.value?.replace(" ", "") } ?: "" }
        // arrival time: "10:46" next to "到达", or "10:46到达"
        val eta = texts.withIndex().firstNotNullOfOrNull { (i, t) ->
            if ("到达" in t && CLOCK_IN.containsMatchIn(t)) CLOCK_IN.find(t)!!.value
            else if (CLOCK.matches(t) && ((i > 0 && "到达" in texts[i - 1]) || (i < texts.lastIndex && "到达" in texts[i + 1]))) t else null } ?: ""
        // distance to the next maneuver: "300米后", or a standalone distance in a turn/maneuver view, or the first standalone one that isn't the remaining distance
        val restNorm = rest
        val dist = texts.firstNotNullOfOrNull { AFTER.find(it)?.let { m -> m.groupValues[1] + m.groupValues[2] } }
            ?: nodes.firstOrNull { idHas(it, "dis", "distance") && DIST_ONLY.matches(it.text) && it.text.replace(" ", "") != restNorm }?.text?.replace(" ", "")
            ?: nodes.firstOrNull { DIST_ONLY.matches(it.text) && it.text.replace(" ", "") != restNorm }?.text?.replace(" ", "") ?: ""
        // turn: keyword in text or content-description
        val turnSrc = nodes.firstOrNull { n -> listOf(n.text, n.desc).any { NavParser.turnOf(it) != Turn.STRAIGHT && "到达" !in it } }
        val straight = nodes.any { "直行" in it.text || "直行" in it.desc }
        val turn = turnSrc?.let { n -> listOf(n.text, n.desc).map { NavParser.turnOf(it) }.first { it != Turn.STRAIGHT } } ?: Turn.STRAIGHT
        // road: "进入XX路", or a view id mentioning road/street, or a short name-like text
        val road = texts.firstNotNullOfOrNull { t -> ROAD.find(t)?.groupValues?.get(1)?.takeIf { it !in NOISE } }
            ?: nodes.firstOrNull { idHas(it, "road", "street", "name") && it.text.length in 2..24 && it.text !in NOISE && !DIST.containsMatchIn(it.text) }?.text
            ?: nodes.firstOrNull { val t = it.text; t.length in 2..24 && t !in NOISE && (t.endsWith("路") || t.endsWith("街") || t.endsWith("道") || t.endsWith("大道") || t.endsWith("高速") || t.endsWith("桥") || t.endsWith("隧道") || t.endsWith("环线")) && !DIST.containsMatchIn(t) }?.text ?: ""
        // traffic light countdown: a 1-3 digit number in a light/traffic view, or next to a 红绿灯/红灯/绿灯 label, or "23秒"
        val light = nodes.withIndex().firstNotNullOfOrNull { (i, n) ->
            val lightCtx = idHas(n, "light", "traffic", "signal", "countdown") || listOf(n.text, n.desc).any { "红绿灯" in it || "红灯" in it || "绿灯" in it }
            if (!lightCtx) null
            else Regex("""(\d{1,3})\s*秒?""").find(n.text)?.groupValues?.get(1)?.takeIf { n.text.length <= 8 || "秒" in n.text }
                ?: listOfNotNull(nodes.getOrNull(i + 1), nodes.getOrNull(i - 1)).firstOrNull { NUM.matches(it.text) || Regex("""^\d{1,3}秒$""").matches(it.text) }?.text?.removeSuffix("秒") }
            ?.takeIf { (it.toIntOrNull() ?: 0) in 1..300 } ?: ""
        // trust gate: need a maneuver distance, or remaining time+distance, or an explicit turn instruction
        if (dist.isEmpty() && !(dur.isNotEmpty() && rest.isNotEmpty()) && turnSrc == null) return null
        if (turnSrc == null && !straight && dist.isEmpty() && road.isEmpty() && dur.isEmpty()) return null
        return Nav(true, dist, road, dur, eta, turn, (if (pkg == NavParser.GMAPS) "Google 地图" else "高德地图") + "·界面", light, rest)
    }
}
