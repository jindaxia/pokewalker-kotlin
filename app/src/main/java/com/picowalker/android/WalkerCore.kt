package com.picowalker.android

import android.os.Handler
import android.os.Looper
import java.io.File

/**
 * Bridge to the native picowalker core (libpicowalker.so).
 *
 * The core runs on its own thread; frames come back through [onNativeFrame]
 * and are forwarded to the registered sink on the main thread.
 */
object WalkerCore {
    const val WIDTH = 96
    const val HEIGHT = 64
    const val EEPROM_SIZE = 64 * 1024

    const val BTN_L = 0x01
    const val BTN_M = 0x02
    const val BTN_R = 0x04

    private val pixels = IntArray(WIDTH * HEIGHT)
    private val mainHandler = Handler(Looper.getMainLooper())

    @Volatile
    private var frameSink: ((IntArray) -> Unit)? = null

    @Volatile
    private var frameReady = false

    @Volatile
    private var started = false

    init {
        System.loadLibrary("picowalker")
    }

    fun setFrameSink(sink: ((IntArray) -> Unit)?) {
        frameSink = sink
        // The core may already have pushed a frame before the activity
        // registered; replay the current buffer so the screen is not blank.
        if (sink != null && frameReady) {
            mainHandler.post { sink(pixels) }
        }
    }

    @Synchronized
    fun start(eepromPath: String) {
        nativeStart(eepromPath, pixels)
        started = true
    }

    @Synchronized
    fun stop() {
        if (!started) return
        nativeStop()
        started = false
    }

    /** Stop the core, replace the save file, then start again. */
    @Synchronized
    fun importEeprom(eepromPath: String, data: ByteArray): Boolean {
        if (data.size != EEPROM_SIZE) return false
        stop()
        File(eepromPath).writeBytes(data)
        start(eepromPath)
        return true
    }

    fun press(mask: Int, down: Boolean) {
        nativeButton(mask, down)
    }

    fun addSteps(steps: Int) {
        if (steps > 0) nativeAddSteps(steps)
    }

    fun setBattery(percent: Int, charging: Boolean, plugged: Boolean) {
        nativeSetBattery(percent, charging, plugged)
    }

    fun setIrConfig(host: String, port: Int, enabled: Boolean) {
        nativeSetIrConfig(host, port, enabled)
    }

    fun irStatus(): Int = nativeIrStatus()

    /** Called from the native loop thread. */
    @JvmStatic
    fun onNativeFrame(p: IntArray) {
        frameReady = true
        mainHandler.post {
            frameSink?.invoke(p)
        }
    }

    private external fun nativeStart(path: String, pixels: IntArray)
    private external fun nativeStop()
    private external fun nativeButton(mask: Int, pressed: Boolean)
    private external fun nativeAddSteps(steps: Int)
    private external fun nativeSetBattery(percent: Int, charging: Boolean, plugged: Boolean)
    private external fun nativeSetIrConfig(host: String, port: Int, enabled: Boolean)
    private external fun nativeIrStatus(): Int
}
