/**
 * @file core.h
 * @brief 学生成绩信息管理系统 —— 数据核心层头文件（纯 C，不依赖 LVGL）。
 *
 * 整理后的分层结构：
 *   ui.c / ui.h       LVGL 图形界面层，只负责显示和交互；
 *   core.c / core.h   数据核心层，负责结构体数组、CSV 读写、筛选、搜索、排序。
 *
 * ui.c 只调用本文件声明的 data_* / ui_help_* 函数，不直接操作学生数组。
 */

#ifndef CORE_H
#define CORE_H

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

 /* ==================================================================
  * 一、容量与长度常量
  * ================================================================== */

  /** 学生数组最大容量，也是表格默认最多显示的数据行数。 */
#define MAX_STUDENTS   200

/** 单个字段（年级/班级/姓名等）最大字节数，含字符串结束符 '\0'。 */
#define MAX_FIELD_LEN  64

/** CSV 单行文本最大长度。 */
#define MAX_LINE_LEN   1024

/** 未筛选且没有任何学生时，网格默认显示的空行数。 */
#define DATA_DEFAULT_DISPLAY_ROWS 200

/** 默认 CSV 表头。 */
#define STUDENT_CSV_HEADER "年级,班级,学号,姓名,性别,分数,绩点,排名"

/* ==================================================================
 * 二、帮助图片配置
 * ================================================================== */

 /**
  * 帮助图片所在目录。
  * 默认使用 LVGL 文件系统路径，A: 对应 LV_FS_STDIO_LETTER 等驱动字母。
  * 如果图片放在其他目录，只需要修改这个宏。
  */
#define CORE_HELP_IMAGE_FOLDER "A:help_png/"

  /** 帮助图片总数；图片实际数量变化时修改这个宏即可。 */
#define CORE_HELP_IMAGE_COUNT  1

/* ==================================================================
 * 三、学生数据结构
 * ================================================================== */

 /**
  * @brief 与学生 CSV 的 8 个数据列一一对应的学生结构体。
  *
  * CSV 列顺序：
  *   年级,班级,学号,姓名,性别,分数,绩点,排名
  */
typedef struct {
  char     grade[MAX_FIELD_LEN];       /* 年级，例如 "2023级" */
  char     class_name[MAX_FIELD_LEN];  /* 班级，例如 "计算机1班" */
  uint64_t id;                         /* 学号 */
  char     name[MAX_FIELD_LEN];        /* 姓名 */
  bool     gender;                     /* 性别：true=男，false=女 */
  float    score;                      /* 分数（0.0 ~ 100.0） */
  float    gpa;                        /* 绩点（0.0 ~ 4.0） */
  uint16_t rank;                       /* 排名（从 1 开始） */
} StudentCSV;

/* ==================================================================
 * 四、字段编号
 * ================================================================== */

 /**
  * @brief 表格第 2~9 列对应的字段编号。
  *
  * 数值必须与 ui.h 中的 ui_field_t 保持一致，UI 层会把 ui_field_t
  * 直接传给 data_sort_students()、data_apply_filter() 等函数。
  */
typedef enum {
  STU_FIELD_GRADE = 0,   /* 年级   */
  STU_FIELD_CLASS = 1,   /* 班级   */
  STU_FIELD_ID = 2,   /* 学号   */
  STU_FIELD_NAME = 3,   /* 姓名   */
  STU_FIELD_GENDER = 4,   /* 性别   */
  STU_FIELD_SCORE = 5,   /* 分数   */
  STU_FIELD_GPA = 6,   /* 绩点   */
  STU_FIELD_RANK = 7,   /* 排名   */
  STU_FIELD_COUNT = 8    /* 字段总数，用于校验 field 是否合法 */
} stu_field_t;

/* ==================================================================
 * 五、UI 数据层接口（ui.c 调用）
 * ================================================================== */

 /* ------------------------- 5.1 数据访问 ------------------------- */

 /** 获取当前内存中的学生总数（未筛选状态）。 */
