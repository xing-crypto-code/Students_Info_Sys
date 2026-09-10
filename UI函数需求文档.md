# 学生成绩信息管理系统 — UI 函数需求文档

> 本文档用于说明 LVGL9.5 UI 框架层已交付内容，以及 UI 框架后续需要的外部数据层函数。
> UI 框架文件：`ui.h`、`ui.c`
> 数据层函数不在 UI 框架中实现，需按本文档确定名称和功能后续编写。

---

## 1. 已交付内容

| 文件 | 说明 |
| --- | --- |
| `ui.h` | UI 框架对外接口、尺寸/字段常量、外部数据层函数需求声明 |
| `ui.c` | LVGL9.5 UI 框架实现：界面布局、按钮交互、网格点击/编辑、筛选弹层、删除确认、提示弹窗 |

---

## 2. 数据结构约定

UI 网格共 9 列：

| 网格列 | 含义 | StudentCSV 字段 |
| --- | --- | --- |
| 第 1 列 | 序号 | 不存储，显示行号 |
| 第 2 列 | 年级 | `grade` |
| 第 3 列 | 班级 | `class_name` |
| 第 4 列 | 学号 | `id` |
| 第 5 列 | 姓名 | `name` |
| 第 6 列 | 性别 | `gender` |
| 第 7 列 | 分数 | `score` |
| 第 8 列 | 绩点 | `gpa` |
| 第 9 列 | 排名 | `rank` |

字段枚举在 `ui.h` 中已定义为：

```c
typedef enum {
    UI_FIELD_GRADE  = 0,
    UI_FIELD_CLASS  = 1,
    UI_FIELD_ID     = 2,
    UI_FIELD_NAME   = 3,
    UI_FIELD_GENDER = 4,
    UI_FIELD_SCORE  = 5,
    UI_FIELD_GPA    = 6,
    UI_FIELD_RANK   = 7,
    UI_FIELD_COUNT  = 8
} ui_field_t;
```

---

## 2.5 图标配置说明

### 2.5.1 图标文件夹地址在哪里修改

在 `ui.h` 中修改：

```c
#define UI_ICON_FOLDER "A:icons/"
```

- 默认写的是 LVGL 文件系统路径 `A:icons/`；
- 如果你把图标放在 `C:/my_app/icons/`：
  - 方式一：开启 LVGL stdio 文件系统，并把 `A:` 映射到 `C:/my_app/`，则保持 `"A:icons/"` 不变；
  - 方式二：你的 LVGL 工程支持直接读物理路径，可改成 `"C:/my_app/icons/"`；
  - 方式三：如果只把图标放在当前程序目录下的 `icons/` 文件夹，可改成 `"./icons/"`，但需要你的 LVGL 文件系统支持该路径。

> 注意：LVGL 默认没有开启“从文件读图片”的文件系统和 PNG 解码器。
> 如果实际运行看不到图标，需要在 `lv_conf.h` 中开启：
> ```c
> #define LV_USE_FS_STDIO 1
> #define LV_FS_STDIO_LETTER 'A'
> #define LV_FS_STDIO_PATH "你的物理目录/"
> #define LV_USE_LODEPNG 1   /* 或使用你工程支持的 PNG 解码器 */
> ```
> 并在初始化 LVGL 后调用 `lv_fs_stdio_init();`。

### 2.5.2 图标文件重命名规则

请把你提供的图标放到 `UI_ICON_FOLDER` 指定目录中，并按下面的名称重命名：

| 用途 | 文件名 |
| --- | --- |
| 顶部搜索框左侧图标 | `search.png` |
| 顶部筛选按钮 | `filter.png` |
| 顶部帮助按钮 | `help.png` |
| 顶部导入按钮 | `import.png` |
| 顶部导出按钮 | `export.png` |
| 顶部删除按钮 | `delete.png` |
| 表头排序按钮（年级/班级/性别/分数共用） | `sort.png` |

### 2.5.3 帮助图片地址在哪里修改

帮助图片不通过 `UI_ICON_FOLDER` 读取，而是由外部函数提供：

```c
const void * ui_help_get_image_src(int index);
```

- 外部实现这个函数时，在对应实现文件（例如 `student_data.c` / `help_images.c`）中修改帮助图片目录；
- `ui.h` 中也预留了帮助图片目录宏：

```c
#define UI_HELP_IMAGE_FOLDER "A:help/"
```

外部实现可读取 `UI_HELP_IMAGE_FOLDER + "help1.png"`、`"help2.png"` 等文件并返回给 UI。

