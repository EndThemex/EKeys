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

| 通道       | 物理层           | 占用 GPIO        |
| ---------- | ---------------- | ---------------- |
| USB-Serial | USB 2.0（D+/D-） | 否，内部 USB PHY |
| BLE        | 2.4 GHz 射频     | 否，内部天线     |

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

| 物理键   | HID          | 用途     |
| -------- | ------------ | -------- |
| KEY1     | `0x1E`       | 数字 `1` |
| KEY2     | `0x1F`       | 数字 `2` |
| KEY3     | `0x20`       | 数字 `3` |
| KEY4     | `0x21`       | 数字 `4` |
| KEY5     | `0x22`       | 数字 `5` |
| KEY6     | `0x23`       | 数字 `6` |
| KEY7     | `0x24`       | 数字 `7` |
| KEY8     | `0x25`       | 数字 `8` |
| KEY9     | `0x26`       | 数字 `9` |
| 旋钮单击 | `KEY_RETURN` | 回车     |
| 旋钮双击 | `KEY_ESC`    | 取消     |
| 旋钮长按 | `KEY_TAB`    | 切换     |

如需改为多媒体键（音量/播放）或快捷键（Ctrl+C），改 `BleKeyMap.cpp` 的 `kProfiles[]` 数组；详见 §9。

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

| 错误                          | 后果                                                          |
| ----------------------------- | ------------------------------------------------------------- |
| 在 ISR 中调用 `g_ble.press()` | BLE 库内部用 mutex，ISR 内阻塞会死锁                          |
| 改用 USB HID 双栈             | 要重新分配 USB CDC 资源，与 USB-Serial 冲突                   |
| 把 BLE 库实例放成多份         | NimBLE 全局只有一套，多实例会触发 `NimBLEDevice::deinit` 重复 |

## 7. 验证步骤

1. **编译**：`pio run`，首次会拉 `https://github.com/T-vK/ESP32-BLE-Keyboard/archive/refs/tags/v0.3.2.zip`；
2. **烧录**：插 USB，烧录，**不要拔**——USB 串口继续输出日志；
3. **配对**：手机/电脑蓝牙列表里会出现 `EKeys`，点连接；
4. **按键测试**：按 KEY1~KEY9，对端应依次打出 `123456789`；
5. **旋钮测试**：单击 → 回车；双击 → Esc；长按 → Tab；
6. **日志观察**：`Serial.println("[BLE] isConnected=true")` 类日志可手动加；
7. **断开重连**：关闭对端 BLE，串口日志应仍正常；再次连接，无需重启 ESP32。

## 9. KeyMap Profile —— 多套预置键位配置 + 运行时切换

> 日期：2026-09-01 加入；BLE 子页（PAGE_KEYMAP）实现。

### 9.1 设计目标

- 同一台 EKeys 设备在 BLE 模式下可承担**多种使用场景**：演示 PPT 翻页 / 媒体控制 / 编辑器快捷键 / 数字小键盘。
- 用户在 BLE 页 → KEY2 直接进入 `KeyMap` 子页，**旋钮 / KEY2 / KEY3..9 即可查看与改键**。
- 切换的 profile **写入 NVS**（Preferences 命名空间 `ekeys`，键 `ble_profile`），重启仍然保留。

### 9.2 4 套预置 profile（`BleKeyMap.cpp::kProfiles`）

| 索引 | 名称      | KEY1..9 映射                                        | 旋钮单击 / 双击 / 长按 |
| ---- | --------- | --------------------------------------------------- | ---------------------- |
| 0    | `Numpad`  | `1..9` 顶部数字                                     | Enter / Esc / Tab      |
| 1    | `Media`   | Play / Vol+/Vol- / Next/Prev / Esc / Home/PgUp/PgDn | Play / Esc / Tab       |
| 2    | `Editor`  | `c`/`v`/`x`/`z`/`a`/`s`/`f`/`r`/Space               | Enter / Esc / Tab      |
| 3    | `Present` | `c`/`d`/`e`/`f`/`g`/`h`/`i`/`j`/`k`                 | Enter / Esc / Tab      |

旋钮旋转：所有 profile 均为 CW→`Right` / CCW→`Left`。

### 9.3 UI 行为

- BLE 页：hint 改为 `KEY2 KeyMap  KEY3 toggle  KEY1 back`。
  - `KEY2` / 旋钮单击 → 进入 `KeyMap` 子页。
  - `KEY3` → 切换 BLE 总开关（之前由 KEY2 承担）。
- KeyMap 子页：
  - 3×3 矩阵显示 `keyId→当前映射`（短标签：字母/数字/功能键/媒体键）。
  - 旋钮：循环选中按键（9 个）。
  - `KEY2` / 旋钮单击：切到下一个 profile（循环）。
  - `KEY3..9`：直接跳到对应 keyId。

### 9.4 实现要点

- `BleKeyMap.h` 暴露 `bleActiveProfile() / bleSetActiveProfile(idx) / bleProfile(idx)` 与一个 `refreshMapsFromActiveProfile()`：后者把当前 profile 内容复制到 BleKeyboardSink 仍在用的全局数组 `BLE_KEY_MAP[1..9]`、`BLE_ENCODER_MAP[1..3]`、`BLE_ROTATE_MAP[1..2]`，**sink 内部无改动**，保持最小侵入。
- `BleKeyboardSink::setActiveProfile()` 除了调上面那个同步函数，还会清空旋转状态机（`g_lastRotateHid` 等），防止旧 profile 的方向键卡在 HID report 里被新 profile 复用。
- 持久化：`main.cpp` 中加了匿名 namespace 里的 `loadBleProfileFromNvs()` / `saveBleProfileToNvs()`，每帧末尾比对当前 profile 与上次写入的索引，发现变化才 `Preferences::putUChar`，**避免每次按键都写 flash**。

### 9.5 后续扩展

- 单键 HID 编辑：在 3×3 选中 keyId 上，按 KEY2 进入"该键候选 HID 列表"循环。本版本只做 profile 切换；用户自定义某个按键需要改 `BleKeyMap.cpp::kProfiles` 数组后重烧。

- **功耗**：BLE 持续广播约 5 mA，长期续航需要后续接电池；
- **配对数量**：HID 默认允许 1 个 host，多 host 需要改库 `BLEDevice::setSecurityAuth`；
- **串口互斥**：现有 `serialPrintf()` 仍走 FreeRTOS mutex；BLE 库内部有自己 mutex，互不阻塞。
