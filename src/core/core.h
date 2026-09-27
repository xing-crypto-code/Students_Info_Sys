/*
 * @file core.h
 * @brief 学生成绩信息管理系统 —— 数据核心层头文件（纯 C，不依赖 LVGL）。
 *
 * 分层关系：
 *   ui.c / ui.h      LVGL 图形界面层：只负责界面显示和用户交互；
 *   core.c / core.h  数据核心层：学生数组、CSV 读写、筛选、搜索、排序和统计。
 *
 * ui.c 只调用本文件声明的接口，不直接访问学生数组。
 */

#ifndef CORE_H
#define CORE_H

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

/* ==================================================================
 * 一、学生数据结构
 * ================================================================== */

/*
 * @brief 与 CSV 数据列一一对应的学生结构体。
 *
 * CSV 列顺序：
 *   年级,班级,学号,姓名,性别,平时,期中,期末,总分,绩点,排名
 */
typedef struct {
    char     grade[64];       /* 年级，例如 "2023级" */
    char     class_name[64];  /* 班级，例如 "计算机1班" */
    uint64_t id;              /* 学号（12 位数字） */
    char     name[64];        /* 姓名 */
    bool     gender;          /* 性别：true=男，false=女 */
    float    regular_score;   /* 平时成绩（0.0 ~ 100.0） */
    float    midterm_score;   /* 期中成绩（0.0 ~ 100.0） */
    float    final_score;     /* 期末成绩（0.0 ~ 100.0） */
    float    score;           /* 总分（按权重实时计算） */
    float    gpa;             /* 绩点（按总分实时计算，0.0 ~ 5.0） */
    uint16_t rank;            /* 排名（从 1 开始） */

    /* 以下两个字段只在内存中使用，不写入 CSV */
    bool     is_new;          /* true=新增且还没填完的行 */
    uint16_t new_filled_mask; /* 已正确填写的字段位图，bit 与 stu_field_t 对应 */
} StudentCSV;

/* ==================================================================
 * 二、字段编号
 * ================================================================== */

/*
 * @brief 表格数据列对应的字段编号。
 *
 * 数值必须与 ui.h 中的 ui_field_t 保持一致，UI 层会把 ui_field_t
 * 直接传给 data_sort_students()、data_apply_filter() 等函数。
 */
typedef enum {
    STU_FIELD_GRADE = 0,    /* 年级   */
    STU_FIELD_CLASS = 1,    /* 班级   */
    STU_FIELD_ID = 2,       /* 学号   */
    STU_FIELD_NAME = 3,     /* 姓名   */
    STU_FIELD_GENDER = 4,   /* 性别   */
    STU_FIELD_REGULAR = 5,  /* 平时   */
    STU_FIELD_MIDTERM = 6,  /* 期中   */
    STU_FIELD_FINAL = 7,    /* 期末   */
    STU_FIELD_SCORE = 8,    /* 总分（按权重实时计算） */
    STU_FIELD_GPA = 9,      /* 绩点（按总分实时计算） */
    STU_FIELD_RANK = 10,    /* 排名   */
    STU_FIELD_COUNT = 11    /* 字段总数，用于校验 field 是否合法 */
} stu_field_t;

/* ==================================================================
 * 三、数据访问
 * ================================================================== */

/* 获取当前内存中的学生总数（未筛选状态）。 */
int data_get_count(void);

/* 获取学生结构体数组首地址，数组长度为 data_get_count()。 */
StudentCSV* data_get_all(void);

/*
 * 获取网格当前应显示的数据行数。
 *   无筛选且无数据：返回 200（显示 200 行空白网格）；
 *   无筛选且有数据：返回 200（不足部分显示为空行）；
 *   筛选生效：返回筛选命中的行数。
 */
int data_get_display_count(void);

/*
 * 把显示行号（从 0 开始）映射为内存数组下标。
 * @return 对应 StudentCSV 下标；该行为空时返回 -1。
 */
int data_get_display_index(int display_row);

/* ==================================================================
 * 四、CSV 导入、导出与路径解析
 * ================================================================== */

/*
 * 从 CSV 文件导入学生数据，整体覆盖内存中的数据。
 * 成功返回导入条数；路径为空或读取失败返回 -1。
 */
