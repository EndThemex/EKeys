# 09 — BLE 蓝牙键盘（与 USB 串口共存）

> 蓝牙 HID 键盘模式：保留 USB-Serial 日志，同时通过 BLE 上报按键。
>
> 日期：2026-09-01

---

## 1. 设计目标

- 设备作为 **BLE HID 键盘** 被手机/平板/电脑识别并接收按键；
- 同时，**USB 串口继续可用**——打日志、调试、烧录均不受影响；
- 不做 USB HID（避免与 BLE 双栈增加复杂度）。

## 2. 硬件无改动

ESP32-S3 的 USB-Serial/JTAG 与 BLE 走的是**两个独立的物理层**：

| 通道 | 物理层 | 占用 GPIO |
|---|---|---|
| USB-Serial | USB 2.0（D+/D-） | 否，内部 USB PHY |
| BLE | 2.4 GHz 射频 | 否，内部天线 |

不冲突、不需要额外飞线、不需要改 [07-pinout.md](./07-pinout.md)。

## 3. 代码结构

```
src/ble/
├── BleKeyboardSink.h
└── BleKeyboardSink.cpp   ← 唯一与 BLE 库耦合的地方
```

`BleKeyboardSink` 是薄封装：

- `begin()` —— 启动 BLE 广播（设备名 `EKeys`）；
- `pressKey(keyId)` / `releaseKey(keyId)` —— 物理键 1..9 → HID；
- `encoderClick(kind)` —— 单击=Enter / 双击=Esc / 长按=Tab；
- `isConnected()` —— 查询是否已配对，未连时所有发送自动跳过。

## 4. 默认键位映射

| 物理键 | HID | 用途 |
|---|---|---|
| KEY1 | `0x1E` | 数字 `1` |
| KEY2 | `0x1F` | 数字 `2` |
| KEY3 | `0x20` | 数字 `3` |
| KEY4 | `0x21` | 数字 `4` |
| KEY5 | `0x22` | 数字 `5` |
| KEY6 | `0x23` | 数字 `6` |
| KEY7 | `0x24` | 数字 `7` |
| KEY8 | `0x25` | 数字 `8` |
| KEY9 | `0x26` | 数字 `9` |
| 旋钮单击 | `KEY_RETURN` | 回车 |
| 旋钮双击 | `KEY_ESC` | 取消 |
| 旋钮长按 | `KEY_TAB` | 切换 |

如需改为多媒体键（音量/播放）或快捷键（Ctrl+C），改 `BleKeyboardSink.cpp` 的 `keyIdToHid()` 与 `encoderClick()`。

## 5. 主循环接入点

在 [main.cpp](../../main.cpp) `loop()` 中：

```cpp
// 按下 → 走 BLE + UI
g_pm.handleKeyPress(pressed);
g_bleKbd.pressKey(pressed);

// 释放 → BLE release
if (released != 0) g_bleKbd.releaseKey(released);

// 旋钮事件 → BLE Enter/Esc/Tab
g_bleKbd.encoderClick(...);
g_bleKbd.encoderRelease();
```

UI 路由（`g_pm.handleEncoderRotate`）与 BLE 上报**互不干扰**，便于独立演进。

## 6. ⛔ 不要做的事

| 错误 | 后果 |
|---|---|
| 在 ISR 中调用 `g_ble.press()` | BLE 库内部用 mutex，ISR 内阻塞会死锁 |
| 改用 USB HID 双栈 | 要重新分配 USB CDC 资源，与 USB-Serial 冲突 |
| 把 BLE 库实例放成多份 | NimBLE 全局只有一套，多实例会触发 `NimBLEDevice::deinit` 重复 |

## 7. 验证步骤

1. **编译**：`pio run`，首次会拉 `https://github.com/T-vK/ESP32-BLE-Keyboard/archive/refs/tags/v0.3.2.zip`；
2. **烧录**：插 USB，烧录，**不要拔**——USB 串口继续输出日志；
3. **配对**：手机/电脑蓝牙列表里会出现 `EKeys`，点连接；
4. **按键测试**：按 KEY1~KEY9，对端应依次打出 `123456789`；
5. **旋钮测试**：单击 → 回车；双击 → Esc；长按 → Tab；
6. **日志观察**：`Serial.println("[BLE] isConnected=true")` 类日志可手动加；
7. **断开重连**：关闭对端 BLE，串口日志应仍正常；再次连接，无需重启 ESP32。

## 8. 已知限制

- **功耗**：BLE 持续广播约 5 mA，长期续航需要后续接电池；
- **配对数量**：HID 默认允许 1 个 host，多 host 需要改库 `BLEDevice::setSecurityAuth`；
- **串口互斥**：现有 `serialPrintf()` 仍走 FreeRTOS mutex；BLE 库内部有自己 mutex，互不阻塞。
