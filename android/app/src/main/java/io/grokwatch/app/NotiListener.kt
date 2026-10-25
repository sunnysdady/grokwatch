package io.grokwatch.app

import android.app.Notification
import android.graphics.Bitmap
import android.graphics.drawable.Icon
import android.service.notification.NotificationListenerService
import android.service.notification.StatusBarNotification

/** Reads navigation (Google Maps / 高德) and regular notifications and forwards them to the watch. */
class NotiListener : NotificationListenerService() {
    private var navKey: String? = null
    private var lastSig = ""
    override fun onListenerConnected() { LinkService.start(this)
        try { activeNotifications?.filter { it.packageName in NavParser.NAV_APPS }?.forEach { onNotificationPosted(it) } } catch (_: Exception) {} }

    override fun onNotificationPosted(sbn: StatusBarNotification) {
        val n = sbn.notification ?: return; val e = n.extras
        val title = e.getCharSequence(Notification.EXTRA_TITLE)?.toString() ?: ""
        val text = e.getCharSequence(Notification.EXTRA_TEXT)?.toString() ?: ""
        val sub = e.getCharSequence(Notification.EXTRA_SUB_TEXT)?.toString() ?: ""
        val big = e.getCharSequence(Notification.EXTRA_BIG_TEXT)?.toString() ?: ""
        if (sbn.packageName in NavParser.NAV_APPS && (n.flags and Notification.FLAG_ONGOING_EVENT) != 0) {
            if (!Hub.flag(this, "nav")) return
            val dmp = NavDumper.dump(this, n, sbn.packageName)
            val nav = NavParser.parse(sbn.packageName, title, text, sub, big, dmp.texts)
            if (nav.dist.isEmpty() && nav.road.isEmpty()) { maybeLog(dmp, "未识别"); return }
            navKey = sbn.key
            // turn icon candidates: large icon, then square-ish images from RemoteViews / extras
            val cands = listOfNotNull(largeIcon(n)) + dmp.images.filter { it.width >= 32 && kotlin.math.abs(it.width - it.height) < it.width / 3 }
            var bits: ByteArray? = null; var src = "按文字画(${nav.turn})"
            for ((i, bm) in cands.withIndex()) { val b0 = NavParser.iconBits(bm, nav.turn); val on = b0.sumOf { Integer.bitCount(it.toInt() and 0xFF) }
                if (on in 40..1800) { bits = b0; src = "图#$i ${bm.width}x${bm.height} 亮点$on"; break } }
            if (bits == null) bits = NavParser.iconBits(null, nav.turn)
            val sig = "${nav.dist}|${nav.road}|${nav.dur}|${nav.eta}|${nav.turn}|${bits.contentHashCode()}"
            if (sig != lastSig) { lastSig = sig
                Hub.log("${nav.turn} ${nav.dist} | ${nav.road} | ${nav.dur} | ${nav.eta} | 箭头:$src")
                maybeLog(dmp, "解析=${nav.turn} 距离=${nav.dist} 道路=${nav.road} 剩余=${nav.dur} 到达=${nav.eta} 箭头=$src (候选图${cands.size})") }
            Hub.realNav = nav; Hub.realIcon = bits
            // a placeholder notification ("正在导航") must not override fresh accessibility data; curNav() already prefers it
            Hub.applyNav()
            return
        }
        if (!Hub.flag(this, "notify")) return
        if ((n.flags and (Notification.FLAG_ONGOING_EVENT or Notification.FLAG_GROUP_SUMMARY)) != 0) return
        if (sbn.packageName == packageName || (title.isEmpty() && text.isEmpty())) return
        if (n.category in setOf(Notification.CATEGORY_PROGRESS, Notification.CATEGORY_TRANSPORT, Notification.CATEGORY_SERVICE)) return
        val app = try { packageManager.getApplicationLabel(packageManager.getApplicationInfo(sbn.packageName, 0)).toString() } catch (_: Exception) { sbn.packageName }
        Hub.lastNote.value = "$app · $title"
        Hub.send(Proto.notify(app, title, (if (big.length > text.length) big else text).take(120)))
    }
    override fun onNotificationRemoved(sbn: StatusBarNotification) {
        if (sbn.key == navKey) { navKey = null; lastSig = ""; Hub.realNav = Nav(); Hub.realIcon = null; Hub.log("导航通知已移除 → 结束导航"); Hub.applyNav() }
    }
    private var lastDumpSig = ""
    private fun maybeLog(d: NavDump, head: String) {
        val sig = d.texts.joinToString("|"); if (sig == lastDumpSig) return; lastDumpSig = sig
        val t = java.text.SimpleDateFormat("HH:mm:ss", java.util.Locale.US).format(java.util.Date())
        val entry = "===== $t $head\n${d.dump}"; Hub.navDump.value = entry; NavDumper.append(this, entry)
        if (head == "未识别") Hub.log("未识别: ${d.texts.take(4).joinToString(" / ")}")
    }
    private fun largeIcon(n: Notification): Bitmap? = try {
        val ic: Icon? = n.getLargeIcon()
        (ic?.loadDrawable(this) as? android.graphics.drawable.BitmapDrawable)?.bitmap
            ?: ic?.loadDrawable(this)?.let { d -> Bitmap.createBitmap(96, 96, Bitmap.Config.ARGB_8888).also { b -> val c = android.graphics.Canvas(b); d.setBounds(0, 0, 96, 96); d.draw(c) } }
    } catch (_: Exception) { null }
}
