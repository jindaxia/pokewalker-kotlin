# Picowalker for Android

[HGSS Pokewalker](https://bulbapedia.bulbagarden.net/wiki/Pok%C3%A9walker) 复刻的 Android 移植版，基于开源项目 [picowalker](https://github.com/schmatzler/picowalker) 与 [picowalker-core](https://github.com/schmatzler/picowalker-core)。

核心逻辑（状态机、步数结算、密码计算、2bpp 屏幕缓冲）全部由原生 C 代码（NDK）驱动，与硬件版固件行为一致；Kotlin 层只负责 UI、输入与传感器。

## 功能

- 96×64 / 4 阶灰度屏幕，最近邻缩放显示
- L / M / R 三键（移动光标 / 确认）
- 计步：优先使用系统 `TYPE_STEP_DETECTOR`，回退 `TYPE_STEP_COUNTER`，另支持手动加步
- 存档：64KB EEPROM，原子写入应用私有目录，支持 SAF 导出 / 导入（与硬件版固件互通）
- 电池状态上报（充电 / 拔插事件 + 15s 电量测量）
- IR 桥接：预留 TCP 接口（手机无红外硬件，需中转服务与游戏 / 实体计步器通讯），在设置页配置

## 构建

### GitHub Actions（推荐）

仓库自带 `.github/workflows/build.yml`，push 后自动构建，产物在 Actions Artifacts 中
（`picowalker-debug-apk`）。

### 本地

需要 JDK 17、Android SDK + NDK 26.3.11579264 + CMake 3.22.1、Gradle 8.9：

```sh
gradle assembleDebug          # 或先 gradle wrapper 生成 wrapper 后 ./gradlew assembleDebug
```

APK 输出：`app/build/outputs/apk/debug/app-debug.apk`

## 结构

```
app/src/main/cpp/
  core/            vendored picowalker-core（平台无关逻辑）
  assets/          ROM 补充资产（flash image / fancy 字体，由脚本生成）
  dr_*.c           驱动：屏幕、EEPROM、时间、按键、计步、电源、Flash、日志、IR(TCP)、AAudio
  runner.c         核心循环线程（4ms tick，每 tick 推一帧到 Java）
  jni_bridge.c     JNI 接口
app/src/main/java/com/picowalker/android/
  PwApp.kt         进程级：启动核心、电池广播、计步注册、IR 恢复
  MainActivity.kt  主界面（屏幕 + 三键 + 设置入口）
  WalkerView.kt    96×64 帧缓冲 → View
  WalkerCore.kt    JNI 外观
  SettingsActivity.kt  手动加步 / 存档导入导出 / IR 配置
  StepSensor.kt    系统计步器接入
```

## 已知限制

- 无红外硬件：IR 功能必须通过 TCP 桥接服务（协议实现见 `dr_ir.c`，需按
  `picowalker` 的 IR 协议自建中转）
- 需要 `ACTIVITY_RECOGNITION` 运行时权限（Android 10+）才能读取系统计步数据
- 屏幕按设备旋转锁定为竖屏

## 许可证

上游 picowalker / picowalker-core 的许可证见其仓库；本移植代码同样遵循。