int data_import_csv(const char* path);

/*
 * 将内存中的全部学生数据导出为 CSV。
 * 成功返回导出条数；路径为空或写入失败返回 -1。
 */
int data_export_csv(const char* path);

/*
 * 按“年级-班级”导出统计结果 TXT。
 * 每个班级输出平时/期中/期末/总分 4 行，每行包含平均分、最高/最低成绩、
 * 及格人数和及格率；不同班级之间空一行。
 * 成功返回写入的班级数（>=0）；路径为空或写入失败返回 -1。
 */
int data_export_statistics(const char* path);

/*
 * 根据用户输入的地址生成导出文件的完整路径。
 *
 * 规则：
 *   - user_path 为空：使用桌面；
 *   - user_path 是已存在的文件夹：使用该文件夹；
 *   - user_path 是文件（含 .csv/.txt 文件名）：使用它所在的文件夹；
 *   - 文件名固定为 students_年月日时分秒.csv（或 .txt）。
 *
 * @param user_path      用户输入（可为 NULL 或空字符串）
 * @param is_statistics  0=CSV 成绩表，1=TXT 统计结果
 * @param out_path       输出完整路径缓冲区
 * @param out_size       out_path 大小
 * @return 1 成功，0 失败
 */
int data_resolve_export_path(const char* user_path, int is_statistics,
    char* out_path, size_t out_size);

/*
 * 根据用户输入的地址查找要导入的 CSV 文件完整路径。
 *
 * 规则：
 *   - user_path 为空：在桌面查找；
 *   - user_path 是文件夹：在该文件夹查找；
 *   - user_path 是 .csv 文件：直接使用该文件（兼容旧用法）；
 *   - 查找目标为 students_*.csv 中最后修改时间最新的一个。
 *
 * @param user_path 用户输入（可为 NULL 或空字符串）
 * @param out_path  输出完整文件路径缓冲区
 * @param out_size  out_path 大小
 * @return 1 找到并写入 out_path，0 未找到或失败
 */
int data_resolve_import_path(const char* user_path, char* out_path, size_t out_size);

/* ==================================================================
 * 五、新增、删除、修改与字段校验
 * ================================================================== */

/*
 * 按下标删除一名学生，后面的元素整体前移一位。
 * 删除后调用 core_recalc_rank() 按总分降序重算排名。
 * @return 1 成功；0 失败（下标越界）。
 */
int data_delete_student(int index);

/*
 * 新增一名学生，只设置年级和班级，其他字段为空。
 * @return 成功返回新学生数组下标；失败返回 -1。
 */
int data_add_student(const char* grade, const char* class_name);

/*
 * 检查学号是否合法：必须是 12 位数字，且不能为全 0。
 * @return 1 合法；0 不合法。
 */
int data_validate_student_id(const char* id_text);

/*
 * 通过学号文本新增一名学生。
 * @return >=0 新学生数组下标；
 *         -1 学号格式错误；
 *         -2 学号已存在；
 *         -3 人数已满；
 *         -4 学号不合法（不是 12 位数字或全 0）。
 */
int data_add_student_by_id(const char* id_text);

/*
 * 判断某个学生的某个字段是否已经填写且格式正确。
 * 只对“新增未填完”的行生效；原有数据一律返回 1。
 * @return 1 合法；0 未填写或格式错误。
 */
int data_validate_student_field(int index, int field);

/*
 * 修改指定学生的某个字段。
 *
 * 修改平时/期中/期末成绩时：
 *   - 立即重算该生总分和绩点；
 *   - 调用 core_recalc_rank_inplace() 原地刷新所有学生的 rank 数值，
 *     不移动学生数组位置，表格当前行顺序保持不变。
 *
 * @param index 学生数组下标
 * @param field 字段编号（stu_field_t，0~10）
 * @param text  用户输入的新内容
 * @return 1 成功；0 失败（下标、字段或输入内容不合法）。
 */
int data_update_student_field(int index, int field, const char* text);

/* ==================================================================
 * 六、成绩计算与权重
 * ================================================================== */

