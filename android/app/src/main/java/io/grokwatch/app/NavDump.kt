package io.grokwatch.app

import android.app.Notification
import android.content.Context
import android.graphics.Bitmap
import android.graphics.Canvas
import android.graphics.drawable.BitmapDrawable
import android.graphics.drawable.Drawable
import android.graphics.drawable.Icon
import android.os.Bundle
import android.os.Parcelable
import android.view.View
import android.view.ViewGroup
import android.widget.FrameLayout
import android.widget.ImageView
import android.widget.RemoteViews
import android.widget.TextView
import java.io.File

/** Exhaustive dump of a navigation notification: all extras + inflated RemoteViews (text + images). */
class NavDump(val texts: List<String>, val images: List<Bitmap>, val dump: String)

object NavDumper {
    fun dump(c: Context, n: Notification, pkg: String): NavDump {
        val sb = StringBuilder(); val texts = mutableListOf<String>(); val imgs = mutableListOf<Bitmap>()
        sb.append("pkg=$pkg cat=${n.category} flags=0x${Integer.toHexString(n.flags)} ch=${n.channelId} style=${n.extras.getString("android.template")}\n")
        sb.append("promoted=${try { n.javaClass.getMethod("hasPromotableCharacteristics").invoke(n) } catch (_: Throwable) { "?" }}\n")
        sb.append("-- extras --\n"); bundle(n.extras, "", sb, texts, imgs, c)
        n.smallIcon?.let { sb.append("smallIcon type=${it.type}\n") }
        for ((name, rv) in listOf("content" to n.contentView, "big" to n.bigContentView, "headsUp" to n.headsUpContentView)) {
            if (rv == null) continue
            sb.append("-- RemoteViews $name pkg=${rv.`package`} layout=${try { c.packageManager.getResourcesForApplication(rv.`package`).getResourceEntryName(rv.layoutId) } catch (_: Throwable) { rv.layoutId }} --\n")
            try { val v = rv.apply(c, FrameLayout(c)); walk(v, 0, sb, texts, imgs, c) } catch (t: Throwable) { sb.append("  inflate failed: ${t.javaClass.simpleName} ${t.message}\n") }
        }
        return NavDump(texts.filter { it.isNotBlank() }.distinct(), imgs, sb.toString())
    }
    private fun bundle(b: Bundle, ind: String, sb: StringBuilder, texts: MutableList<String>, imgs: MutableList<Bitmap>, c: Context) {
        for (k in b.keySet().sorted()) {
            val v = try { @Suppress("DEPRECATION") b.get(k) } catch (t: Throwable) { "<${t.javaClass.simpleName}>" }
            val s = when (v) {
                null -> "null"
                is CharSequence -> { texts += v.toString(); "\"$v\"" }
                is Array<*> -> v.joinToString(prefix = "[", postfix = "]") { e -> if (e is CharSequence) { texts += e.toString(); "\"$e\"" } else if (e is Bundle) "{${e.keySet().joinToString { kk -> "$kk=${try { @Suppress("DEPRECATION") e.get(kk) } catch (_: Throwable) { "?" }}" }}}" else e.toString() }
                is Bundle -> { sb.append("$ind$k = {\n"); bundle(v, "$ind  ", sb, texts, imgs, c); sb.append("$ind}\n"); continue }
                is Bitmap -> { imgs += v; "Bitmap ${v.width}x${v.height}" }
                is Icon -> { draw(v.loadDrawable(c))?.let { imgs += it }; "Icon type=${v.type}" }
                is IntArray -> v.joinToString(prefix = "int[", postfix = "]")
                is LongArray -> v.joinToString(prefix = "long[", postfix = "]")
                is ArrayList<*> -> v.joinToString(prefix = "list[", postfix = "]") { it.toString().take(200) }
                is Parcelable -> "${v.javaClass.name}: ${v.toString().take(300)}"
                else -> "${v.javaClass.simpleName}: ${v.toString().take(300)}"
            }
            sb.append("$ind$k = $s\n")
        }
    }
    private fun walk(v: View, d: Int, sb: StringBuilder, texts: MutableList<String>, imgs: MutableList<Bitmap>, c: Context) {
        val id = try { if (v.id > 0) v.resources.getResourceEntryName(v.id) else "" } catch (_: Throwable) { "#${v.id}" }
        val ind = "  ".repeat(d + 1)
        when (v) {
            is TextView -> { val t = v.text?.toString() ?: ""; if (t.isNotBlank()) { texts += t; sb.append("${ind}Text[$id] \"$t\"\n") } }
            is ImageView -> { val b = draw(v.drawable); if (b != null) { imgs += b; sb.append("${ind}Image[$id] ${b.width}x${b.height}\n") } }
        }
        if (v is ViewGroup) for (i in 0 until v.childCount) walk(v.getChildAt(i), d + 1, sb, texts, imgs, c)
    }
    fun draw(d: Drawable?): Bitmap? = try { when {
        d == null -> null
        d is BitmapDrawable && d.bitmap != null -> d.bitmap
        d.intrinsicWidth in 1..512 -> Bitmap.createBitmap(d.intrinsicWidth, d.intrinsicHeight.coerceIn(1, 512), Bitmap.Config.ARGB_8888).also { val cv = Canvas(it); d.setBounds(0, 0, it.width, it.height); d.draw(cv) }
        else -> null } } catch (_: Throwable) { null }

    /** Rolling ride log on disk (max ~1.5 MB) for sharing. */
    fun append(c: Context, s: String) { try { val f = File(c.filesDir, "navlog.txt"); if (f.length() > 1_500_000) f.writeText(f.readText().takeLast(700_000)); f.appendText(s + "\n\n") } catch (_: Throwable) {} }
    fun read(c: Context): String = try { File(c.filesDir, "navlog.txt").readText() } catch (_: Throwable) { "" }
    fun clear(c: Context) { File(c.filesDir, "navlog.txt").delete() }
}