---

## 3. UI 框架需求摘要

### 3.1 主界面布局

- 初始窗口/屏幕：`1080 × 720`
- 顶部容器：`1080 × 80`，位于 `(0, 0)`，白色背景，无边框，无内边距
- 顶部按钮：
  - 左侧：搜索输入框 `300 × 60`，筛选按钮 `60 × 60`
  - 右侧：帮助、导入、导出、删除按钮，均为 `60 × 60`
  - 按钮垂直居中，相邻按钮间距 `10px`
- 下方网格容器：`1080 × 560`，位于 `(0, 100)`，白色背景，无边框，无内边距

### 3.2 网格规则

- 9 列网格，固定行高 `50px`
- 第 1 行为表头，第 2~9 列表头分别为：
  `序号、年级、班级、学号、姓名、性别、分数、绩点、排名`
- 第 2 行开始为数据行，第 1 列生成从 1 开始的序号
- 默认最多 200 行数据行
- 无数据时每列默认宽度 `120px`
- 有数据后列宽按内容自适应，网格整体左对齐，单元格内容居中

### 3.3 表头排序按钮

- 在“年级、班级、性别、分数”这四个表头格的右侧增加排序按钮；
- 排序按钮尺寸：`20 × 40`；
- 排序按钮使用同一个图标文件：`sort.png`；
- 点击第一次：调用排序函数进行正序排序；
- 再点击一次：调用排序函数进行倒序排序；
- 之后继续点击会在正序/倒序之间循环切换；
- 排序后刷新网格显示。

### 3.4 交互功能

- 搜索：搜索框输入后回车，在当前筛选结果中搜索，匹配行黄色高亮约 2 秒
- 筛选：
  - 点击筛选弹出第一级列表：年级、班级、性别、分数、全部
  - 点击具体类别后，在第一级列表右侧弹出第二级选项列表
  - 点击“全部”清除筛选并刷新表格
- 帮助：
  - 创建 `help_imagebtn`，尺寸 `1080 × 80`，位置 `(0, 0)`
  - 每点击一次切换到下一张图
  - 点击最后一张图后删除图片按钮
- 导入：
  - 在导入按钮中心位置弹出 `400 × 60` 输入框
  - 输入 CSV 文件地址后回车调用导入函数
  - 导入完成后刷新表格
- 导出：
  - 在导出按钮中心位置弹出 `400 × 60` 输入框
  - 输入导出文件地址后回车调用导出函数
  - 导出成功后弹窗 2 秒提示“文件已导出至'用户输入的地址'”
  - 点击弹窗可直接关闭
- 删除：
  - 删除按钮有“开启/关闭”两种状态，默认关闭
  - 开启后点击数据行，在窗口正中央弹出 `500 × 200` 确认框
  - 上方提示：你确定要删除“选中对象”的所有信息吗？
  - 下方两个 `80 × 50` 按钮：取消（绿色背景）、确定（红色背景）
  - 确定后删除对应学生并将后续元素前移一位，然后刷新表格
- 单元格编辑：
  - 单击单元格后整行/整列变绿，当前格变黄
  - 再次点击同一可编辑单元格，弹出编辑框
  - 编辑框内默认显示当前单元格内容
  - 输入回车或点击编辑框外部保存编辑内容
  - 保存后写回对应 `StudentCSV` 字段，并刷新表格

---

## 4. 外部数据层函数需求

以下函数由 UI 框架调用，但不在 `ui.c` 中实现。
后续编写数据层文件（如 `student_data.c`）时，必须提供以下确定名称和功能。

### 4.1 数据访问

| 函数原型 | 功能说明 |
| --- | --- |
| `int data_get_count(void);` | 获取当前内存中学生总数（未筛选状态）。 |
| `StudentCSV *data_get_all(void);` | 返回学生结构体数组首地址，数组长度为 `data_get_count()`。 |
| `int data_get_display_count(void);` | 返回网格当前应显示的数据行数。见下方“显示数量约定”。 |
| `int data_get_display_index(int display_row);` | 把显示行号（0 开始）映射为 `StudentCSV` 数组下标；空行返回 `-1`。 |

### 4.2 导入 / 导出

| 函数原型 | 功能说明 |
| --- | --- |
| `int data_import_csv(const char *path);` | 从指定 CSV 路径导入数据到内存数组；成功返回导入条数，失败返回 `-1`。 |
| `int data_export_csv(const char *path);` | 把内存中当前学生数据写成 CSV；成功返回导出条数，失败返回 `-1`。 |

