# core.c / core.h 函数分类说明

> 文件位置：
>
> - `src/core/core.h`
> - `src/core/core.c`
>
> 本文按功能分类说明 core 层的函数、原型、可见范围和大致定义位置。

---

## 一、公共数据接口（core.h 对外暴露，ui.c 调用）

这些函数是 UI 层唯一直接调用的数据层接口。

### 1. 数据访问

| 函数 | 原型 | 功能说明 | 定义位置 |
| --- | --- | --- | --- |
| `data_get_count` | `int data_get_count(void);` | 获取当前学生总数（未筛选） | core.h:102 / core.c:766 |
| `data_get_all` | `StudentCSV* data_get_all(void);` | 获取学生结构体数组首地址 | core.h:105 / core.c:773 |
| `data_get_display_count` | `int data_get_display_count(void);` | 获取当前网格应显示的行数；无数据无筛选返回 200 | core.h:113 / core.c:780 |
| `data_get_display_index` | `int data_get_display_index(int display_row);` | 把显示行号映射为真实学生数组下标 | core.h:119 / core.c:790 |

### 2. CSV 导入 / 导出

| 函数 | 原型 | 功能说明 | 定义位置 |
| --- | --- | --- | --- |
| `data_import_csv` | `int data_import_csv(const char* path);` | 从 CSV 导入数据，覆盖内存数组；成功返回条数，失败返回 -1 | core.h:127 / core.c:801 |
| `data_export_csv` | `int data_export_csv(const char* path);` | 把当前学生数据导出为 CSV；成功返回条数，失败返回 -1 | core.h:133 / core.c:827 |

### 3. 删除 / 修改

| 函数 | 原型 | 功能说明 | 定义位置 |
| --- | --- | --- | --- |
| `data_delete_student` | `int data_delete_student(int index);` | 按下标删除学生，后续元素前移 | core.h:141 / core.c:838 |
| `data_update_student_field` | `int data_update_student_field(int index, int field, const char* text);` | 修改指定学生指定字段，并解析为正确类型 | core.h:150 / core.c:860 |

### 4. 筛选

| 函数 | 原型 | 功能说明 | 定义位置 |
| --- | --- | --- | --- |
| `data_get_filter_options` | `int data_get_filter_options(int field, char options[][MAX_FIELD_LEN], int max_opts);` | 获取年级/班级/性别/分数筛选选项 | core.h:162 / core.c:937 |
| `data_apply_filter` | `int data_apply_filter(int field, const char* option);` | 应用筛选条件 | core.h:168 / core.c:996 |
| `data_clear_filter` | `void data_clear_filter(void);` | 清除筛选，恢复显示全部数据 | core.h:171 / core.c:1011 |

### 5. 搜索

| 函数 | 原型 | 功能说明 | 定义位置 |
| --- | --- | --- | --- |
| `data_find_matches` | `int data_find_matches(const char* keyword, int* out_rows, int max_rows);` | 在当前显示结果中搜索关键字，返回匹配显示行号 | core.h:183 / core.c:1028 |

### 6. 排序

| 函数 | 原型 | 功能说明 | 定义位置 |
| --- | --- | --- | --- |
| `data_sort_students` | `void data_sort_students(int field, bool ascending);` | 按字段排序学生数组，true=正序，false=倒序 | core.h:195 / core.c:1060 |

### 7. 帮助图片

| 函数 | 原型 | 功能说明 | 定义位置 |
| --- | --- | --- | --- |
| `ui_help_get_image_count` | `int ui_help_get_image_count(void);` | 返回帮助图片张数 | core.h:200 / core.c:1079 |
| `ui_help_get_image_src` | `const void* ui_help_get_image_src(int index);` | 返回第 index 张帮助图片路径 | core.h:207 / core.c:1085 |

---

## 二、内部工具函数（core.c 内部 static 函数）

这些函数不对外暴露，仅供 core.c 内部使用。

### 1. 字符串 / 文本工具

| 函数 | 原型 | 功能说明 | 定义位置 |
| --- | --- | --- | --- |
| `core_trim_whitespace` | `static void core_trim_whitespace(char* text);` | 去掉字符串首尾空白 | core.c:67 |
| `core_remove_utf8_bom` | `static void core_remove_utf8_bom(char* text);` | 去掉 UTF-8 BOM | core.c:98 |
| `core_copy_trimmed` | `static void core_copy_trimmed(char* dst, size_t dst_size, const char* src);` | 安全复制字符串并去首尾空白 | core.c:113 |
| `core_contains_ci` | `static int core_contains_ci(const char* haystack, const char* needle);` | 判断是否包含关键字，ASCII 不区分大小写 | core.c:146 |
| `core_compare_text_natural` | `static int core_compare_text_natural(const char* left, const char* right);` | 自然字符串比较，数字部分按数值比较 | core.c:178 |
| `core_field_text` | `static const char* core_field_text(const StudentCSV* student, int field, char* buf, size_t size);` | 把学生某个字段格式化成字符串 | core.c:317 |

### 2. 文本解析

