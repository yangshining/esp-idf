# TFT 触摸屏示例设计文档

**日期：** 2026-04-16  
**目标芯片：** ESP32-S3  
**分支：** adapt/edge-ai-lab

---

## 概述

在 ESP-IDF fork 仓库的 `examples/peripherals/lcd/` 下新增 `tft_touch_s3` 示例工程，面向 Edge AI Lab 场景。该示例在 ESP32-S3 上驱动 ST7789 SPI TFT 显示屏（240×320）和 XPT2046 电阻触摸控制器，并通过 LVGL 实现一个三页触控导航菜单，预留 AI 推理结果接入接口。

---

## 目标与范围

**目标：**
- 提供开箱即用的 ESP32-S3 + ST7789 + XPT2046 硬件验证工程
- 通过 LVGL tabview 实现可扩展的触控多页面 UI 框架
- 预留 `ui_ai_update_result()` 接口，供后续 AI 推理任务填充结果

**不在范围内：**
- AI 推理逻辑本身
- 摄像头集成
- 新建可复用组件（component）

---

## 目录结构

```
examples/peripherals/lcd/tft_touch_s3/
├── CMakeLists.txt
├── README.md
├── sdkconfig.defaults                  # 通用默认配置
├── sdkconfig.defaults.esp32s3          # ESP32-S3 专属配置
└── main/
    ├── CMakeLists.txt
    ├── idf_component.yml               # 外部依赖声明
    ├── Kconfig.projbuild               # 可配置引脚 + 触摸开关
    ├── main.c                          # 入口：硬件初始化 + LVGL 启动
    ├── lcd_touch.c                     # SPI 总线、ST7789、XPT2046 初始化封装
    ├── lcd_touch.h
    └── ui/
        ├── ui_main.c                   # 顶层 UI：tabview 框架 + 页面注册
        ├── ui_main.h
        ├── ui_page_home.c              # 第一页：欢迎页 / 运行时信息
        ├── ui_page_home.h
        ├── ui_page_ai.c                # 第二页：AI 结果占位页
        ├── ui_page_ai.h                # 暴露 ui_ai_update_result() 接口
        ├── ui_page_settings.c          # 第三页：屏幕旋转 + 背光亮度控制
        └── ui_page_settings.h
```

---

## 硬件配置

### 接口

- **显示接口：** SPI（ST7789）
- **触摸接口：** SPI（XPT2046，与显示共用 SPI2_HOST 总线）
- **分辨率：** 240×320
- **色深：** RGB565（16-bit）
- **SPI 时钟：** 20 MHz

### 引脚定义（默认值，通过 Kconfig 可调）

| 信号 | GPIO | 说明 |
|------|------|------|
| SCLK | 18 | LCD + Touch 共用 |
| MOSI | 19 | LCD + Touch 共用 |
| MISO | 21 | 仅 XPT2046 使用 |
| LCD CS | 4 | ST7789 片选 |
| LCD DC | 5 | 数据/命令切换 |
| LCD RST | 3 | 复位 |
| Touch CS | 15 | XPT2046 片选 |
| 背光 BK | 2 | GPIO 低电平开启 |

---

## 外部依赖

`main/idf_component.yml`：

```yaml
dependencies:
  lvgl/lvgl: "9.3.0"
  atanisoft/esp_lcd_touch_xpt2046: "1.0.6"
```

与现有 `spi_lcd_touch` 示例版本保持一致。

---

## 软件架构

### 模块划分

| 模块 | 文件 | 职责 |
|------|------|------|
| 入口 | `main.c` | 调用 `lcd_touch_init()`，启动 LVGL task，调用 `ui_main_init()` |
| 硬件抽象 | `lcd_touch.c/h` | 初始化 SPI 总线、ST7789 panel、XPT2046 touch，返回 handle |
| UI 框架 | `ui_main.c/h` | 创建 `lv_tabview`，注册三个页面，管理旋转回调 |
| 主页 | `ui_page_home.c/h` | 显示芯片名称、运行时间（lv_label，1s 刷新）；暴露 `ui_page_home_init(lv_obj_t *parent)` |
| AI 页 | `ui_page_ai.c/h` | 占位卡片（lv_label + lv_bar），暴露 `ui_page_ai_init(lv_obj_t *parent)` 和 `ui_ai_update_result(const char*, float)` |
| 设置页 | `ui_page_settings.c/h` | 旋转按钮（循环 0/90/180/270°）、背光亮度滑块；暴露 `ui_page_settings_init(lv_obj_t *parent)` |

