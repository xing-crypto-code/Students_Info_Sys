/**
 * @file stu.h
 * @brief 学生成绩信息管理系统 —— 数据层（后端）头文件。
 *
 * 本文件包含两类接口：
 *
 * 【第一类】基础数据管理接口（“四、基础数据管理接口”）
 *   stuInit / addStudent / deleteStudent / sortStudents / saveToFile ...
 *   供控制台菜单（console.c）、文件模块（file.c）以及数据层内部调用。
 *
 * 【第二类】UI 数据层接口（“五、UI 数据层接口”）
 *   data_* / ui_help_* 系列函数。
 *   函数名称、参数、返回值严格按照《UI函数需求文档.md》第 4 节
 *   “外部数据层函数需求”定义，供 LVGL 前端（ui.c）调用。
 *
 * 设计约定：
 *   1. 本文件不包含任何 LVGL 头文件，因此数据层可以在纯控制台环境中
 *      单独编译、单独测试；
 *   2. UI 接口中的 field 参数取 stu_field_t，其数值与 ui.h 中的
 *      ui_field_t 完全一致（0=年级 1=班级 2=学号 3=姓名 4=性别
 *      5=分数 6=绩点 7=排名），两者可直接互相传递；
 *   3. 单个字段的字符串长度上限为 MAX_FIELD_LEN，CSV 单行长度上限为 MAX_LINE_LEN。
 */

#ifndef STU_H
#define STU_H

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

/* ==================================================================
 * 一、容量与长度常量
 * ================================================================== */

#define MAX_STUDENTS   200    /* 内存中学生数组最大容量，也是网格默认最大行数 */
#define MAX_STUDENT    MAX_STUDENTS   /* 兼容旧名称 */
#define MAX_FIELD_LEN  64     /* 单个字段（年级/班级/姓名/筛选选项）最大字符数 */
#define MAX_LINE_LEN   1024   /* 一行 CSV 文本的最大长度 */

/**
 * 未筛选且没有任何学生时，data_get_display_count() 返回的空网格行数。
 * 该值等于 ui.h 中的 UI_MAX_DATA_ROWS，保持两边一致即可显示默认 200 行空网格。
 */
#define DATA_DEFAULT_DISPLAY_ROWS 200

/**
 * 帮助图片总张数（对应 A:help/help1.png ~ helpN.png）。
 * 实际图片数量变化时，只需修改这个宏即可，UI 会自动按数量切换。
 */
#define DATA_HELP_IMAGE_COUNT 5

/**
 * 帮助图片所在目录，与 ui.h 中的 UI_HELP_IMAGE_FOLDER 保持一致。
 * 路径前缀 "A:" 由 LVGL 文件系统（如 LV_FS_STDIO_LETTER）决定。
 */
#define DATA_HELP_IMAGE_FOLDER "A:help/"

/* ==================================================================
 * 二、学生数据结构
 * ================================================================== */

/**
 * @brief 与 students.csv 列一一对应的学生结构体。
 *
 * CSV 列顺序：年级,班级,学号,姓名,性别,分数,绩点,排名
 *
 * 说明：
 *   - 性别使用 bool 表示：true = 男，false = 女，读写 CSV 时自动转换；
 *   - rank（排名）属于派生数据，通常在导入/删除/修改分数后由
 *     recalculate_ranking_by_score() 按分数重新计算并覆盖。
 */
typedef struct {
    char     grade[MAX_FIELD_LEN];       /* 年级，例如 "2023级" */
    char     class_name[MAX_FIELD_LEN];  /* 班级，例如 "计算机1班" */
    uint64_t id;                         /* 学号 */
    char     name[MAX_FIELD_LEN];        /* 姓名 */
    bool     gender;                     /* 性别：true=男，false=女 */
    float    score;                      /* 分数，0.0 ~ 100.0 */
    float    gpa;                        /* 绩点，0.0 ~ 4.0 */
    uint16_t rank;                       /* 排名，从 1 开始 */
} StudentCSV;

/* 兼容旧代码中使用的别名 */
typedef StudentCSV Student;

/* ==================================================================
 * 三、字段编号（数值与 ui.h 中的 ui_field_t 完全一致）
 * ================================================================== */

