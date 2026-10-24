package io.grokwatch.app

import android.content.Context
import kotlinx.coroutines.flow.MutableStateFlow

enum class Link { OFF, SCANNING, CONNECTING, CONNECTED }
data class Nav(val active: Boolean = false, val dist: String = "", val road: String = "", val dur: String = "", val eta: String = "", val turn: Turn = Turn.STRAIGHT, val app: String = "")

/** Process-wide state shared by service, listener and UI. */
object Hub {
    val link = MutableStateFlow(Link.OFF)
    val device = MutableStateFlow("")
    val nav = MutableStateFlow(Nav())
    val speed = MutableStateFlow(-1f)
    val sent = MutableStateFlow(0)
    val lastNote = MutableStateFlow("")
    @Volatile var service: LinkService? = null
    // Navigation sources: the real ongoing AMap/Maps notification, and a temporary test override.
    @Volatile var realNav = Nav(); @Volatile var realIcon: ByteArray? = null
    @Volatile var testNav: Nav? = null; @Volatile var testIcon: ByteArray? = null
    val navLog = MutableStateFlow(listOf<String>()); val navDump = MutableStateFlow("")
    fun log(line: String) { val t = java.text.SimpleDateFormat("HH:mm:ss", java.util.Locale.US).format(java.util.Date())
        navLog.value = (listOf("$t  $line") + navLog.value).take(12) }
    fun curNav(): Nav = testNav ?: realNav
    fun curIcon(): ByteArray? = if (testNav != null) testIcon else realIcon
    /** Recompute the effective nav state and push it to the watch. */
    fun applyNav() { nav.value = curNav(); service?.pushNav(curNav(), curIcon()) }
    fun send(f: ByteArray) { service?.ble?.send(f) }
    fun prefs(c: Context) = c.getSharedPreferences("gw", Context.MODE_PRIVATE)
    fun flag(c: Context, k: String, def: Boolean = true) = prefs(c).getBoolean(k, def)
}