/*
 * 根据总分计算绩点：
 *   100 分 = 5.0；
 *   每低 1 分，绩点降低 0.1；
 *   低于 60 分统一为 0.0。
 */
float data_calc_gpa(float score);

/*
 * 根据平时/期中/期末成绩和当前权重，计算总分：
 *   总分 = (平时 × 平时权重 + 期中 × 期中权重 + 期末 × 期末权重) / 100
 */
float data_calc_total(float regular, float midterm, float final);

/* 恢复默认权重：平时 30%，期中 20%，期末 50%。 */
void data_reset_weights(void);

/*
 * 用文本设置某一项权重（百分数，0~100）。
 * @param which 0=平时, 1=期中, 2=期末
 * @return 1 成功；0 失败（格式错误或超范围）。
 */
int data_set_weight_from_text(int which, const char* text);

/*
 * 一次输入三项权重，支持用空格、逗号、斜杠等符号分隔。
 * 例如："30 20 50"、"30,20,50"、"30/20/50"。
 * @return 1 解析并写入成功；0 格式错误或数量不是 3 个。
 */
int data_set_weights_from_text(const char* text);

/* 获取某一项权重，单位：百分数。 */
float data_get_weight(int which);

/* 获取三项权重之和，单位：百分数。 */
float data_get_weight_sum(void);

/*
 * 按当前权重重新计算所有学生的总分、绩点和排名，并刷新显示映射。
 * 本函数内部使用 core_recalc_rank()，会按总分降序重新排列学生数组。
 */
void data_recalc_all_totals(void);

/* ==================================================================
 * 七、筛选、搜索与排序
 * ================================================================== */

/*
 * 获取某个字段可用于筛选的不同选项。
 *   年级/班级：返回数据中出现过的不同取值；
 *   性别：返回“男”“女”；
 *   分数：按每 10.0 分一段返回区间文本；
 *   其他字段：返回 0。
 * @return 实际选项个数（可能大于 max_opts，此时只写入前 max_opts 个）。
 */
int data_get_filter_options(int field, char options[][64], int max_opts);

/*
 * 按字段 + 选项应用筛选。
 * @return 1 成功；0 失败（字段不支持筛选或选项为空）。
 */
int data_apply_filter(int field, const char* option);

/* 清除当前筛选条件，恢复显示全部数据。 */
void data_clear_filter(void);

/*
 * 在当前筛选结果（显示行）中搜索关键字。
 * 搜索只用于界面高亮，不改变筛选状态和显示行数。
 * @param keyword  搜索关键字；空串直接返回 0，不产生高亮
 * @param out_rows 输出匹配到的显示行号数组（从 0 开始）
 * @param max_rows out_rows 容量
 * @return 匹配数量（可能大于 max_rows，此时只写入前 max_rows 个）。
 */
int data_find_matches(const char* keyword, int* out_rows, int max_rows);

/*
 * 按指定字段对学生数组排序。
 *   含字符字段：先按字符串中的数字部分数值排序，再按字符串排序；
 *   纯数字字段：按数值大小排序；
 *   bool 字段：true=1、false=0，正序按 1 > 0。
 * @param field     字段编号（stu_field_t，0~10）
 * @param ascending true=正序，false=倒序
 */
void data_sort_students(int field, bool ascending);

/* ==================================================================
 * 八、帮助图片与运行辅助
 * ================================================================== */

/* 返回帮助图片总张数（由 core.c 中的 HELP_IMAGE_COUNT 控制）。 */
int ui_help_get_image_count(void);

/*
 * 返回第 index 张帮助图片源（形如 "A:help_png/helpN.png"），
 * 可直接交给 lv_imagebutton_set_src() 使用。
 * index 从 0 开始；越界返回 NULL。
 */
const void* ui_help_get_image_src(int index);

/*
 * 检查程序所在 bin 目录下是否存在首次运行标志文件 help_shown.flag。
 * @return 1 存在；0 不存在或读取失败。
 */
int core_flag_file_exists(void);

/*
 * 在程序所在 bin 目录下创建首次运行标志文件 help_shown.flag。
 * @return 1 创建成功；0 创建失败。
 */
int core_flag_file_create(void);

#endif /* CORE_H */