### 4.3 删除 / 修改

| 函数原型 | 功能说明 |
| --- | --- |
| `int data_delete_student(int index);` | 按下标删除一个学生，并将后面的元素前移一位；成功返回 `1`，失败返回 `0`。 |
| `int data_update_student_field(int index, int field, const char *text);` | 修改指定学生的某个字段；`field` 使用 `ui_field_t`，由数据层把文本解析为字符串/整数/浮点/性别；成功返回 `1`，失败返回 `0`。 |

### 4.4 筛选

| 函数原型 | 功能说明 |
| --- | --- |
| `int data_get_filter_options(int field, char options[][MAX_FIELD_LEN], int max_opts);` | 获取某个字段的可筛选选项：年级/班级/性别返回不同值，分数返回每 10.0 分一段的区间；返回选项数量。 |
| `int data_apply_filter(int field, const char *option);` | 按字段和选项筛选数据；筛选后 `data_get_display_count()` 和 `data_get_display_index()` 同步变化；成功返回 `1`。 |
| `void data_clear_filter(void);` | 清除当前筛选，恢复显示全部数据。 |

### 4.5 搜索

| 函数原型 | 功能说明 |
| --- | --- |
| `int data_find_matches(const char *keyword, int *out_rows, int max_rows);` | 在当前筛选结果中搜索关键字，返回匹配的显示行号数组；返回匹配数量。 |

### 4.6 排序

| 函数原型 | 功能说明 |
| --- | --- |
| `void data_sort_students(int field, bool ascending);` | 按指定字段排序学生数组；`field` 使用 `ui_field_t`，`ascending=true` 正序，`false` 倒序。 |

排序规则约定：

- 含字符的字段（例如年级、班级、姓名等混合数字/文字）：
  - 优先提取字符串中的数字部分，按数字大小排序；
  - 数字部分相同时，再按普通字符串顺序排序；
  - 示例：`计算机2班` 排在 `计算机10班` 前面。
- 纯数字字段（学号、分数、绩点、排名）：
  - 按数值大小排序，而不是按字符串排序。
- bool 型字段（性别）：
  - `true` 视为 `1`，`false` 视为 `0`；
  - 正序排序按 `1 > 0`，即“男”排在“女”前面；
  - 倒序排序按 `0 > 1`，即“女”排在“男”前面。

### 4.7 帮助图片

| 函数原型 | 功能说明 |
| --- | --- |
| `int ui_help_get_image_count(void);` | 返回帮助图片总张数。 |
| `const void *ui_help_get_image_src(int index);` | 返回第 `index` 张帮助图片源，供 `lv_imagebutton_set_src` 使用。帮助图片目录见 `UI_HELP_IMAGE_FOLDER`。 |

---

## 5. 显示数量约定

`data_get_display_count()` 是 UI 判断网格显示行数的关键函数，建议数据层按以下规则实现：

1. 当前没有筛选且没有学生数据时：
   - 返回 `200`，用于显示默认 200 行空网格；
2. 当前没有筛选且已有学生数据时：
   - 返回学生总数；
3. 当前筛选生效时：
   - 返回筛选命中的行数；
4. 搜索只负责高亮，不改变 `data_get_display_count()`。

`data_get_display_index(display_row)` 示例约定：

- `display_row = 0` 表示表格中第 2 行（第 1 行是表头）
- 若该行有数据，返回对应 `StudentCSV` 数组下标
- 若该行为空，返回 `-1`

---

## 6. 后续集成方式

### 6.1 调用 UI

```c
#include "ui.h"

/* 初始化 LVGL 和显示后 */
ui_create(lv_screen_active());

/* 若使用 Win32 keypad 输入设备，则绑定键盘分组 */
lv_indev_t *kb = lv_win32_get_keypad_indev();
if(kb) ui_bind_keyboard(kb);
```

### 6.2 刷新表格

导入、删除、筛选、编辑保存后，UI 内部会自动调用刷新函数；也可以手动调用：

```c
ui_refresh_grid();
```

### 6.3 编译提醒

- 需要把 `ui.c` 加入编译源码列表；
- include path 需要包含工作区根目录，以便找到 `ui.h`、`read_students_csv.h`、`write_students_csv.h`；
- include path 需要包含 LVGL 项目目录和 `lvgl` 源码目录，以便找到 `lvgl/lvgl.h`；
- 链接前必须实现第 4 节列出的外部数据层函数，否则会报未定义引用。
