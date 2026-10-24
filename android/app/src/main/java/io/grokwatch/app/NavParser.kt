package io.grokwatch.app

import android.graphics.*

enum class Turn { STRAIGHT, LEFT, RIGHT, SLIGHT_LEFT, SLIGHT_RIGHT, SHARP_LEFT, SHARP_RIGHT, UTURN, ARRIVE, ROUNDABOUT }

/** Best-effort parsing of Google Maps / 高德 navigation notifications (text fields). */
object NavParser {
    const val GMAPS = "com.google.android.apps.maps"
    const val AMAP = "com.autonavi.minimap"
    val NAV_APPS = setOf(GMAPS, AMAP, "com.autonavi.amapauto")
    private val DIST = Regex("""(\d+(?:\.\d+)?)\s*(公里|千米|米|km|m)(?![a-z])""", RegexOption.IGNORE_CASE)
    private val ETA = Regex("""(\d{1,2}:\d{2})""")
    private val DUR = Regex("""(\d+\s*小时\s*\d*\s*分钟?|\d+\s*分钟|\d+\s*min|\d+\s*h\s*\d*\s*min)""", RegexOption.IGNORE_CASE)
    private val ROAD = Regex("""(?:进入|驶入|上|沿|朝|往|onto|on|toward)\s*([^\s，,。·]{2,24})""", RegexOption.IGNORE_CASE)

    fun turnOf(t: String): Turn { val s = t.lowercase()
        return when {
            "掉头" in s || "u-turn" in s || "u turn" in s -> Turn.UTURN
            "环岛" in s || "roundabout" in s -> Turn.ROUNDABOUT
            "到达" in s || "目的地" in s || "arrive" in s || "destination" in s -> Turn.ARRIVE
            "向左后" in s || "急左" in s || "sharp left" in s -> Turn.SHARP_LEFT
            "向右后" in s || "急右" in s || "sharp right" in s -> Turn.SHARP_RIGHT
            "靠左" in s || "左前" in s || "slight left" in s || "keep left" in s -> Turn.SLIGHT_LEFT
            "靠右" in s || "右前" in s || "slight right" in s || "keep right" in s -> Turn.SLIGHT_RIGHT
            "左转" in s || "turn left" in s || "左拐" in s -> Turn.LEFT
            "右转" in s || "turn right" in s || "右拐" in s -> Turn.RIGHT
            else -> Turn.STRAIGHT } }

    private val DIST_ONLY = Regex("""^\s*(\d+(?:\.\d+)?)\s*(公里|千米|米|km|m)\s*$""", RegexOption.IGNORE_CASE)
    fun parse(pkg: String, title: String, text: String, sub: String, big: String, extra: List<String> = emptyList()): Nav {
        val all = (listOf(title, text, big, sub) + extra).filter { it.isNotBlank() }
        val joined = all.joinToString(" ")
        // next-turn distance: a standalone distance text wins, then the title, then anything
        val dist = all.firstOrNull { DIST_ONLY.matches(it) }?.trim() ?: DIST.find(title)?.value ?: all.firstNotNullOfOrNull { t -> if ("剩余" in t || "全程" in t) null else DIST.find(t)?.value } ?: ""
        val rest = all.firstOrNull { "剩余" in it || "全程" in it || "remaining" in it.lowercase() } ?: sub
        val eta = all.firstOrNull { ("到达" in it || "arriv" in it.lowercase()) && ETA.containsMatchIn(it) }?.let { ETA.find(it)?.value } ?: ETA.find(rest)?.value ?: ETA.find(joined)?.value ?: ""
        val dur = DUR.find(rest)?.value ?: DUR.find(joined)?.value ?: ""
        val road = all.firstNotNullOfOrNull { ROAD.find(it)?.groupValues?.get(1) }
            ?: all.firstOrNull { !DIST_ONLY.matches(it) && !ETA.containsMatchIn(it) && "剩余" !in it && it.length in 2..24 && it != "高德地图" }?.replace(DIST, "")?.trim() ?: ""
        val turnSrc = all.firstOrNull { turnOf(it) != Turn.STRAIGHT } ?: joined
        return Nav(true, dist, road, dur.replace(" ", ""), eta, turnOf(turnSrc), if (pkg == GMAPS) "Google 地图" else "高德地图")
    }