### UI 布局

```
┌─────────────────────────┐  240px
│  [主页]  [AI]  [设置]   │  ← tabview 标签栏，高 40px
├─────────────────────────┤
│                         │
│      页面内容区          │  280px 可用高度
│                         │
└─────────────────────────┘
```

### AI 页面扩展接口

```c
// ui_page_ai.h
/**
 * @brief 更新 AI 推理结果显示
 * @param label      分类标签字符串（如 "cat", "dog"）
 * @param confidence 置信度，范围 0.0 ~ 1.0
 */
void ui_ai_update_result(const char *label, float confidence);
```

**线程安全约定：** `lvgl_api_lock` 声明为非 static 全局变量，定义在 `lcd_touch.c`，在 `lcd_touch.h` 中以 `extern _lock_t lvgl_api_lock;` 暴露。任何需要调用 LVGL API 的外部模块（包括 AI 推理任务）均通过此头文件获取锁。调用方须先 `_lock_acquire(&lvgl_api_lock)`，调用完成后 `_lock_release(&lvgl_api_lock)`。

### 线程安全

- LVGL 运行在独立 FreeRTOS task（优先级 2，栈 **6 KB**，初始值需在验收测试中通过 `uxTaskGetHighWaterMark()` 调优）
- 所有 LVGL API 调用通过 `_lock_t lvgl_api_lock` 保护（定义在 `lcd_touch.c`，声明见 `lcd_touch.h`）
- `esp_timer` 提供 2ms tick（`lv_tick_inc`）
- LVGL draw buffer：2 个，各 `240 × 20 × 2 = 9.6 KB`，使用 `spi_bus_dma_memory_alloc` 分配

---

## sdkconfig 配置

`sdkconfig.defaults.esp32s3`：
```
CONFIG_SPIRAM=y
CONFIG_SPIRAM_MODE_OCT=y
CONFIG_SPIRAM_SPEED_80M=y
```

`sdkconfig.defaults`：
```
CONFIG_LV_CONF_SKIP=y
CONFIG_FREERTOS_HZ=1000
```

注：LVGL 9.x 中色深通过 `lv_display_set_color_format()` 在运行时设置，无需 Kconfig 符号。

注：`CONFIG_IDF_TARGET` 不写入 sdkconfig.defaults，由 `idf.py set-target esp32s3` 设置，与所有其他 LCD 示例保持一致。版本依赖采用精确锁定（无 `^`），确保 lab 环境的可重现性。

---

## 与现有示例的差异

| 项目 | `spi_lcd_touch`（现有） | `tft_touch_s3`（新增） |
|------|------------------------|----------------------|
| 目标芯片 | 全部芯片 | ESP32-S3 优化 |
| UI 内容 | 旋转弧动画 | 三页触控导航菜单 |
| AI 接口 | 无 | `ui_ai_update_result()` |
| 代码组织 | 单文件 main.c | 按模块分离到 lcd_touch / ui/ |
| 场景定位 | 通用验证 | Edge AI Lab 演示框架 |

---

## 验收标准

1. `idf.py set-target esp32s3 && idf.py build` 编译通过，`main/` 目录下项目自有源文件无新增 warning（第三方组件头文件产生的 warning 不计入）
2. Flash 到 ESP32-S3 后，屏幕点亮，显示 tabview 三页面
3. 触摸标签栏可切换页面，触摸设置页旋转按钮可旋转屏幕
4. 调用 `ui_ai_update_result("test", 0.95f)` 后 AI 页面内容更新
