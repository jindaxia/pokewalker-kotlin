package com.picowalker.android

import android.Manifest
import android.app.Activity
import android.content.Intent
import android.content.pm.PackageManager
import android.os.Build
import android.os.Bundle
import android.view.MotionEvent
import android.view.View
import android.view.WindowManager
import android.widget.Button

class MainActivity : Activity() {

    companion object {
        private const val REQ_ACTIVITY_RECOGNITION = 1001
    }

    private lateinit var walkerView: WalkerView

    override fun onCreate(savedInstanceState: Bundle?) {
        super.onCreate(savedInstanceState)
        setContentView(R.layout.activity_main)

        window.addFlags(WindowManager.LayoutParams.FLAG_KEEP_SCREEN_ON)

        walkerView = findViewById(R.id.walker_view)
        WalkerCore.setFrameSink { pixels -> walkerView.updateFrame(pixels) }

        bindButton(R.id.btn_l, WalkerCore.BTN_L)
        bindButton(R.id.btn_m, WalkerCore.BTN_M)
        bindButton(R.id.btn_r, WalkerCore.BTN_R)

        findViewById<Button>(R.id.btn_settings).setOnClickListener {
            startActivity(Intent(this, SettingsActivity::class.java))
        }

        requestStepPermission()
    }

    override fun onResume() {
        super.onResume()
        (application as PwApp).maybeStartStepSensor()
    }

    override fun onDestroy() {
        if (isFinishing) {
            WalkerCore.setFrameSink(null)
        }
        super.onDestroy()
    }

    private fun bindButton(viewId: Int, mask: Int) {
        val button = findViewById<View>(viewId)
        button.setOnTouchListener { _, event ->
            when (event.actionMasked) {
                MotionEvent.ACTION_DOWN -> {
                    WalkerCore.press(mask, true)
                    button.isPressed = true
                    true
                }
                MotionEvent.ACTION_UP, MotionEvent.ACTION_CANCEL -> {
                    WalkerCore.press(mask, false)
                    button.isPressed = false
                    true
                }
                else -> false
            }
        }
    }

    private fun requestStepPermission() {
        if (Build.VERSION.SDK_INT < 29) return
        val granted = checkSelfPermission(Manifest.permission.ACTIVITY_RECOGNITION) ==
            PackageManager.PERMISSION_GRANTED
        if (!granted) {
            requestPermissions(arrayOf(Manifest.permission.ACTIVITY_RECOGNITION), REQ_ACTIVITY_RECOGNITION)
        }
    }

    override fun onRequestPermissionsResult(
        requestCode: Int,
        permissions: Array<out String>,
        grantResults: IntArray
    ) {
        super.onRequestPermissionsResult(requestCode, permissions, grantResults)
        if (requestCode == REQ_ACTIVITY_RECOGNITION &&
            grantResults.isNotEmpty() &&
            grantResults[0] == PackageManager.PERMISSION_GRANTED
        ) {
            (application as PwApp).maybeStartStepSensor()
        }
    }
}