    /** 48x48 1bpp (MSB first) arrow bitmap, from the app's large icon when present, otherwise drawn from the turn code. */
    fun iconBits(src: Bitmap?, turn: Turn): ByteArray {
        val bmp = Bitmap.createBitmap(48, 48, Bitmap.Config.ARGB_8888); val c = Canvas(bmp)
        if (src != null) c.drawBitmap(src, null, Rect(0, 0, 48, 48), Paint(Paint.FILTER_BITMAP_FLAG)) else drawArrow(c, turn)
        val out = ByteArray(288)
        for (y in 0 until 48) for (x in 0 until 48) { val p = bmp.getPixel(x, y)
            val on = Color.alpha(p) > 110 && (Color.red(p) + Color.green(p) + Color.blue(p)) > 200
            if (on) { val i = y * 48 + x; out[i / 8] = (out[i / 8].toInt() or (0x80 shr (i % 8))).toByte() } }
        return out
    }
    private fun drawArrow(c: Canvas, t: Turn) {
        val p = Paint(Paint.ANTI_ALIAS_FLAG).apply { color = Color.WHITE; style = Paint.Style.STROKE; strokeWidth = 7f; strokeCap = Paint.Cap.ROUND; strokeJoin = Paint.Join.ROUND }
        val f = Paint(p).apply { style = Paint.Style.FILL }
        fun head(x: Float, y: Float, ang: Float) { c.save(); c.rotate(ang, x, y)
            c.drawPath(Path().apply { moveTo(x, y - 9); lineTo(x + 10, y + 5); lineTo(x - 10, y + 5); close() }, f); c.restore() }
        when (t) {
            Turn.STRAIGHT -> { c.drawLine(24f, 44f, 24f, 14f, p); head(24f, 10f, 0f) }
            Turn.LEFT, Turn.SHARP_LEFT -> { c.drawPath(Path().apply { moveTo(30f, 44f); lineTo(30f, 22f); quadTo(30f, 16f, 24f, 16f); lineTo(14f, 16f) }, p); head(10f, 16f, -90f) }
            Turn.RIGHT, Turn.SHARP_RIGHT -> { c.drawPath(Path().apply { moveTo(18f, 44f); lineTo(18f, 22f); quadTo(18f, 16f, 24f, 16f); lineTo(34f, 16f) }, p); head(38f, 16f, 90f) }
            Turn.SLIGHT_LEFT -> { c.drawPath(Path().apply { moveTo(28f, 44f); lineTo(28f, 28f); lineTo(16f, 14f) }, p); head(13f, 11f, -40f) }
            Turn.SLIGHT_RIGHT -> { c.drawPath(Path().apply { moveTo(20f, 44f); lineTo(20f, 28f); lineTo(32f, 14f) }, p); head(35f, 11f, 40f) }
            Turn.UTURN -> { c.drawPath(Path().apply { moveTo(32f, 44f); lineTo(32f, 20f); quadTo(32f, 10f, 23f, 10f); quadTo(14f, 10f, 14f, 20f); lineTo(14f, 30f) }, p); head(14f, 34f, 180f) }
            Turn.ROUNDABOUT -> { c.drawCircle(24f, 20f, 9f, p); c.drawLine(24f, 44f, 24f, 29f, p); head(33f, 12f, 45f) }
            Turn.ARRIVE -> { c.drawCircle(24f, 18f, 10f, p); c.drawLine(24f, 28f, 24f, 44f, p); c.drawCircle(24f, 18f, 3f, f) }
        }
    }
}