| 函数 | 原型 | 功能说明 | 定义位置 |
| --- | --- | --- | --- |
| `core_parse_uint64_text` | `static int core_parse_uint64_text(const char* text, uint64_t* out);` | 解析完整非负整数文本 | core.c:221 |
| `core_parse_float_text` | `static int core_parse_float_text(const char* text, float* out);` | 解析完整浮点数文本 | core.c:245 |
| `core_parse_gender_text` | `static int core_parse_gender_text(const char* text, bool* out);` | 解析性别（男/女、M/F、1/0） | core.c:270 |

### 3. CSV 解析与读写

| 函数 | 原型 | 功能说明 | 定义位置 |
| --- | --- | --- | --- |
| `core_split_csv_line` | `static int core_split_csv_line(char* line, char** fields, int max_fields);` | 按逗号拆一行 CSV | core.c:372 |
| `core_parse_csv_student` | `static int core_parse_csv_student(char* line, StudentCSV* student);` | 解析一行 CSV 为学生结构体 | core.c:405 |
| `core_read_csv` | `static int core_read_csv(const char* filename, StudentCSV* out, int capacity, char* header, size_t header_size);` | 从 CSV 文件读取学生数据 | core.c:461 |
| `core_write_csv` | `static int core_write_csv(const char* filename, const StudentCSV* students, int count, const char* header);` | 把学生数组写到 CSV 文件 | core.c:507 |

### 4. 排序与排名

| 函数 | 原型 | 功能说明 | 定义位置 |
| --- | --- | --- | --- |
| `core_cmp_score_desc` | `static int core_cmp_score_desc(const void* a, const void* b);` | qsort 比较函数：分数降序 | core.c:550 |
| `core_recalc_rank` | `static void core_recalc_rank(StudentCSV* students, int count);` | 按分数降序重新计算排名 | core.c:565 |
| `core_compare_students` | `static int core_compare_students(const void* left_ptr, const void* right_ptr);` | qsort 通用学生比较函数 | core.c:583 |

### 5. 筛选与显示映射

| 函数 | 原型 | 功能说明 | 定义位置 |
| --- | --- | --- | --- |
| `core_filter_active` | `static int core_filter_active(void);` | 判断当前是否有有效筛选 | core.c:626 |
| `core_score_range_text` | `static void core_score_range_text(float score, char* buf, size_t buf_size);` | 把分数转为每 10 分一段的区间文本 | core.c:635 |
| `core_score_match` | `static int core_score_match(float score, const char* option);` | 判断分数是否落在筛选区间 | core.c:652 |
| `core_match_filter` | `static int core_match_filter(const StudentCSV* student);` | 判断学生是否满足当前筛选条件 | core.c:667 |
| `core_rebuild_display` | `static void core_rebuild_display(void);` | 重建“显示行 -> 数组下标”映射 | core.c:700 |

### 6. 初始化 / 默认加载

| 函数 | 原型 | 功能说明 | 定义位置 |
| --- | --- | --- | --- |
| `data_load_default` | `static void data_load_default(void);` | 首次启动时尝试加载默认 CSV | core.c:721 |
| `data_ensure_initialized` | `static void data_ensure_initialized(void);` | 保证数据已初始化，供所有 data_* 接口调用 | core.c:754 |

---

## 三、core.h 中的类型与常量

| 名称 | 类型/值 | 说明 |
| --- | --- | --- |
| `MAX_STUDENTS` | `200` | 学生数组最大容量 |
| `MAX_FIELD_LEN` | `64` | 单个字段最大长度 |
| `MAX_LINE_LEN` | `1024` | CSV 单行最大长度 |
| `DATA_DEFAULT_DISPLAY_ROWS` | `200` | 无数据时网格默认显示行数 |
| `STUDENT_CSV_HEADER` | `"年级,班级,学号,姓名,性别,分数,绩点,排名"` | 默认 CSV 表头 |
| `CORE_HELP_IMAGE_FOLDER` | `"A:help_png/"` | 帮助图片目录 |
| `CORE_HELP_IMAGE_COUNT` | `1` | 帮助图片数量 |
| `StudentCSV` | `struct` | 学生结构体 |
| `stu_field_t` | `enum` | 字段编号枚举 |

---

## 四、调用关系简图

```text
ui.c
  │
  │ 调用公共接口
  ▼
data_get_count / data_get_all
data_get_display_count / data_get_display_index
data_import_csv / data_export_csv
data_delete_student / data_update_student_field
data_get_filter_options / data_apply_filter / data_clear_filter
data_find_matches
data_sort_students
ui_help_get_image_count / ui_help_get_image_src

core.c 内部
  │
  ├── CSV 读写
  │     core_read_csv / core_write_csv
  │        ├── core_split_csv_line
  │        └── core_parse_csv_student
  │
  ├── 筛选
  │     core_match_filter
  │        ├── core_filter_active
  │        ├── core_score_range_text
  │        └── core_score_match
  │
  ├── 排序
  │     core_compare_students
  │     core_recalc_rank
  │
  └── 显示映射
        core_rebuild_display
```
