package com.picowalker.android

import android.content.Context
import android.content.SharedPreferences
import android.hardware.Sensor
import android.hardware.SensorEvent
import android.hardware.SensorEventListener
import android.hardware.SensorManager

/**
 * Feeds the core's step counter.
 *
 * Prefers TYPE_STEP_DETECTOR (one event per step, instant feedback for the
 * walking animation) and falls back to TYPE_STEP_COUNTER (cumulative,
 * batched). If the device has neither, only manual step entry is available.
 */
object StepSensor : SensorEventListener {

    const val MODE_NONE = 0
    const val MODE_DETECTOR = 1
    const val MODE_COUNTER = 2

    private const val PREFS = "pw_steps"
    private const val KEY_LAST_COUNT = "last_step_count"

    private var sensorManager: SensorManager? = null
    private var appContext: Context? = null
    private var lastCount = -1

    var mode: Int = MODE_NONE
        private set

    val isRegistered: Boolean
        get() = mode != MODE_NONE

    fun register(context: Context): Int {
        if (mode != MODE_NONE) return mode

        val ctx = context.applicationContext
        appContext = ctx
        val sm = ctx.getSystemService(Context.SENSOR_SERVICE) as? SensorManager
        sensorManager = sm
        if (sm == null) {
            mode = MODE_NONE
            return mode
        }

        val detector = sm.getDefaultSensor(Sensor.TYPE_STEP_DETECTOR)
        val counter = sm.getDefaultSensor(Sensor.TYPE_STEP_COUNTER)
        val sensor = detector ?: counter

        if (sensor == null) {
            mode = MODE_NONE
            return mode
        }

        lastCount = prefs(ctx).getInt(KEY_LAST_COUNT, -1)
        mode = if (sensor.type == Sensor.TYPE_STEP_DETECTOR) MODE_DETECTOR else MODE_COUNTER
        sm.registerListener(this, sensor, SensorManager.SENSOR_DELAY_NORMAL)
        return mode
    }

    fun statusText(context: Context): String {
        return when (mode) {
            MODE_DETECTOR -> "系统计步器（每步事件）已启用"
            MODE_COUNTER -> "系统计步器（累计计数）已启用"
            else -> "此设备不支持系统计步，仅手动加步可用"
        }
    }

    override fun onSensorChanged(event: SensorEvent) {
        when (mode) {
            MODE_DETECTOR -> {
                WalkerCore.addSteps(1)
            }
            MODE_COUNTER -> {
                val total = event.values[0].toInt()
                if (lastCount < 0) {
                    lastCount = total
                } else {
                    var delta = total - lastCount
                    if (delta < 0) {
                        // device rebooted since last run, counter restarted
                        delta = total
                    }
                    WalkerCore.addSteps(delta)
                    lastCount = total
                }
                appContext?.let { prefs(it).edit().putInt(KEY_LAST_COUNT, lastCount).apply() }
            }
        }
    }

    override fun onAccuracyChanged(sensor: Sensor?, accuracy: Int) {}

    private fun prefs(context: Context): SharedPreferences =
        context.getSharedPreferences(PREFS, Context.MODE_PRIVATE)
}
