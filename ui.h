/**
 * @file ui.h
 * @brief 学生成绩信息管理系统 —— LVGL v9.5 UI 框架层头文件。
 *
 * 本文件只描述 UI 框架对外的接口，以及 UI 框架需要外部数据层提供的函数。
 * 数据层函数（search / filter / import / export / delete / update 等）
 * 不在这里实现，已按《UI函数需求文档.md》第 4 节在 Src/stu.c 中实现。
 */

#ifndef UI_H
#define UI_H

#include <stddef.h>

/* LVGL v9.5 主头文件 */
#include "lvgl/lvgl.h"

/* 工作区已提供的学生 CSV 结构体与 CSV 读写接口 */
#include "read_students_csv.h"
#include "write_students_csv.h"

#ifdef __cplusplus
extern "C" {
#endif

/* ======================== 尺寸常量 ======================== */

#define UI_SCR_W              1080   /* 窗口/屏幕宽 */
#define UI_SCR_H              720    /* 窗口/屏幕高 */
#define UI_TOP_H              80     /* 顶部工具栏高 */
#define UI_LIST_W             1080   /* 下方表格区域宽 */
#define UI_LIST_H             560    /* 下方表格区域高 */
#define UI_LIST_X             0
#define UI_LIST_Y             100

#define UI_MAX_DATA_ROWS      200    /* 默认最多显示 200 行学生数据 */
#define UI_COL_NUM            9      /* 第1列序号 + 8列数据 */
#define UI_ROW_H              50     /* 固定行高 */
#define UI_DEFAULT_COL_W      120    /* 无数据时列宽默认值 */

#define UI_TOP_BTN_Y          10
#define UI_TOP_BTN_H          60
#define UI_BTN_GAP            10
#define UI_SEARCH_W           300
#define UI_SMALL_BTN_W        60

/* ======================== 图标配置 ======================== */

/**
 * 图标文件夹地址。
 *
 * 这里默认使用 LVGL 文件系统路径 "A:icons/"。
 * 实际使用时请把该宏改成你的图标文件夹：
 *   - 如果使用 LVGL stdio 文件系统：
 *       A: 对应 lv_conf.h 中的 LV_FS_STDIO_LETTER；
 *       LV_FS_STDIO_PATH 是 A: 映射的物理目录；
 *       例如图标在 C:/my_app/icons 下，则通常可写 "A:icons/"。
 *   - 如果不想启用文件系统，也可以改成完整物理路径
 *       "C:/my_app/icons/"（前提是你的 LVGL 工程已支持直接读路径）。
 */
#define UI_ICON_FOLDER "A:icons/"

/* 顶部按钮/搜索框图标文件名 */
#define UI_ICON_SEARCH  "search.png"
#define UI_ICON_FILTER  "filter.png"
#define UI_ICON_HELP    "help.png"
#define UI_ICON_IMPORT  "import.png"
#define UI_ICON_EXPORT  "export.png"
#define UI_ICON_DELETE  "delete.png"

/* 表头排序按钮图标文件名（四个排序按钮共用同一个图标） */
#define UI_ICON_SORT    "sort.png"

/**
 * 帮助图片目录地址。
 * ui.c 不直接读取帮助图片，而是调用外部函数 ui_help_get_image_src(index)；
 * 外部实现该函数时，可在此处修改帮助图片所在目录。
 */
#define UI_HELP_IMAGE_FOLDER "A:help/"

/* ======================== 字段/筛选枚举 ======================== */

/**
 * 表格第 2~9 列对应的学生结构体字段。
 * 注意：第 1 列是序号，不属于 StudentCSV。
 */
typedef enum {
    UI_FIELD_GRADE  = 0,   /* 年级   */
    UI_FIELD_CLASS  = 1,   /* 班级   */
    UI_FIELD_ID     = 2,   /* 学号   */
    UI_FIELD_NAME   = 3,   /* 姓名   */
    UI_FIELD_GENDER = 4,   /* 性别   */
    UI_FIELD_SCORE  = 5,   /* 分数   */
    UI_FIELD_GPA    = 6,   /* 绩点   */
    UI_FIELD_RANK   = 7,   /* 排名   */
    UI_FIELD_COUNT  = 8
} ui_field_t;

/**
 * 筛选第一级列表中的类别。
 */
typedef enum {
    UI_FILTER_GRADE  = 0,
    UI_FILTER_CLASS  = 1,
    UI_FILTER_GENDER = 2,
    UI_FILTER_SCORE  = 3,
    UI_FILTER_ALL    = 4
} ui_filter_kind_t;

/* ======================== UI 框架对外接口 ======================== */

/**
 * 创建完整 UI。
 * @param scr 通常传 lv_screen_active()；会在其上创建顶部工具栏和下方表格。
 */
void ui_create(lv_obj_t * scr);

/**
 * 刷新下方网格/表格：重新从数据层读取当前可见行，重设列宽和单元格文本。
 * 导入、删除、筛选、编辑保存后都应调用。
 */
void ui_refresh_grid(void);

/**
 * 将内部键盘分组绑定到移植层返回的 keypad indev。
 * 如果项目没有调用本函数，请确保外部把文本框加入合适的 group 并绑定。
 */
void ui_bind_keyboard(lv_indev_t * kb);

/* ======================== 外部数据层函数需求 ========================
 *
 * 以下函数不属于 UI 框架，UI 只调用不实现。这些函数已按本文档第 4 节的
 * 确定名称与功能在 Src/stu.c 中实现（原 lvgl/student_data.c 已作废）。
 * ================================================================= */

/**
 * 获取当前学生总数（未筛选时内存中的学生个数）。
 */
int data_get_count(void);

/**
 * 获取学生结构体数组首地址。
 * 返回数组长度为 data_get_count()，按 StudentCSV 顺序存放。
 */
StudentCSV * data_get_all(void);

/**
 * 获取网格当前应显示的数据行数。
 *
 * 规则约定：
 *   - 未筛选、无数据时返回 UI_MAX_DATA_ROWS（200），用于初始空表格；
 *   - 未筛选、有数据时返回学生总数；
 *   - 筛选生效时返回当前筛选命中的行数；
 *   - 搜索只负责高亮，不改变该返回值。
 */
int data_get_display_count(void);

/**
 * 获取“显示行号”（从 0 开始的数据行）对应的学生数组下标。
 * @param display_row 0 表示表格中第 2 行（第一行是表头）。
 * @return 对应 StudentCSV 数组下标；若该行为空则返回 -1。
 */
int data_get_display_index(int display_row);

/**
 * 从用户输入的路径导入 CSV。
 * 内部应更新内存中的结构体数组/总数，并可在导入后重算排名。
 * @return 成功返回导入条数；失败返回 -1。
 */
int data_import_csv(const char * path);

/**
 * 把内存中当前学生数据导出为 CSV。
 * @return 成功返回导出条数；失败返回 -1。
 */
int data_export_csv(const char * path);

/**
 * 按学生数组下标删除一名学生，并把后面元素前移一位。
 * @return 1 成功，0 失败。
 */
int data_delete_student(int index);

/**
 * 修改某个学生的某个字段。
 * @param index 学生数组下标
 * @param field 使用 ui_field_t（0~7）
 * @param text  新的文本；由数据层负责解析为 int/float/bool/字符串。
 * @return 1 成功，0 失败。
 */
int data_update_student_field(int index, int field, const char * text);

/**
 * 获取某个字段可用于筛选的“不同选项”。
 * @param field    ui_field_t，用于年级/班级/性别/分数。
 * @param options  输出缓冲区，二维数组。
 * @param max_opts 缓冲区最多可存多少项。
 * @return 选项个数；分数段按每 10.0 分一段生成。
 */
int data_get_filter_options(int field, char options[][MAX_FIELD_LEN], int max_opts);

/**
 * 应用筛选：只保留 field 等于 option 的数据（分数段为区间判断）。
 * 调用后 data_get_display_count()/data_get_display_index() 应反映筛选结果。
 * @return 1 成功，0 失败。
 */
int data_apply_filter(int field, const char * option);

/**
 * 清除筛选，恢复显示全部数据。
 */
void data_clear_filter(void);

/**
 * 在当前筛选结果中搜索关键字。
 * @param keyword   搜索关键字（可为空串）。
 * @param out_rows  输出匹配的显示行号数组（0 开始的数据行）。
 * @param max_rows  out_rows 容量。
 * @return 匹配行数。
 */
int data_find_matches(const char * keyword, int * out_rows, int max_rows);

/**
 * 对学生数组按指定字段排序。
 * @param field     使用 ui_field_t（0~7）。
 * @param ascending true=正序，false=倒序。
 *
 * 排序规则约定：
 *   - 含字符的字符串字段：先按其中的数字部分数值排序，再按普通字符串排序；
 *   - 纯数字字段：按数值大小排序；
 *   - bool 型字段：正序按 1 > 0 排序（即 true 排在 false 前面）。
 */
void data_sort_students(int field, bool ascending);

/**
 * 帮助图片数量。UI 点击“帮助”后按顺序切换这些图片。
 */
int ui_help_get_image_count(void);

/**
 * 获取第 index 张帮助图片源（可用于 lv_imagebutton_set_src 的 src_mid）。
 * index 从 0 开始。
 */
const void * ui_help_get_image_src(int index);

#ifdef __cplusplus
}
#endif

#endif /* UI_H */
