# ESP32 USB Composite Framework (CDC + HID)

基于 ESP32-S2 / ESP32-S3 原生 USB-OTG 控制器构建的 USB 复合设备框架。实现免驱 USB CDC 虚拟串口通信与高精度 HID 输入控制（键盘 + 原生触摸屏 Digitizer + 消费级多媒体按键）。

---

## ✨ 核心特性

- **USB 复合设备 (Composite Device)**：单 USB 端口同时承载 CDC 虚拟串口与 HID 复合输入功能。
- **原生触摸屏协议 (Digitizer)**：
  - 严格遵循 Android 原生触摸规范，触摸操作**无鼠标指针残留**，完美适配安卓设备及触控屏终端。
  - 绝对坐标定位（0 ~ 32767），支持点按、长按及连续触控。
- **拟人化滑动算法 (Human-like Dragging)**：
  - 内置带扰动偏移的**二阶贝塞尔曲线 (Bézier Curve)** 插值算法，轨迹平滑自然，防机器风控检测。
- **系统级导航与快捷操作**：
  - Consumer 按键支持：返回键（Back）、主屏幕（Home）、多任务（Recents）、搜索唤醒（Search/Wake）。
  - 常用系统快捷键：一键全选（Ctrl+A）、粘贴（Ctrl+V）以及通用 HID 键码发送。
- **极简 CDC 串口协议**：通过任意串口助手即可发送 ASCII 指令控制目标设备。

---

## 🛠️ 硬件与环境要求

- **微控制器**：ESP32-S2 / ESP32-S3（需支持原生 USB OTG，注意连接到芯片的 D+/D- 引脚）
- **开发框架**：[ESP-IDF](https://github.com/espressif/esp-idf) >= 5.0
- **核心组件依赖**：
  - `espressif/esp_tinyusb` (^1.6.0)
  - `espressif/led_strip` (^3.0.0)

---

## 📡 串口控制指令协议

设备上电枚举为 CDC 虚拟串口后，波特率自适应（默认 115200 即可）。支持以下指令格式：

| 指令前缀 | 格式示例 | 说明 |
| :--- | :--- | :--- |
| `C` (Click) | `C,x,y,dur` | 在指定坐标 `(x, y)` 触发点击，按压持续时间 `dur` 毫秒 |
| `D` (Drag) | `D,x1,y1,x2,y2,pd,sd` | 从 `(x1, y1)` 滑动至 `(x2, y2)`，按下等待 `pd` 毫秒，滑动耗时 `sd` 毫秒（贝塞尔平滑轨迹） |
| `N` (Navigation) | `N,H`<br>`N,B`<br>`N,R`<br>`N,S` | 系统导航键：<br>• `H`：Home 主页<br>• `B`：Back 返回<br>• `R`：Recents 多任务<br>• `S`：Search/唤醒 |
| `K` (Key) | `K,keycode` | 发送指定 HID 键码（如 `K,40` 发送回车 Enter） |
| `V` (Paste) | `V` | 触发剪贴板粘贴（Ctrl + V） |
| `A` (Select All) | `A` | 触发全选（Ctrl + A） |

> 📌 **坐标范围**：X, Y 为绝对坐标，范围为 `0 ~ 32767`。例如屏幕分辨率为 1080x2400 时，需按比例映射至 0~32767。

---

## 🚀 快速上手与编译烧录

### 1. 配置工程目标芯片

```bash
# 以 ESP32-S3 为例
idf.py set-target esp32s3
```

### 2. 编译并烧录

```bash
idf.py build flash monitor
```

### 3. 连接与使用

1. 将 ESP32 的原生 USB（GPIO 19/20）连接至被控端（如 Android 手机、平板或 PC）。
2. 被控设备将识别到复合输入设备（键盘 + 触摸屏）。
3. 可以在 PC 端打开 CDC 虚拟串口，发送如 `C,16384,16384,100` 测试屏幕中心点击。

---

## 📂 项目结构

```
esp32_usb_framework/
├── CMakeLists.txt
├── dependencies.lock
├── sdkconfig.defaults
├── main/
│   ├── CMakeLists.txt
│   ├── idf_component.yml    # 依赖组件声明
│   └── main.c               # 核心实现逻辑 (CDC 任务, HID 描述符, 贝塞尔算法)
├── .gitignore
├── LICENSE
└── README.md
```

---

## 📄 开源许可证

本项目基于 [MIT License](LICENSE) 开源。
