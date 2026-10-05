package com.picowalker.android

import android.app.Application
import android.content.BroadcastReceiver
import android.content.Context
import android.content.Intent
import android.content.IntentFilter
import android.content.pm.PackageManager
import android.os.Build
import android.os.BatteryManager
import java.io.File

/**
 * Owns the process-wide walker core: it starts once and keeps running for
 * the lifetime of the app process.
 */
class PwApp : Application() {

    lateinit var eepromPath: String
        private set

    private val batteryReceiver = object : BroadcastReceiver() {
        override fun onReceive(context: Context, intent: Intent) {
            val level = intent.getIntExtra(BatteryManager.EXTRA_LEVEL, -1)
            val scale = intent.getIntExtra(BatteryManager.EXTRA_SCALE, -1)
            if (level < 0 || scale <= 0) return

            val status = intent.getIntExtra(
                BatteryManager.EXTRA_STATUS,
                BatteryManager.BATTERY_STATUS_UNKNOWN
            )
            val plugged = intent.getIntExtra(BatteryManager.EXTRA_PLUGGED, 0)
            val percent = (level * 100f / scale).toInt().coerceIn(0, 100)
            val charging =
                status == BatteryManager.BATTERY_STATUS_CHARGING ||
                    status == BatteryManager.BATTERY_STATUS_FULL

            WalkerCore.setBattery(percent, charging, plugged != 0)
        }
    }

    override fun onCreate() {
        super.onCreate()

        eepromPath = File(filesDir, "eeprom.bin").absolutePath
        WalkerCore.start(eepromPath)

        val filter = IntentFilter(Intent.ACTION_BATTERY_CHANGED)
        if (Build.VERSION.SDK_INT >= 33) {
            registerReceiver(batteryReceiver, filter, Context.RECEIVER_NOT_EXPORTED)
        } else {
            registerReceiver(batteryReceiver, filter)
        }

        maybeStartStepSensor()

        val irPrefs = getSharedPreferences("pw_ir", MODE_PRIVATE)
        if (irPrefs.getBoolean("enabled", false)) {
            WalkerCore.setIrConfig(
                irPrefs.getString("host", "") ?: "",
                irPrefs.getInt("port", 0),
                true
            )
        }
    }

    /** Called again after the ACTIVITY_RECOGNITION permission is granted. */
    fun maybeStartStepSensor(): Int {
        if (!hasActivityRecognitionPermission()) {
            return StepSensor.MODE_NONE
        }
        return StepSensor.register(this)
    }

    private fun hasActivityRecognitionPermission(): Boolean {
        if (Build.VERSION.SDK_INT < 29) return true
        return checkSelfPermission(android.Manifest.permission.ACTIVITY_RECOGNITION) ==
            PackageManager.PERMISSION_GRANTED
    }
}
