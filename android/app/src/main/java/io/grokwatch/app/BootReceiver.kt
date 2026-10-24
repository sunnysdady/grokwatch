package io.grokwatch.app
import android.content.BroadcastReceiver
import android.content.Context
import android.content.Intent
class BootReceiver : BroadcastReceiver() { override fun onReceive(c: Context, i: Intent) { try { LinkService.start(c) } catch (_: Exception) {} } }