/**
 * @brief 表格第 2~9 列对应的字段编号。
 *
 * UI 层会把 ui_field_t 直接传给本层的排序 / 筛选 / 修改函数，
 * 因此这里的数值必须与 ui_field_t 一一对应，两者可以互换使用。
 */
typedef enum {
    STU_FIELD_GRADE  = 0,   /* 年级   */
    STU_FIELD_CLASS  = 1,   /* 班级   */
    STU_FIELD_ID     = 2,   /* 学号   */
    STU_FIELD_NAME   = 3,   /* 姓名   */
    STU_FIELD_GENDER = 4,   /* 性别   */
    STU_FIELD_SCORE  = 5,   /* 分数   */
    STU_FIELD_GPA    = 6,   /* 绩点   */
    STU_FIELD_RANK   = 7,   /* 排名   */
    STU_FIELD_COUNT  = 8    /* 字段总数，用于校验 field 是否合法 */
} stu_field_t;

/* ==================================================================
 * 四、基础数据管理接口（供 console.c / file.c 及数据层内部使用）
 * ================================================================== */

/** 清空内存中的学生数据，并复位筛选状态与显示行映射。 */
void stuInit(void);

/**
 * 追加一名学生到数组末尾。
 * @return 1 成功；0 失败（学号或姓名为空、分数/绩点越界、
 *         数组已满、学号与已有数据重复）。
 */
int addStudent(const StudentCSV *student);

/**
 * 按学号删除一名学生，并把后面的元素整体前移一位。
 * @return 1 成功；0 失败（未找到该学号）。
 */
int deleteStudent(uint64_t id);

/** 获取内存中学生总数（未筛选状态）。 */
int getStudentCount(void);

/** 获取学生结构体数组首地址，数组长度等于 getStudentCount()。 */
StudentCSV *getStudentArray(void);

/**
 * 按内存数组下标取得学生指针。
 * @return 下标越界时返回 NULL。
 */
StudentCSV *getStudent(int index);

/**
 * 按学号查找学生。
 * @return 找到返回学生指针，未找到返回 NULL。
 */
StudentCSV *findById(uint64_t id);

/**
 * 按姓名精确查找，可一次返回多条结果。
 * @param result    调用者提供的接收数组
 * @param max_count 接收数组容量
 * @return 实际匹配人数（可能大于 max_count，此时只有前 max_count 条写入 result）。
 */
int findByName(const char *name, StudentCSV result[], int max_count);

/**
 * 按学号整体替换一名学生的信息。
 * 学号以参数 id 为准，不会被 new_info->id 覆盖。
 * @return 1 成功；0 失败（未找到学生或新数据非法）。
 */
int modifyStudent(uint64_t id, const StudentCSV *new_info);

/**
 * 用新数组整体替换内存中的学生数据。
 * @param source 源数组；当 count 为 0 时允许传 NULL（表示清空数据）
 * @param count  元素个数，取值必须在 0 ~ MAX_STUDENTS 之间
 * @return 1 成功；0 参数非法。
 */
int replaceStudents(const StudentCSV *source, int count);

/**
 * 按字段对内存中的学生数组排序（qsort，直接改变数组物理顺序）。
 *
 * 排序规则：
 *   - 含字符的字段（年级/班级/姓名）：先比较字符串中的数字部分，
 *     数字相同时再按普通字符串比较，例如 “计算机2班” 排在 “计算机10班” 之前；
 *   - 纯数字字段（学号/分数/绩点/排名）：按数值大小比较，而不是字符串比较；
 *   - bool 型字段（性别）：true(男) 视为 1、false(女) 视为 0，正序时男在前。
 *
 * @param field     字段编号（stu_field_t，0~7）
 * @param ascending true = 正序（升序），false = 倒序（降序）
 */
void sortStudents(int field, bool ascending);

/**
 * 统计分数信息。
 * @param average    输出平均分
 * @param highest    输出最高分
 * @param lowest     输出最低分
 * @param pass_count 输出及格（>= 60 分）人数
 * @return 1 成功；0 失败（无学生数据或任一输出指针为 NULL）。
 */
