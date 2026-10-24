package io.grokwatch.app

import android.annotation.SuppressLint
import android.bluetooth.*
import android.bluetooth.le.*
import android.content.Context
import android.os.Handler
import android.os.Looper
import android.os.ParcelUuid
import android.util.Log
import java.util.ArrayDeque

/** BLE central: scan by service UUID, connect, MTU, CCCD, serialized write queue, auto-reconnect with backoff. */
@SuppressLint("MissingPermission")
class BleLink(private val ctx: Context, private val onReady: () -> Unit) {
    private val h = Handler(Looper.getMainLooper())
    private val mgr = ctx.getSystemService(BluetoothManager::class.java)
    private var gatt: BluetoothGatt? = null
    private var rx: BluetoothGattCharacteristic? = null
    private val q = ArrayDeque<ByteArray>(); private var busy = false
    private var backoff = 2000L; private var stopped = false

    fun start() { stopped = false; val saved = Hub.prefs(ctx).getString("addr", null)
        if (saved != null) connect(mgr.adapter.getRemoteDevice(saved)) else scan() }
    fun stop() { stopped = true; h.removeCallbacksAndMessages(null); stopScan(); gatt?.close(); gatt = null; Hub.link.value = Link.OFF }
    fun forget() { Hub.prefs(ctx).edit().remove("addr").apply(); gatt?.close(); gatt = null; scan() }

    private val scanCb = object : ScanCallback() {
        override fun onScanResult(t: Int, r: ScanResult) { stopScan(); Hub.prefs(ctx).edit().putString("addr", r.device.address).apply(); connect(r.device) }
    }
    private fun scan() {
        val s = mgr.adapter?.bluetoothLeScanner ?: return retry()
        Hub.link.value = Link.SCANNING
        s.startScan(listOf(ScanFilter.Builder().setServiceUuid(ParcelUuid(Proto.SVC)).build()),
            ScanSettings.Builder().setScanMode(ScanSettings.SCAN_MODE_LOW_LATENCY).build(), scanCb)
        h.postDelayed({ if (Hub.link.value == Link.SCANNING) { stopScan(); retry() } }, 15000)
    }
    private fun stopScan() { try { mgr.adapter?.bluetoothLeScanner?.stopScan(scanCb) } catch (_: Exception) {} }
    private fun retry() { if (stopped) return; Hub.link.value = Link.OFF
        h.postDelayed({ start() }, backoff); backoff = (backoff * 2).coerceAtMost(30000) }

    private fun connect(d: BluetoothDevice) {
        Hub.link.value = Link.CONNECTING; gatt?.close()
        gatt = d.connectGatt(ctx, false, cb, BluetoothDevice.TRANSPORT_LE)
        h.postDelayed({ if (Hub.link.value == Link.CONNECTING) { gatt?.close(); gatt = null; retry() } }, 12000)
    }
    private val cb = object : BluetoothGattCallback() {
        override fun onConnectionStateChange(g: BluetoothGatt, status: Int, st: Int) {
            if (st == BluetoothProfile.STATE_CONNECTED) { g.requestConnectionPriority(BluetoothGatt.CONNECTION_PRIORITY_BALANCED); g.requestMtu(247) }
            else { Log.i("GW", "disconnected $status"); rx = null; synchronized(q) { q.clear(); busy = false }; g.close(); if (gatt == g) gatt = null; retry() }
        }
        override fun onMtuChanged(g: BluetoothGatt, mtu: Int, status: Int) { g.discoverServices() }
        override fun onServicesDiscovered(g: BluetoothGatt, status: Int) {
            val s = g.getService(Proto.SVC); rx = s?.getCharacteristic(Proto.RX)
            if (rx == null) { g.disconnect(); return }
            s.getCharacteristic(Proto.TX)?.let { tx -> g.setCharacteristicNotification(tx, true)
                tx.getDescriptor(Proto.CCCD)?.let { g.writeDescriptor(it, BluetoothGattDescriptor.ENABLE_NOTIFICATION_VALUE) } }
            backoff = 2000L; Hub.device.value = g.device.name ?: g.device.address
            h.postDelayed({ Hub.link.value = Link.CONNECTED; onReady() }, 300)
        }
        override fun onDescriptorWrite(g: BluetoothGatt, d: BluetoothGattDescriptor, status: Int) { pump(true) }
        override fun onCharacteristicWrite(g: BluetoothGatt, c: BluetoothGattCharacteristic, status: Int) { pump(true) }
    }
    fun send(f: ByteArray) { if (Hub.link.value != Link.CONNECTED) return
        synchronized(q) { if (q.size > 40) q.poll(); q.add(f) }; h.post { pump(false) } }
    private fun pump(done: Boolean) {
        val g = gatt ?: return; val c = rx ?: return
        val f: ByteArray
        synchronized(q) { if (done) busy = false; if (busy) return; f = q.poll() ?: return; busy = true }
        val rc = g.writeCharacteristic(c, f, BluetoothGattCharacteristic.WRITE_TYPE_DEFAULT)
        if (rc != BluetoothStatusCodes.SUCCESS) { synchronized(q) { busy = false; q.addFirst(f) }; h.postDelayed({ pump(false) }, 60) }
        else Hub.sent.value++
    }
}
