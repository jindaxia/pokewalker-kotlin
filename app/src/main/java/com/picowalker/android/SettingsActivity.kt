package com.picowalker.android

import android.app.Activity
import android.content.Intent
import android.content.SharedPreferences
import android.os.Bundle
import android.os.Handler
import android.os.Looper
import android.widget.Button
import android.widget.EditText
import android.widget.TextView
import android.widget.Toast
import java.io.ByteArrayOutputStream
import java.io.File

class SettingsActivity : Activity() {

    companion object {
        private const val REQ_EXPORT = 1001
        private const val REQ_IMPORT = 1002
    }

    private lateinit var eepromPath: String
    private lateinit var irPrefs: SharedPreferences
    private lateinit var irStatusText: TextView
    private lateinit var irHost: EditText
    private lateinit var irPort: EditText
    private lateinit var irToggle: Button
    private lateinit var stepStatus: TextView

    private val handler = Handler(Looper.getMainLooper())
    private var irRunning = false

    private val statusTicker = object : Runnable {
        override fun run() {
            updateIrStatus()
            if (irRunning) handler.postDelayed(this, 700)
        }
    }

    override fun onCreate(savedInstanceState: Bundle?) {
        super.onCreate(savedInstanceState)
        setContentView(R.layout.activity_settings)
        title = "Picowalker 设置"

        eepromPath = (application as PwApp).eepromPath
        irPrefs = getSharedPreferences("pw_ir", MODE_PRIVATE)

        stepStatus = findViewById(R.id.step_status)
        irStatusText = findViewById(R.id.ir_status)
        irHost = findViewById(R.id.ir_host)
        irPort = findViewById(R.id.ir_port)
        irToggle = findViewById(R.id.ir_toggle)

        stepStatus.text = StepSensor.statusText(this)

        // ---- manual steps ----
        findViewById<Button>(R.id.btn_steps_10).setOnClickListener { WalkerCore.addSteps(10) }
        findViewById<Button>(R.id.btn_steps_100).setOnClickListener { WalkerCore.addSteps(100) }
        findViewById<Button>(R.id.btn_steps_1000).setOnClickListener { WalkerCore.addSteps(1000) }
        findViewById<Button>(R.id.btn_steps_custom).setOnClickListener {
            val input = findViewById<EditText>(R.id.step_custom).text.toString().toIntOrNull()
            if (input == null || input <= 0) {
                Toast.makeText(this, "请输入有效的步数", Toast.LENGTH_SHORT).show()
            } else {
                WalkerCore.addSteps(minOf(input, 999999))
            }
        }

        // ---- save import / export ----
        findViewById<TextView>(R.id.eeprom_path).text = eepromPath
        findViewById<Button>(R.id.btn_export).setOnClickListener { exportEeprom() }
        findViewById<Button>(R.id.btn_import).setOnClickListener { importEeprom() }

        // ---- IR bridge ----
        irHost.setText(irPrefs.getString("host", ""))
        irPort.setText(irPrefs.getInt("port", 9876).toString())
        irToggle.setOnClickListener { toggleIr() }
        updateIrToggleLabel()
    }

    override fun onResume() {
        super.onResume()
        stepStatus.text = StepSensor.statusText(this)
        irRunning = true
        handler.post(statusTicker)
    }

    override fun onPause() {
        irRunning = false
        handler.removeCallbacks(statusTicker)
        super.onPause()
    }

    // ------------------------------------------------------------------ IR

    private fun toggleIr() {
        if (irPrefs.getBoolean("enabled", false)) {
            irPrefs.edit().putBoolean("enabled", false).apply()
            WalkerCore.setIrConfig("", 0, false)
            Toast.makeText(this, "IR 桥接已关闭", Toast.LENGTH_SHORT).show()
        } else {
            val host = irHost.text.toString().trim()
            val port = irPort.text.toString().toIntOrNull()
            if (host.isEmpty() || port == null || port !in 1..65535) {
                Toast.makeText(this, "请填写有效的主机和端口", Toast.LENGTH_SHORT).show()
                return
            }
            irPrefs.edit()
                .putString("host", host)
                .putInt("port", port)
                .putBoolean("enabled", true)
                .apply()
            WalkerCore.setIrConfig(host, port, true)
            Toast.makeText(this, "IR 桥接已启用", Toast.LENGTH_SHORT).show()
        }
        updateIrToggleLabel()
        updateIrStatus()
    }

    private fun updateIrToggleLabel() {
        irToggle.text = if (irPrefs.getBoolean("enabled", false)) "关闭 IR 桥接" else "启用 IR 桥接"
    }

    private fun updateIrStatus() {
        val enabled = irPrefs.getBoolean("enabled", false)
        val text = when {
            !enabled -> "状态：未启用（无法与游戏/实体计步器通讯）"
            else -> when (WalkerCore.irStatus()) {
                2 -> "状态：已连接 ${irPrefs.getString("host", "")}:${irPrefs.getInt("port", 0)}"
                1 -> "状态：已配置，等待进入连接界面后建立连接"
                else -> "状态：未启用"
            }
        }
        if (irStatusText.text != text) irStatusText.text = text
    }

    // -------------------------------------------------------------- EEPROM

    private fun exportEeprom() {
        val intent = Intent(Intent.ACTION_CREATE_DOCUMENT).apply {
            addCategory(Intent.CATEGORY_OPENABLE)
            type = "application/octet-stream"
            putExtra(Intent.EXTRA_TITLE, "eeprom.bin")
        }
        startActivityForResult(intent, REQ_EXPORT)
    }

    private fun importEeprom() {
        val intent = Intent(Intent.ACTION_OPEN_DOCUMENT).apply {
            addCategory(Intent.CATEGORY_OPENABLE)
            type = "*/*"
        }
        startActivityForResult(intent, REQ_IMPORT)
    }

    override fun onActivityResult(requestCode: Int, resultCode: Int, data: Intent?) {
        super.onActivityResult(requestCode, resultCode, data)
        if (resultCode != RESULT_OK || data?.data == null) return

        when (requestCode) {
            REQ_EXPORT -> {
                try {
                    val out = contentResolver.openOutputStream(data.data!!)
                    out.use { stream ->
                        if (stream == null) throw IllegalStateException("null stream")
                        File(eepromPath).inputStream().use { it.copyTo(stream) }
                    }
                    Toast.makeText(this, "存档已导出", Toast.LENGTH_SHORT).show()
                } catch (e: Exception) {
                    Toast.makeText(this, "导出失败: ${e.message}", Toast.LENGTH_SHORT).show()
                }
            }
            REQ_IMPORT -> {
                try {
                    val bytes = contentResolver.openInputStream(data.data!!)?.use { input ->
                        val out = ByteArrayOutputStream()
                        input.copyTo(out)
                        out.toByteArray()
                    }
                    if (bytes == null || bytes.size != WalkerCore.EEPROM_SIZE) {
                        Toast.makeText(
                            this,
                            "存档大小无效（需要 ${WalkerCore.EEPROM_SIZE} 字节），实际 ${bytes?.size ?: 0}",
                            Toast.LENGTH_LONG
                        ).show()
                        return
                    }
                    if (WalkerCore.importEeprom(eepromPath, bytes)) {
                        Toast.makeText(this, "存档已导入，核心已重启", Toast.LENGTH_SHORT).show()
                    }
                } catch (e: Exception) {
                    Toast.makeText(this, "导入失败: ${e.message}", Toast.LENGTH_SHORT).show()
                }
            }
        }
    }
}