int data_get_count(void);

/** 获取学生结构体数组首地址，数组长度为 data_get_count()。 */
StudentCSV* data_get_all(void);

/**
 * 获取网格当前应显示的数据行数。
 *   无筛选且无数据：返回 DATA_DEFAULT_DISPLAY_ROWS（200）；
 *   无筛选且有数据：返回学生总数；
 *   筛选生效：返回筛选命中的行数。
 */
int data_get_display_count(void);

/**
 * 把显示行号（从 0 开始）映射为内存数组下标。
 * @return 对应 StudentCSV 下标；该行为空时返回 -1。
 */
int data_get_display_index(int display_row);

/* ---------------------- 5.2 导入 / 导出 ---------------------- */

/**
 * 从 CSV 文件导入学生数据，整体覆盖内存中的数据。
 * 成功返回导入条数；路径为空或读取失败返回 -1。
 */
int data_import_csv(const char* path);

/**
 * 将内存中的全部学生数据导出为 CSV。
 * 成功返回导出条数；路径为空或写入失败返回 -1。
 */
int data_export_csv(const char* path);

/* ---------------------- 5.3 删除 / 修改 ---------------------- */

/**
 * 按下标删除一名学生，后面的元素整体前移一位。
 * @return 1 成功；0 失败（下标越界）。
 */
int data_delete_student(int index);

/**
 * 修改指定学生的某个字段。
 * @param index 学生数组下标
 * @param field 字段编号（stu_field_t，0~7）
 * @param text  用户输入的新内容
 * @return 1 成功；0 失败（下标、字段或输入内容不合法）。
 */
int data_update_student_field(int index, int field, const char* text);

/* ------------------------- 5.4 筛选 ------------------------- */

/**
 * 获取某个字段可用于筛选的不同选项。
 *   年级/班级：返回数据中出现过的不同取值；
 *   性别：返回“男”“女”；
 *   分数：按每 10.0 分一段返回区间文本；
 *   其他字段：返回 0。
 * @return 实际选项个数（可能大于 max_opts，此时只写入前 max_opts 个）。
 */
int data_get_filter_options(int field, char options[][MAX_FIELD_LEN], int max_opts);

/**
 * 按字段 + 选项应用筛选。
 * @return 1 成功；0 失败（字段不支持筛选或选项为空）。
 */
int data_apply_filter(int field, const char* option);

/** 清除当前筛选条件，恢复显示全部数据。 */
void data_clear_filter(void);

/* ------------------------- 5.5 搜索 ------------------------- */

/**
 * 在当前筛选结果（显示行）中搜索关键字。
 * 搜索只用于界面高亮，不改变筛选状态和显示行数。
 * @param keyword  搜索关键字；空串直接返回 0，不产生高亮
 * @param out_rows 输出匹配到的显示行号数组（从 0 开始）
 * @param max_rows out_rows 容量
 * @return 匹配数量（可能大于 max_rows，此时只写入前 max_rows 个）。
 */
int data_find_matches(const char* keyword, int* out_rows, int max_rows);

/* ------------------------- 5.6 排序 ------------------------- */

/**
 * 按指定字段对学生数组排序。
 *   含字符字段：先按字符串中的数字部分数值排序，再按字符串排序；
 *   纯数字字段：按数值大小排序；
 *   bool 字段：true=1、false=0，正序按 1 > 0。
 * @param field     字段编号（stu_field_t，0~7）
 * @param ascending true=正序，false=倒序
 */
void data_sort_students(int field, bool ascending);

/* ---------------------- 5.7 帮助图片 ---------------------- */

/** 返回帮助图片总张数（CORE_HELP_IMAGE_COUNT）。 */
int ui_help_get_image_count(void);

/**
 * 返回第 index 张帮助图片源（形如 "A:help/help1.png"），
 * 可直接交给 lv_imagebutton_set_src() 使用。
 * index 从 0 开始；越界返回 NULL。
 */
const void* ui_help_get_image_src(int index);

#endif /* CORE_H */