int getStatistics(float *average, float *highest, float *lowest, int *pass_count);

/* 下面两个函数由 file.c 实现，提供基于 CSV 文件的整体存/取 */
int saveToFile(const char *filename);   /* 保存成功返回 1，失败返回 0 */
int loadFromFile(const char *filename); /* 读取成功返回 1，失败返回 0 */

/* ==================================================================
 * 五、UI 数据层接口（《UI函数需求文档.md》第 4 节）
 *
 * 以下函数由 LVGL 前端 ui.c 调用，全部在 stu.c 中实现。
 * 函数名称与参数不可随意更改，否则 ui.c 会链接失败。
 * ================================================================== */

/* ------------------------- 4.1 数据访问 ------------------------- */

/**
 * 获取当前内存中学生总数（未筛选状态）。
 * @return 学生人数（无数据时为 0）。
 */
int data_get_count(void);

/**
 * 返回学生结构体数组首地址，数组长度为 data_get_count()。
 * @return 数组首地址；没有学生时仍返回有效数组（内容为空）。
 */
StudentCSV *data_get_all(void);

/**
 * 返回网格当前应显示的数据行数，规则见《UI函数需求文档.md》第 5 节：
 *   1. 未筛选且没有学生数据：返回 DATA_DEFAULT_DISPLAY_ROWS（200），显示空网格；
 *   2. 未筛选且已有学生数据：返回学生总数；
 *   3. 筛选生效时：返回筛选命中的行数（可能为 0）；
 *   4. 搜索只负责高亮，不会改变本函数返回值。
 */
int data_get_display_count(void);

/**
 * 把“显示行号”映射为内存数组下标。
 * @param display_row 显示行号，从 0 开始；0 表示表格第 2 行（第 1 行是表头）。
 * @return 对应的 StudentCSV 数组下标；该行为空（超出显示行数）时返回 -1。
 */
int data_get_display_index(int display_row);

/* ---------------------- 4.2 导入 / 导出 ---------------------- */

/**
 * 从指定 CSV 路径导入数据，覆盖内存中的全部学生数据。
 * 导入成功后会自动：
 *   1. 按分数重新计算排名；
 *   2. 清除已有筛选条件；
 *   3. 重建显示行映射。
 * @param path CSV 文件路径
 * @return 成功返回导入条数（可能为 0）；路径为空或读取失败返回 -1。
 */
int data_import_csv(const char *path);

/**
 * 把内存中的全部学生数据写成 CSV 文件。
 * 注意：导出的是内存中的全部数据，与当前筛选状态无关
 * （筛选 / 搜索只影响界面显示，不影响导出内容）。
 * @param path 目标文件路径
 * @return 成功返回导出条数；路径为空或写入失败返回 -1。
 */
int data_export_csv(const char *path);

/* ---------------------- 4.3 删除 / 修改 ---------------------- */

/**
 * 按内存数组下标删除一名学生，并把后面的元素前移一位。
 * 删除成功后按分数重新计算排名，并重建显示行映射。
 * @param index 学生数组下标（不是显示行号，需用 data_get_display_index() 换算）
 * @return 1 成功；0 失败（下标越界）。
 */
int data_delete_student(int index);

/**
 * 修改指定学生的某个字段：由数据层把文本解析成正确类型后写回。
 *
 * 解析规则：
 *   - 年级 / 班级 / 姓名：直接作为字符串写入；
 *   - 学号：非负整数，且必须全是数字；
 *   - 性别：“男/M/m/1” → true，“女/F/f/0” → false；
 *   - 分数：浮点数，范围 0.0 ~ 100.0（修改后会自动重算排名）；
 *   - 绩点：浮点数，范围 0.0 ~ 4.0；
 *   - 排名：0 ~ 65535 的整数。
 *
 * @param index 学生数组下标
 * @param field 字段编号（stu_field_t / ui_field_t，0~7）
 * @param text  用户输入的新内容文本
 * @return 1 成功；0 失败（下标越界、文本为空或解析/范围校验失败）。
 */
int data_update_student_field(int index, int field, const char *text);

/* ------------------------- 4.4 筛选 ------------------------- */

