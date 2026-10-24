package io.grokwatch.app

import android.app.PendingIntent
import android.content.BroadcastReceiver
import android.content.Context
import android.content.Intent
import android.content.pm.PackageInstaller
import kotlinx.coroutines.Dispatchers
import kotlinx.coroutines.flow.MutableStateFlow
import kotlinx.coroutines.withContext
import org.json.JSONObject
import java.io.File
import java.net.HttpURLConnection
import java.net.URL
import java.security.MessageDigest

/**
 * In-app self-update. Manifest JSON (any static host: LAN server now, GitHub Releases later):
 * {"versionCode":62,"versionName":"0.6.2","url":"https://…/GrokWatch-0.6.2.apk","sha256":"…","notes":"…"}
 * Relative "url" is resolved against the manifest URL. APK must be signed with the same release key.
 */
object Updater {
    data class Info(val code: Int, val name: String, val url: String, val sha: String, val notes: String)
    val status = MutableStateFlow("")
    fun url(c: Context) = Hub.prefs(c).getString("update_url", BuildConfig.UPDATE_URL) ?: ""
    fun setUrl(c: Context, u: String) = Hub.prefs(c).edit().putString("update_url", u.trim()).apply()
    private fun http(u: String): HttpURLConnection = (URL(u).openConnection() as HttpURLConnection).apply { connectTimeout = 8000; readTimeout = 20000 }

    suspend fun check(c: Context): Info? = withContext(Dispatchers.IO) {
        val mu = url(c); if (mu.isBlank()) { status.value = "未设置更新地址"; return@withContext null }
        try {
            val j = JSONObject(http(mu).inputStream.bufferedReader().readText())
            val i = Info(j.getInt("versionCode"), j.optString("versionName"), URL(URL(mu), j.getString("url")).toString(), j.optString("sha256").lowercase(), j.optString("notes"))
            val cur = c.packageManager.getPackageInfo(c.packageName, 0).longVersionCode
            status.value = if (i.code > cur) "发现新版本 ${i.name}" else "已是最新版本"
            if (i.code > cur) i else null
        } catch (t: Throwable) { status.value = "检查失败：${t.javaClass.simpleName}"; null }
    }

    suspend fun install(c: Context, i: Info) = withContext(Dispatchers.IO) {
        try {
            status.value = "下载中…"
            val f = File(c.cacheDir, "update.apk"); http(i.url).inputStream.use { inp -> f.outputStream().use { inp.copyTo(it) } }
            val sha = MessageDigest.getInstance("SHA-256").digest(f.readBytes()).joinToString("") { "%02x".format(it) }
            if (i.sha.isNotEmpty() && sha != i.sha) { status.value = "校验失败，已取消"; f.delete(); return@withContext }
            val pi = c.packageManager.packageInstaller
            val params = PackageInstaller.SessionParams(PackageInstaller.SessionParams.MODE_FULL_INSTALL).apply { setAppPackageName(c.packageName)
                setRequireUserAction(PackageInstaller.SessionParams.USER_ACTION_NOT_REQUIRED) }
            val id = pi.createSession(params)
            pi.openSession(id).use { s ->
                s.openWrite("base.apk", 0, f.length()).use { o -> f.inputStream().use { it.copyTo(o) }; s.fsync(o) }
                val cb = PendingIntent.getBroadcast(c, id, Intent(c, InstallReceiver::class.java), PendingIntent.FLAG_MUTABLE or PendingIntent.FLAG_UPDATE_CURRENT)
                s.commit(cb.intentSender)
            }
            status.value = "正在安装…"
        } catch (t: Throwable) { status.value = "更新失败：${t.javaClass.simpleName}" }
    }

    class InstallReceiver : BroadcastReceiver() {
        override fun onReceive(c: Context, i: Intent) {
            when (i.getIntExtra(PackageInstaller.EXTRA_STATUS, -999)) {
                PackageInstaller.STATUS_PENDING_USER_ACTION -> {
                    @Suppress("DEPRECATION") val confirm = i.getParcelableExtra<Intent>(Intent.EXTRA_INTENT)
                    confirm?.addFlags(Intent.FLAG_ACTIVITY_NEW_TASK)?.let { c.startActivity(it) } }
                PackageInstaller.STATUS_SUCCESS -> status.value = "更新完成"
                else -> status.value = "安装未完成：${i.getStringExtra(PackageInstaller.EXTRA_STATUS_MESSAGE) ?: ""}"
            }
        }
    }
}
