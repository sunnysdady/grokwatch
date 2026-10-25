package io.grokwatch.app

import android.annotation.SuppressLint
import android.app.*
import android.content.Context
import android.content.Intent
import android.content.pm.ServiceInfo
import android.location.Location
import android.location.LocationListener
import android.location.LocationManager
import android.os.BatteryManager
import android.os.Handler
import android.os.Looper
import java.util.TimeZone

/** Foreground service: keeps the BLE link, 5 s heartbeat, GPS speed, and replays nav state on reconnect. */
class LinkService : Service() {
    var ble: BleLink? = null
    private val h = Handler(Looper.getMainLooper())
    private var lm: LocationManager? = null
    private var lastIcon: ByteArray? = null

    override fun onBind(i: Intent?) = null
    override fun onCreate() {
        super.onCreate(); Hub.service = this
        val nm = getSystemService(NotificationManager::class.java)
        nm.createNotificationChannel(NotificationChannel("link", "手表连接", NotificationManager.IMPORTANCE_MIN))
        val pi = PendingIntent.getActivity(this, 0, Intent(this, MainActivity::class.java), PendingIntent.FLAG_IMMUTABLE)
        val n = Notification.Builder(this, "link").setSmallIcon(R.drawable.ic_launcher).setContentTitle("GrokWatch 正在连接手表")
            .setContentIntent(pi).setOngoing(true).build()
        val type = ServiceInfo.FOREGROUND_SERVICE_TYPE_CONNECTED_DEVICE or
            (if (hasLoc()) ServiceInfo.FOREGROUND_SERVICE_TYPE_LOCATION else 0)
        startForeground(1, n, type)
        ble = BleLink(this) { onLinkReady() }.also { it.start() }
        h.post(beat); startGps()
    }
    override fun onStartCommand(i: Intent?, f: Int, id: Int): Int { if (i?.action == "forget") ble?.forget(); if (lm == null) startGps(); return START_STICKY }
    override fun onDestroy() { h.removeCallbacksAndMessages(null); ble?.stop(); stopGps(); Hub.service = null; super.onDestroy() }

    private fun hasLoc() = checkSelfPermission(android.Manifest.permission.ACCESS_FINE_LOCATION) == android.content.pm.PackageManager.PERMISSION_GRANTED
    private val beat = object : Runnable { override fun run() {
        val b = (getSystemService(Context.BATTERY_SERVICE) as BatteryManager).getIntProperty(BatteryManager.BATTERY_PROPERTY_CAPACITY)
        Hub.send(Proto.hello(b, if (Hub.curNav().active) 1 else 0))
        pushNav(Hub.curNav(), Hub.curIcon(), ++beats % 6 == 0)   // resend nav every 5 s, icon every 30 s
        h.postDelayed(this, 5000) } }
    private var beats = 0

    private fun onLinkReady() {
        val now = System.currentTimeMillis()
        Hub.send(Proto.time(now / 1000, TimeZone.getDefault().getOffset(now) / 60000))
        Hub.send(Proto.hello(50, 0)); pushNav(Hub.curNav(), Hub.curIcon(), true)
    }
    fun pushNav(n: Nav, icon: ByteArray?, force: Boolean = false) {
        if (!n.active) { Hub.send(Proto.navState(false)); lastIcon = null; return }
        Hub.send(Proto.navState(true)); Hub.send(Proto.navData(n.dist, n.road, n.dur, n.eta, if (n.light.isNotEmpty()) "L=${n.light}" else ""))
        if (icon != null && (force || !icon.contentEquals(lastIcon))) { for (p in 0..2) Hub.send(Proto.navIcon(p, icon)) }
        lastIcon = icon
    }

    private val gps = LocationListener { l: Location ->
        if (!Hub.flag(this, "speed")) return@LocationListener
        val kmh = if (l.hasSpeed()) l.speed * 3.6f else 0f
        Hub.speed.value = kmh; Hub.send(Proto.speed((kmh * 10).toInt().coerceIn(0, 2000)))
    }
    @SuppressLint("MissingPermission")
    private fun startGps() { if (!hasLoc()) return; val m = getSystemService(LocationManager::class.java)
        try { m.requestLocationUpdates(LocationManager.GPS_PROVIDER, 1000L, 0f, gps, Looper.getMainLooper()); lm = m } catch (_: Exception) {} }
    private fun stopGps() { lm?.removeUpdates(gps) }

    companion object {
        fun start(c: Context) { c.startForegroundService(Intent(c, LinkService::class.java)) }
        fun forget(c: Context) { c.startForegroundService(Intent(c, LinkService::class.java).setAction("forget")) }
    }
}
