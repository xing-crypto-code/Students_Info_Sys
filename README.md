# 学生成绩信息管理系统 · Student Score Information Management System

> 基于 **LVGL 9.5** 与 **SDL2 2.32.10** 的 Windows 桌面学生成绩管理程序
> A Windows desktop student score management application built on **LVGL 9.5** and **SDL2 2.32.10**

![Platform](https://img.shields.io/badge/platform-Windows%2010%2F11-0078D4)
![Language](https://img.shields.io/badge/language-C99-A8B9CC)
![GUI](https://img.shields.io/badge/GUI-LVGL%209.5-3C3C3C)
![Backend](https://img.shields.io/badge/backend-SDL2%202.32.10-1B4B8F)
![License](https://img.shields.io/badge/license-MIT-green)

**语言 / Language：** [中文文档](#中文文档) · [English Documentation](#english-documentation)

---

## 中文文档

### 一、项目简介

本项目是一个 C 语言课程设计作品，实现了一款可在 Windows 上独立运行的学生成绩信息管理系统。程序以 **LVGL 9.5** 作为图形界面库、以 **SDL2 2.32.10** 提供窗口与输入设备，业务逻辑全部用标准 **C99** 编写，合计约 **6 800 行**源码。

系统采用**数据核心层 / 图形界面层 / 平台适配层**三层分离的架构：

| 层次 | 位置 | 职责 |
| --- | --- | --- |
| 图形界面层 | `src/ui/` | 只用 LVGL 控件搭建界面、处理用户交互，通过 `core.h` 访问数据，**不直接触碰学生数组** |
| 数据核心层 | `src/core/` | 学生数组、CSV 读写、成绩计算、筛选、搜索、排序与统计，**不依赖任何 GUI 库** |
| 平台适配层 | `src/sdl/`、`src/hal/` | 把 SDL2 窗口、显示设备和鼠标键盘输入注册进 LVGL |

这种分层带来的直接好处是：**数据层的算法与界面完全解耦**，可以在没有图形环境的条件下单独测试；界面层更换渲染后端（例如从 SDL2 换到嵌入式显示驱动）时，核心代码不需要改动。

主界面固定为 **1080 × 720**，表格为 **12 列 × 最多 200 行**，中文通过 TinyTTF 动态加载思源黑体（NotoSansSC）显示。

### 二、功能特性

| 模块 | 功能 |
| --- | --- |
| **数据管理** | 按 12 位学号新增学生、双击单元格修改、删除模式 + 二次确认弹窗；人数上限 200 |
| **成绩计算** | 按可配置权重计算总分；按总分计算绩点；按总分降序计算排名 |
| **查询** | 在全部 12 个字段中搜索关键字，命中行黄色高亮约 5 秒，并自动滚动定位到第一条匹配记录 |
| **筛选** | 按年级、班级、性别、总分区间筛选；班级采用「年级-班级」组合避免同名班级混淆；总分按每 10 分分段 |
| **排序** | 10 个表头按钮支持升序 / 降序切换；字符字段（年级、班级）按其中的数字部分自然排序 |
| **导入** | 从 CSV 导入数据，支持文件夹路径并自动选取其中**最后修改时间最新**的 `students_*.csv` |
| **导出** | 导出学生成绩表（CSV）或按班级的统计结果（TXT），文件名自动附加时间戳 |
| **界面交互** | 固定表头、鼠标滚轮滚动、序号表头翻页、单元格选中（整行整列绿色）、不及格整行标红、新增行红色闪烁两次 |
| **运行辅助** | 首次运行自动弹出帮助（标志文件控制）、UTF-8 BOM 输出（Excel 可直接打开）、中文路径支持 |

### 三、系统架构

```text
                    ┌───────────────────────────────┐
                    │   main.c  (56 行)             │
                    │   lv_init → HAL → UI → 主循环  │
                    └───────────────┬───────────────┘
                                    │
              ┌─────────────────────┴─────────────────────┐
              │                                           │
     ┌────────▼─────────┐                      ┌──────────▼─────────┐
     │  src/ui/         │   仅调用 core.h       │  src/sdl/          │
     │  ui.c 3299 行    │   声明的接口          │  sdl_hal.c 237 行  │
     │  ui.h   90 行    │◄────────────────────►│  SDL2 窗口/显示/输入│
     │  LVGL 界面层     │                      │  src/hal/ 适配接口  │
     └────────┬─────────┘                      └────────────────────┘
              │  46 个函数
     ┌────────▼─────────┐
     │  src/core/       │
     │  core.c 2423 行  │  纯 C，不依赖 LVGL
     │  core.h  325 行  │  71 个函数
     │  数据核心层      │
     └──────────────────┘
              │
     ┌────────▼───────────────────────────────────────────┐
     │  学生数组 · CSV 读写 · 成绩计算 · 筛选/搜索/排序/统计 │
     └────────────────────────────────────────────────────┘
```

各层源码规模：

| 文件 | 行数 | 函数数 | 说明 |
| --- | ---: | ---: | --- |
| `src/core/core.c` | 2 423 | 71 | 数据核心层实现 |
| `src/core/core.h` | 325 | — | 数据层对外接口 |
| `src/ui/ui.c` | 3 299 | 46 | LVGL 界面层实现 |
| `src/ui/ui.h` | 90 | — | 界面层对外接口 |
| `src/sdl/sdl_hal.c` | 237 | — | SDL2 显示与输入适配 |
| `src/main.c` | 56 | — | 程序入口与主循环 |
| `src/freertos_main.c` | 190 | — | 可选的 FreeRTOS 入口 |
| `src/hal/hal.c` | 34 | — | HAL 接口 |

### 四、目录结构

```text
lv_port_pc_vscode-master/
├── src/                        源码（约 6 800 行）
│   ├── core/                   数据核心层
│   │   ├── core.c              学生数组、CSV、成绩计算、筛选排序统计
│   │   └── core.h              数据层接口、StudentCSV、stu_field_t
│   ├── ui/                     LVGL 界面层
│   │   ├── ui.c                工具栏、12 列表格、弹窗、帮助浮层
│   │   └── ui.h                界面层接口、ui_field_t、界面尺寸
│   ├── sdl/                    SDL2 平台适配层
│   │   ├── sdl_hal.c           创建窗口 / 显示设备 / 鼠标键盘输入设备
│   │   └── sdl_hal.h
│   ├── hal/                    硬件抽象层接口
│   ├── main.c                  程序入口与 LVGL 主循环
│   └── freertos_main.c         可选 FreeRTOS 版入口
├── bin/                        运行资源与构建产物
│   ├── main.exe                可执行文件（构建产物，不入库）
│   ├── SDL2.dll                SDL2 运行库
│   ├── students.csv            示例数据，170 名学生（2021～2023 级）
│   ├── fonts/                  中文字体 NotoSansSC-Regular.ttf
│   ├── btn_png/                工具栏按钮图标 8 个
│   └── help_png/               帮助图片 10 张，help1.png ~ help10.png
├── config/                     FreeRTOSConfig.h
├── lvgl/                       LVGL 9.5 图形库源码
├── SDL2-2.32.10/               SDL2 预编译库与头文件
├── mingw64/                    MinGW-w64 GCC 14.2.0 工具链（随仓库携带）
├── cmake-4.1.2-windows-x86_64/ CMake 4.1.2（随仓库携带）
├── ninja.exe                   Ninja 1.13.1 构建器（随仓库携带）
├── CMakeLists.txt              构建脚本
├── build.bat                   一键构建（双击即可，使用仓库自带的工具链）
├── run.bat                     一键运行（自动切到 bin 目录再启动 main.exe）
├── lv_conf.h                   LVGL 功能裁剪配置
├── simulator.code-workspace    VS Code 工作区配置
└── licence.txt                 MIT 许可证
```

> **关于随仓库携带的工具链**：`mingw64/`、`cmake-4.1.2-windows-x86_64/` 和 `ninja.exe` 一并提交到了版本库，目的是让仓库**克隆后即可离线构建**，无需额外配置编译环境。代价是仓库体积较大（工作区约 1.7 GB）。

### 五、核心数据结构

数据层的唯一数据源是 `core.c` 中的静态数组 `s_students[200]`，配套一张**显示行映射表** `s_display_indexes[]`——筛选和排序只重排映射表，不搬动学生数组本身。

```c
typedef struct {
    char     grade[64];       /* 年级，例如 "2023级"            */
    char     class_name[64];  /* 班级，例如 "计算机1班"          */
    uint64_t id;              /* 学号（12 位数字）               */
    char     name[64];        /* 姓名                            */
    bool     gender;          /* 性别：true = 男，false = 女     */
    float    regular_score;   /* 平时成绩 0.0 ~ 100.0            */
    float    midterm_score;   /* 期中成绩 0.0 ~ 100.0            */
    float    final_score;     /* 期末成绩 0.0 ~ 100.0            */
    float    score;           /* 总分（按权重实时计算）           */
    float    gpa;             /* 绩点（按总分实时计算）           */
    uint16_t rank;            /* 排名，从 1 开始                 */

    /* 以下两个字段只在内存中使用，不写入 CSV */
    bool     is_new;          /* true = 新增且尚未填写完整的行    */
    uint16_t new_filled_mask; /* 已正确填写的字段位图             */
} StudentCSV;
```

字段编号由枚举 `stu_field_t` 定义，界面层的 `ui_field_t` 与之取值完全一致，因此 UI 可以把字段编号直接传给数据层函数：

```c
STU_FIELD_GRADE=0, CLASS=1, ID=2, NAME=3, GENDER=4,
REGULAR=5, MIDTERM=6, FINAL=7, SCORE=8, GPA=9, RANK=10
```

### 六、业务规则

**总分**（权重可修改，默认 30% / 20% / 50%）：

```text
总分 = (平时 × 平时权重 + 期中 × 期中权重 + 期末 × 期末权重) / 100
```

**绩点**——以 100 分为 5.0 分基准，每低 1 分减 0.1，不及格归零：

| 总分 | 100 | 95 | 88.5 | 60 | 59.9 |
| --- | --- | --- | --- | --- | --- |
| 绩点 | 5.00 | 4.50 | 3.85 | 1.00 | 0.00 |

**排名**：按总分从高到低计算，同分不并列，按表格当前顺序依次编号。

**关于「原地刷新」**：在单元格中修改平时 / 期中 / 期末成绩后，程序只重新计算该生的总分与绩点，并调用 `core_recalc_rank_inplace()` **就地刷新所有学生的排名数值**，学生数组和表格行顺序都保持不变——这样用户正在编辑的行不会突然跳走。只有修改权重、导入数据或删除学生时才会重新排序。

**及格线为 60 分**：总分低于 60 的学生整行标红。

### 七、CSV 数据格式

**新格式（11 列，推荐）**：

```text
年级,班级,学号,姓名,性别,平时,期中,期末,总分,绩点,排名
2023级,计算机1班,202300000001,张三,男,90.0,80.0,70.0,78.0,2.80,1
```

**旧格式（8 列，兼容导入）**：

```text
年级,班级,学号,姓名,性别,分数,绩点,排名
```

导入旧格式时，原有的「分数」会同时填入平时、期中、期末三项，保证总分不变。

程序启动时**不自动读取任何 CSV**，数据从空白开始；`bin/students.csv` 是随程序附带的 170 条示例数据，需通过工具栏「导入」手动加载。导出的文件均为 **UTF-8 with BOM**，Excel 可直接打开。

### 八、界面与操作

界面分为两部分：上方 1080 × 80 的工具栏，下方 1080 × 560 的表格区。

| 工具栏控件 | 作用 |
| --- | --- |
| 搜索框 | 输入关键字后回车或点击放大镜，在 12 个字段中搜索 |
| 筛选 | 依次选择分类（年级 / 班级 / 性别 / 总分）与具体选项 |
| 新增 | 输入 12 位学号新增学生 |
| 帮助 | 弹出帮助图片（共 10 张，点击切换） |
| 导入 | 输入文件夹地址，自动导入最新的 `students_*.csv` |
| 导出 | 选择导出成绩表（CSV）或统计结果（TXT） |
| 删除 | 切换删除模式，点击行后弹出确认窗口 |

**表格 12 列**：序号、年级、班级、学号、姓名、性别、平时、期中、期末、总分、绩点、排名。其中**序号、学号、总分、绩点、排名不可编辑**。

**几个不那么直观的操作**：

- **单击序号表头可翻页**——每点一次向下翻 40 行，翻到最后一行后再点会回到第一行；
- **单击「平时 / 期中 / 期末」表头可修改权重**——一次输入三个数，分隔符支持空格、英文逗号、中文逗号和斜杠（如 `30 20 50`、`30,20,50`、`30/20/50`），三项之和必须为 100，否则弹窗并恢复默认值；
- **选中单元格需要点两次**——第一次单击选中（整行整列变绿、焦点格变黄），第二次单击同一格才打开编辑框；
- **所有输入框都支持 `Ctrl+V`** 粘贴。

**帮助图片**：`bin/help_png/` 下的 PNG 由 LodePNG 解码后缩放到屏幕内显示。程序启动时检查标志文件 `bin/help_shown.flag`，文件不存在则自动弹出帮助并创建该文件；删除该文件即可再次看到首次运行的帮助。

### 九、构建与运行

**最简单的方式**：双击项目根目录的 `build.bat`（自动配置 CMake 并编译），看到 `Build succeeded` 后双击 `run.bat` 启动程序。

**开箱即用**——仓库自带全部工具链，克隆后无需额外安装任何软件：

```bat
:: 在项目根目录执行（cmd）
set "PATH=%~dp0mingw64\bin;%~dp0cmake-4.1.2-windows-x86_64\bin;%~dp0;%PATH%"

cmake -S . -B build -G Ninja ^
      -DCMAKE_BUILD_TYPE=Release ^
      -DCMAKE_MAKE_PROGRAM=%~dp0ninja.exe

ninja -C build main
```

PowerShell 等价写法：

```powershell
$root = $PSScriptRoot
$env:PATH = "$root\mingw64\bin;$root\cmake-4.1.2-windows-x86_64\bin;$root;$env:PATH"

& "$root\cmake-4.1.2-windows-x86_64\bin\cmake.exe" -S $root -B "$root\build" -G Ninja `
    -DCMAKE_BUILD_TYPE=Release -DCMAKE_MAKE_PROGRAM="$root\ninja.exe"
& "$root\ninja.exe" -C "$root\build" main
```

生成的程序位于 `bin\main.exe`。**必须从 `bin` 目录运行**（或直接双击 `run.bat`）：LVGL 的 `A:` 盘符映射到程序当前工作目录，字体、图标和帮助图片都相对于它查找。若从项目根目录启动，中文字体与图标会静默加载失败。

```bat
cd bin && main.exe
```

> 上述命令已在 **Windows 11 + GCC 14.2.0 + CMake 4.1.2 + Ninja 1.13.1** 环境下完整验证：全量构建 930 个目标，约 26 秒完成。

### 十、开发环境

| 项目 | 版本 / 说明 |
| --- | --- |
| 操作系统 | Windows 10 / 11 |
| 语言标准 | C99 |
| 编译器 | MinGW-w64 GCC 14.2.0（`x86_64-win32-seh-rev2`） |
| 构建工具 | CMake 4.1.2 + Ninja 1.13.1 |
| GUI 库 | LVGL 9.5 |
| 窗口与输入 | SDL2 2.32.10 |
| 中文字体 | NotoSansSC-Regular.ttf（经 TinyTTF 加载，16 px） |
| PNG 解码 | LVGL 内置 LodePNG |
| 数据格式 | CSV / TXT（UTF-8 with BOM） |

**`lv_conf.h` 中的关键配置**：

```c
#define LV_USE_SDL                   1   /* SDL2 显示/输入驱动      */
#define LV_USE_FS_WIN32              1   /* 支持 A: 盘符式路径      */
#define LV_USE_TINY_TTF              8   /* TrueType 字体加载       */
#define LV_TINY_TTF_FILE_SUPPORT     1
#define LV_USE_LODEPNG               1   /* PNG 解码                */
#define LV_COLOR_DEPTH              32   /* ARGB8888               */
#define LV_USE_OS            LV_OS_NONE  /* 不使用 RTOS（默认）      */
```

`CMakeLists.txt` 中另提供 `USE_FREERTOS` 选项（默认 `OFF`），开启后会改为编译 `src/freertos_main.c` 并链接 FreeRTOS 内核。

### 十一、许可证

本项目采用 **MIT 许可证**发布，详见 [`licence.txt`](licence.txt)。其中 LVGL 与 SDL2 分别遵循各自的许可证。

---

## English Documentation

### 1. Overview

This project is a C programming course design assignment: a self-contained student score management system for Windows. The GUI is built with **LVGL 9.5**, the window and input devices are provided by **SDL2 2.32.10**, and all business logic is written in standard **C99** — roughly **6,800 lines** of source code in total.

The system is organised into three clearly separated layers:

| Layer | Location | Responsibility |
| --- | --- | --- |
| GUI layer | `src/ui/` | Builds the interface and handles user interaction with LVGL widgets only. It reaches data through `core.h` and **never touches the student array directly**. |
| Data core layer | `src/core/` | Student array, CSV I/O, score calculation, filtering, searching, sorting and statistics. **Depends on no GUI library.** |
| Platform layer | `src/sdl/`, `src/hal/` | Registers the SDL2 window, display device and mouse/keyboard input devices with LVGL. |

The main benefit of this split is that the **data algorithms stay completely decoupled from the interface**: they can be tested without any graphical environment, and swapping the rendering backend (for example, from SDL2 to an embedded display driver) requires no change to the core code.

The window is fixed at **1080 × 720**, the table holds **12 columns by up to 200 rows**, and Chinese text is rendered by dynamically loading the Noto Sans SC font through TinyTTF.

### 2. Features

| Area | Capability |
| --- | --- |
| **Data management** | Add a student by 12-digit ID, edit by double-clicking a cell, delete through a confirmation dialog. Capacity is capped at 200 students. |
| **Score calculation** | Weighted total score, grade point average derived from the total, and ranking sorted by total score descending. |
| **Search** | Search a keyword across all 12 fields; matching rows highlight in yellow for about 5 seconds and the view scrolls to the first match. |
| **Filtering** | Filter by grade, class, gender or score range. Classes use a "grade-class" composite key so that identically named classes in different grades stay distinct; scores are bucketed in steps of 10. |
| **Sorting** | Ten column headers toggle between ascending and descending order. Text fields such as grade and class sort naturally by the numeric part of the string. |
| **Import** | Import from CSV, accepting a folder path and automatically picking the **most recently modified** `students_*.csv` inside it. |
| **Export** | Export the score table as CSV, or per-class statistics as TXT, with a timestamp appended to the file name automatically. |
| **Interface** | Sticky table header, mouse-wheel scrolling, paging by clicking the index header, cell selection, whole-row highlighting in red for failing students, and a red flash on newly added rows. |
| **Convenience** | Help images on first run controlled by a flag file, UTF-8 BOM output that Excel opens directly, and full support for Chinese file paths. |

### 3. Architecture

```text
                    ┌───────────────────────────────┐
                    │   main.c  (56 lines)          │
                    │   lv_init → HAL → UI → loop   │
                    └───────────────┬───────────────┘
                                    │
              ┌─────────────────────┴─────────────────────┐
              │                                           │
     ┌────────▼─────────┐                      ┌──────────▼─────────┐
     │  src/ui/         │   calls only the     │  src/sdl/          │
     │  ui.c 3299 lines │   API declared in    │  sdl_hal.c 237 ln  │
     │  ui.h   90 lines │◄────────────────────►│  SDL2 window/input │
     │  LVGL GUI layer  │       core.h         │  src/hal/          │
     └────────┬─────────┘                      └────────────────────┘
              │  46 functions
     ┌────────▼─────────┐
     │  src/core/       │
     │  core.c 2423 ln  │  Plain C, no LVGL dependency
     │  core.h  325 ln  │  71 functions
     │  Data core layer │
     └──────────────────┘
              │
     ┌────────▼───────────────────────────────────────────┐
     │  Student array · CSV I/O · Score calculation ·      │
     │  Filtering / searching / sorting / statistics       │
     └────────────────────────────────────────────────────┘
```

Source size by file:

| File | Lines | Functions | Purpose |
| --- | ---: | ---: | --- |
| `src/core/core.c` | 2,423 | 71 | Data core implementation |
| `src/core/core.h` | 325 | — | Public data-layer API |
| `src/ui/ui.c` | 3,299 | 46 | LVGL interface implementation |
| `src/ui/ui.h` | 90 | — | Public UI API |
| `src/sdl/sdl_hal.c` | 237 | — | SDL2 display and input plumbing |
| `src/main.c` | 56 | — | Entry point and main loop |
| `src/freertos_main.c` | 190 | — | Optional FreeRTOS entry point |
| `src/hal/hal.c` | 34 | — | HAL interface |

### 4. Project Structure

```text
lv_port_pc_vscode-master/
├── src/                        Source code (~6,800 lines)
│   ├── core/                   Data core layer
│   │   ├── core.c              Student array, CSV, scores, filtering, statistics
│   │   └── core.h              Data API, StudentCSV, stu_field_t
│   ├── ui/                     LVGL interface layer
│   │   ├── ui.c                Toolbar, 12-column grid, dialogs, help overlay
│   │   └── ui.h                UI API, ui_field_t, screen size
│   ├── sdl/                    SDL2 platform layer
│   │   ├── sdl_hal.c           Creates window, display and input devices
│   │   └── sdl_hal.h
│   ├── hal/                    Hardware abstraction interface
│   ├── main.c                  Entry point and LVGL main loop
│   └── freertos_main.c         Optional FreeRTOS entry point
├── bin/                        Runtime assets and build output
│   ├── main.exe                Executable (build product, not tracked)
│   ├── SDL2.dll                SDL2 runtime library
│   ├── students.csv            170 sample students (grades 2021-2023)
│   ├── fonts/                  NotoSansSC-Regular.ttf
│   ├── btn_png/                8 toolbar button icons
│   └── help_png/               10 help images, help1.png .. help10.png
├── config/                     FreeRTOSConfig.h
├── lvgl/                       LVGL 9.5 source tree
├── SDL2-2.32.10/               Prebuilt SDL2 libraries and headers
├── mingw64/                    MinGW-w64 GCC 14.2.0 toolchain (vendored)
├── cmake-4.1.2-windows-x86_64/ CMake 4.1.2 (vendored)
├── ninja.exe                   Ninja 1.13.1 builder (vendored)
├── CMakeLists.txt              Build script
├── build.bat                   One-click build (double-click; uses the vendored toolchain)
├── run.bat                     One-click run (switches to bin\ before launching main.exe)
├── lv_conf.h                   LVGL feature configuration
├── simulator.code-workspace    VS Code workspace configuration
└── licence.txt                 MIT license
```

> **About the vendored toolchain**: `mingw64/`, `cmake-4.1.2-windows-x86_64/` and `ninja.exe` are committed to the repository on purpose, so that a fresh clone can be **built offline with no extra setup**. The trade-off is repository size — the working tree is roughly 1.7 GB.

### 5. Core Data Structure

The single source of truth in the data layer is the static array `s_students[200]` in `core.c`, paired with a **display-row mapping table** `s_display_indexes[]`. Filtering and sorting only rearrange that mapping table; the student array itself is never reshuffled.

```c
typedef struct {
    char     grade[64];       /* Grade, e.g. "2023级"            */
    char     class_name[64];  /* Class, e.g. "计算机1班"          */
    uint64_t id;              /* Student ID (12 digits)          */
    char     name[64];        /* Full name                       */
    bool     gender;          /* true = male, false = female     */
    float    regular_score;   /* Coursework score, 0.0 - 100.0   */
    float    midterm_score;   /* Midterm score,    0.0 - 100.0   */
    float    final_score;     /* Final exam score, 0.0 - 100.0   */
    float    score;           /* Weighted total                  */
    float    gpa;             /* Grade point average             */
    uint16_t rank;            /* Ranking, starting from 1        */

    /* The two fields below are memory-only and never written to CSV */
    bool     is_new;          /* true = newly added, incomplete   */
    uint16_t new_filled_mask; /* Bitmap of correctly filled fields*/
} StudentCSV;
```

Field indices are defined by the `stu_field_t` enumeration. The UI layer's `ui_field_t` uses exactly the same values, so the interface can pass a field index straight to a data-layer function:

```c
STU_FIELD_GRADE=0, CLASS=1, ID=2, NAME=3, GENDER=4,
REGULAR=5, MIDTERM=6, FINAL=7, SCORE=8, GPA=9, RANK=10
```

### 6. Business Rules

**Total score** (weights are configurable; defaults are 30% / 20% / 50%):

```text
total = (coursework × w_coursework
       + midterm    × w_midterm
       + final      × w_final) / 100
```

**Grade point average** — anchored at 100 points = 5.0, dropping by 0.1 per point, and falling to zero below the pass mark:

| Total score | 100 | 95 | 88.5 | 60 | 59.9 |
| --- | --- | --- | --- | --- | --- |
| GPA | 5.00 | 4.50 | 3.85 | 1.00 | 0.00 |

**Ranking**: computed by total score in descending order. Ties are not shared — equal scores are numbered in the table's current order.

**On in-place refresh**: after editing coursework, midterm or final grades in a cell, the program recalculates only that student's total and GPA, then calls `core_recalc_rank_inplace()` to **update every student's rank value in place**. Neither the student array nor the row order changes, so the row being edited never jumps out from under the user. The array is only re-sorted when weights change, data is imported, or a student is deleted.

**The pass mark is 60**: any student whose total falls below 60 is highlighted across the entire row in red.

### 7. CSV Format

**Current format (11 columns, recommended)**:

```text
年级,班级,学号,姓名,性别,平时,期中,期末,总分,绩点,排名
2023级,计算机1班,202300000001,张三,男,90.0,80.0,70.0,78.0,2.80,1
```

**Legacy format (8 columns, still importable)**:

```text
年级,班级,学号,姓名,性别,分数,绩点,排名
```

When a legacy file is imported, the single score column is copied into all three of coursework, midterm and final, so the total score is preserved.

The program **loads no CSV automatically at startup** — it begins with an empty dataset. `bin/students.csv` ships alongside the binary as a sample of 170 students and must be loaded manually via the Import button. Everything the program exports is written as **UTF-8 with BOM** so Excel opens it correctly.

### 8. Interface and Controls

The window is split into a 1080 × 80 toolbar on top and a 1080 × 560 table area below.

| Toolbar control | Action |
| --- | --- |
| Search box | Enter a keyword and press Return, or click the magnifier, to search all 12 fields |
| Filter | Pick a category (grade / class / gender / score) and then a specific option |
| Add | Enter a 12-digit student ID to add a student |
| Help | Show the help images (10 in total, click to advance) |
| Import | Enter a folder path to load the newest `students_*.csv` |
| Export | Choose either the score table (CSV) or statistics (TXT) |
| Delete | Toggle delete mode, then click a row to confirm removal |

**The table has 12 columns**: index, grade, class, student ID, name, gender, coursework, midterm, final, total, GPA and rank. The **index, student ID, total, GPA and rank columns are read-only**.

**A few less obvious interactions**:

- **Click the "序号" (index) header to page through the table** — each click scrolls down 40 rows, and clicking again once the last row is visible jumps back to the top;
- **Click the "平时 / 期中 / 期末" headers to change the score weights** — enter all three values at once, separated by spaces, commas (ASCII or full-width) or slashes, for example `30 20 50`, `30,20,50` or `30/20/50`. The three values must sum to 100; otherwise a warning dialog appears and the defaults are restored;
- **Selecting a cell takes two clicks** — the first click selects it (the row and column turn green, the focused cell turns yellow), and a second click on the same cell opens the editor;
- **Every input field supports `Ctrl+V`** pasting.

**Help images**: the PNGs under `bin/help_png/` are decoded with LodePNG and scaled to fit the screen. On startup the program checks for the flag file `bin/help_shown.flag`; if it is missing, the help overlay is shown automatically and the flag file is created. Delete that file to see the first-run help again.

### 9. Building and Running

**Quickest way**: double-click `build.bat` in the project root (it configures CMake and compiles), then double-click `run.bat` to launch the app.

**Everything needed is already in the repository** — a fresh clone requires no additional software:

```bat
:: Run from the project root (cmd)
set "PATH=%~dp0mingw64\bin;%~dp0cmake-4.1.2-windows-x86_64\bin;%~dp0;%PATH%"

cmake -S . -B build -G Ninja ^
      -DCMAKE_BUILD_TYPE=Release ^
      -DCMAKE_MAKE_PROGRAM=%~dp0ninja.exe

ninja -C build main
```

The equivalent in PowerShell:

```powershell
$root = $PSScriptRoot
$env:PATH = "$root\mingw64\bin;$root\cmake-4.1.2-windows-x86_64\bin;$root;$env:PATH"

& "$root\cmake-4.1.2-windows-x86_64\bin\cmake.exe" -S $root -B "$root\build" -G Ninja `
    -DCMAKE_BUILD_TYPE=Release -DCMAKE_MAKE_PROGRAM="$root\ninja.exe"
& "$root\ninja.exe" -C "$root\build" main
```

The resulting executable is `bin\main.exe`. **It must be run from the `bin` directory** (or just double-click `run.bat`): LVGL's `A:` drive maps to the process working directory, and the fonts, icons and help images are resolved relative to it. Launching it from the project root makes the CJK font and the icons fail silently.

```bat
cd bin && main.exe
```

> The commands above were verified end to end on **Windows 11 with GCC 14.2.0, CMake 4.1.2 and Ninja 1.13.1**: a full build of 930 targets completes in about 26 seconds.

### 10. Environment

| Item | Version / Notes |
| --- | --- |
| Operating system | Windows 10 / 11 |
| Language standard | C99 |
| Compiler | MinGW-w64 GCC 14.2.0 (`x86_64-win32-seh-rev2`) |
| Build tools | CMake 4.1.2 + Ninja 1.13.1 |
| GUI library | LVGL 9.5 |
| Window and input | SDL2 2.32.10 |
| Chinese font | NotoSansSC-Regular.ttf, loaded through TinyTTF at 16 px |
| PNG decoding | LVGL's bundled LodePNG |
| Data formats | CSV / TXT (UTF-8 with BOM) |

**Key settings in `lv_conf.h`**:

```c
#define LV_USE_SDL                   1   /* SDL2 display/input driver  */
#define LV_USE_FS_WIN32              1   /* Enables A: style paths     */
#define LV_USE_TINY_TTF              8   /* TrueType font loading      */
#define LV_TINY_TTF_FILE_SUPPORT     1
#define LV_USE_LODEPNG               1   /* PNG decoding               */
#define LV_COLOR_DEPTH              32   /* ARGB8888                   */
#define LV_USE_OS            LV_OS_NONE  /* No RTOS by default         */
```

`CMakeLists.txt` also exposes a `USE_FREERTOS` option (off by default). Enabling it switches the build to `src/freertos_main.c` and links against the FreeRTOS kernel instead.

### 11. License

This project is released under the **MIT License**; see [`licence.txt`](licence.txt). LVGL and SDL2 are covered by their own respective licenses.