/**
 * 获取某个字段可用于筛选的“不同选项”。
 *
 * 规则：
 *   - 年级 / 班级：返回数据中出现过的不同取值；
 *   - 性别：返回 "男" / "女" 两种取值；
 *   - 分数：按每 10.0 分一段生成区间文本，例如 "0-10"、"80-90"、"100-110"；
 *   - 其余字段（学号/姓名/绩点/排名）不支持筛选，直接返回 0。
 * 返回的选项按“数字部分优先”的自然顺序升序排列，便于下拉列表展示。
 *
 * @param field    字段编号（stu_field_t）
 * @param options  输出缓冲区，形如 char options[N][MAX_FIELD_LEN]
 * @param max_opts 缓冲区最多可以存放多少个选项
 * @return 实际选项个数（可能大于 max_opts，此时只写入前 max_opts 个）。
 */
int data_get_filter_options(int field, char options[][MAX_FIELD_LEN], int max_opts);

/**
 * 按字段 + 选项应用筛选。
 * 调用后 data_get_display_count() 与 data_get_display_index() 立即反映筛选结果。
 * @param field  仅支持 STU_FIELD_GRADE / CLASS / GENDER / SCORE
 * @param option 选项文本，通常来自 data_get_filter_options()
 * @return 1 成功；0 失败（字段不支持筛选或选项为空）。
 */
int data_apply_filter(int field, const char *option);

/** 清除当前筛选条件，恢复显示全部数据。 */
void data_clear_filter(void);

/* ------------------------- 4.5 搜索 ------------------------- */

/**
 * 在当前筛选结果（显示行）中搜索关键字。
 * 搜索只用于界面高亮：既不会改变筛选状态，也不会改变显示行数。
 * 匹配范围为 8 个数据列：年级、班级、学号、姓名、性别、分数、绩点、排名。
 *
 * @param keyword  搜索关键字，允许为空串（此时匹配所有显示行）
 * @param out_rows 输出匹配到的“显示行号”数组（从 0 开始）
 * @param max_rows out_rows 的容量
 * @return 匹配数量（可能大于 max_rows，此时只写入前 max_rows 个）。
 */
int data_find_matches(const char *keyword, int *out_rows, int max_rows);

/* ------------------------- 4.6 排序 ------------------------- */

/**
 * 按指定字段对学生数组排序，并刷新显示行映射。
 *
 * 排序规则约定：
 *   - 含字符的字段（年级、班级、姓名等混合数字/文字）：
 *     优先提取字符串中的数字部分按数值排序，数字相同时再按普通字符串排序，
 *     例如 “计算机2班” 排在 “计算机10班” 前面；
 *   - 纯数字字段（学号、分数、绩点、排名）：按数值大小排序，而不是字符串排序；
 *   - bool 型字段（性别）：true 视为 1、false 视为 0；
 *     正序按 1 > 0（“男”排在“女”前面），倒序按 0 > 1（“女”排在“男”前面）。
 *
 * @param field     字段编号（stu_field_t / ui_field_t，0~7）
 * @param ascending true = 正序，false = 倒序
 */
void data_sort_students(int field, bool ascending);

/* ---------------------- 4.7 帮助图片 ---------------------- */

/**
 * 返回帮助图片总张数。
 * UI 每点击一次帮助按钮就切换到下一张，切到最后一张后再点击会关闭帮助图片。
 * @return 帮助图片数量（由 DATA_HELP_IMAGE_COUNT 决定）。
 */
int ui_help_get_image_count(void);

/**
 * 返回第 index 张帮助图片源，可直接交给 lv_imagebutton_set_src() 使用。
 *
 * 实现约定：
 *   - 返回稳定的静态路径字符串 "A:help/helpN.png"（N 从 1 开始），
 *     指针在程序运行期间一直有效，UI 层可以放心保存；
 *   - 图片能否真正显示取决于 LVGL 文件系统（LV_USE_FS_STDIO 等）
 *     以及 PNG 解码器是否开启、文件是否存在。
 *
 * @param index 图片序号，从 0 开始
 * @return 图片源指针；index 越界时返回 NULL。
 */
const void *ui_help_get_image_src(int index);

#endif