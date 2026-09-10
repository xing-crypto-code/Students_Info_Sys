/**
 * @file stu.c
 * @brief 学生成绩信息管理系统 —— 数据层（后端）实现。
 *
 * 本文件分两大部分：
 *
 * 【一、基础数据管理】
 *   students[] 数组 + student_count 是唯一的数据源，
 *   提供增删改查、排序、统计等基础操作，供控制台菜单使用。
 *
 * 【二、UI 数据层接口】
 *   实现《UI函数需求文档.md》第 4 节要求的全部 data_* / ui_help_* 函数，
 *   供 LVGL 前端 ui.c 调用。这一层在基础数组之上增加了三样东西：
 *     1. 显示行映射表 display_indexes[]：把“表格显示行”映射到“数组下标”，
 *        使筛选后 UI 仍然按 0,1,2... 的顺序逐行取值；
 *     2. 筛选状态 filter_field / filter_option：记录当前筛选条件；
 *     3. CSV 表头缓存 csv_header：导入时保存表头，导出时原样写回。
 *
 * 注意：所有 data_* 函数在真正访问数据前都会调用 data_ensure_initialized()，
 * 保证第一次被 UI 调用时会自动尝试从默认 CSV 路径加载数据。
 */

#include "stu.h"

#include "read_students_csv.h"   /* read_students_csv / recalculate_ranking_by_score */
#include "write_students_csv.h"  /* write_students_csv */

#include <ctype.h>
#include <inttypes.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

/* ==================================================================
 * 内部状态
 * ================================================================== */

/* ---- 基础数据 ---- */

static StudentCSV students[MAX_STUDENTS];   /* 学生数组，全系统唯一数据源 */
static int student_count;                   /* 当前学生人数 */

/* ---- UI 显示层状态 ---- */

/**
 * 显示行映射表。
 * display_indexes[row] = 该显示行对应的 students[] 下标。
 * 未筛选时它就是 0,1,2,...,count-1；
 * 筛选后只保存命中的下标，于是 UI 只看到筛选结果。
 */
static int display_indexes[MAX_STUDENTS];
static int display_count;                   /* 当前显示行数（= 映射表有效长度） */

static int  data_initialized;               /* 是否已完成首次自动加载 */
static int  filter_field = -1;              /* 当前筛选字段；-1 表示未筛选 */
static char filter_option[MAX_FIELD_LEN];   /* 当前筛选选项文本 */
static char csv_header[MAX_LINE_LEN];       /* 导入时保存的 CSV 表头，导出时写回 */

/**
 * @brief 带“数字识别”的自然字符串比较（排序内部使用）。
 *
 * 普通 strcmp 会得到 "计算机10班" < "计算机2班"，因为字符 '1' < '2'；
 * 本函数遇到两串同时出现数字时，先按数字的数值大小比较，因此
 * "计算机2班" < "计算机10班"，符合人的直觉。
 *
 * @return <0：left 排在 right 前；0：相等；>0：left 排在 right 后。
 */
static int compare_text_natural(const char *left, const char *right)
{
    while(*left != '\0' && *right != '\0') {
        if(*left >= '0' && *left <= '9' && *right >= '0' && *right <= '9') {
            char *left_end;
            char *right_end;
            unsigned long left_number = strtoul(left, &left_end, 10);
            unsigned long right_number = strtoul(right, &right_end, 10);

            if(left_number != right_number) return left_number < right_number ? -1 : 1;
            left = left_end;
            right = right_end;
            continue;
        }

        if((unsigned char)*left != (unsigned char)*right) {
            return (unsigned char)*left < (unsigned char)*right ? -1 : 1;
        }
        left++;
        right++;
    }

    if(*left == *right) return 0;
    return *left == '\0' ? -1 : 1;
}

/* qsort 的比较函数不能接收自定义参数，因此用文件级静态变量传递排序条件 */
static int sort_field;      /* 排序字段（stu_field_t） */
static int sort_direction;  /* 1 = 正序，-1 = 倒序 */

/**
 * @brief qsort 使用的学生比较函数，读取上面的 sort_field / sort_direction。
 *
 * 字段比较规则（对应《UI函数需求文档.md》4.6 节）：
 *   年级 / 班级 / 姓名 → 自然字符串比较（数字部分优先）；
 *   学号 / 分数 / 绩点 / 排名 → 数值比较；
 *   性别 → true(男) 视为 1、false(女) 视为 0。
 */
static int compare_students(const void *left_ptr, const void *right_ptr)
{
    const StudentCSV *left = (const StudentCSV *)left_ptr;
    const StudentCSV *right = (const StudentCSV *)right_ptr;
    int result = 0;

    switch(sort_field) {
    case STU_FIELD_GRADE:   /* 年级：混合文字+数字，用自然比较 */
        result = compare_text_natural(left->grade, right->grade);
        break;
    case STU_FIELD_CLASS:   /* 班级：混合文字+数字，用自然比较 */
        result = compare_text_natural(left->class_name, right->class_name);
        break;
    case STU_FIELD_ID:      /* 学号：纯数字，按数值比较 */
        result = left->id < right->id ? -1 : left->id > right->id;
        break;
    case STU_FIELD_NAME:    /* 姓名：自然比较，便于姓名中带数字的情况 */
        result = compare_text_natural(left->name, right->name);
        break;
    case STU_FIELD_GENDER:  /* 性别：true 视为 1，false 视为 0 */
        result = left->gender == right->gender ? 0 : (left->gender ? -1 : 1);
        break;
    case STU_FIELD_SCORE:   /* 分数：按数值比较 */
        result = left->score < right->score ? -1 : left->score > right->score;
        break;
    case STU_FIELD_GPA:     /* 绩点：按数值比较 */
        result = left->gpa < right->gpa ? -1 : left->gpa > right->gpa;
        break;
    case STU_FIELD_RANK:    /* 排名：按数值比较 */
        result = left->rank < right->rank ? -1 : left->rank > right->rank;
        break;
    default:
        break;
    }

    return result * sort_direction;
}

/* ==================================================================
 * 一、基础数据管理接口实现
 * ================================================================== */

/* ---- 内部工具：筛选判断与显示行映射 ---- */

/** 当前是否存在生效中的筛选条件。 */
static int data_filter_active(void)
{
    return filter_field >= 0 && filter_option[0] != '\0';
}

/**
 * @brief 把分数换算成“每 10.0 分一段”的区间文本。
 * 例如 85.5 → "80-90"，0 分 → "0-10"，100 分 → "100-110"。
 */
static void data_score_range_text(float score, char *buf, size_t buf_size)
{
    int lower;
    int upper;

    if(score < 0.0f) score = 0.0f;              /* 容错：负数按 0 处理 */
    lower = (int)(score / 10.0f) * 10;          /* 向下取到 10 的整数倍 */
    upper = lower + 10;
    snprintf(buf, buf_size, "%d-%d", lower, upper);
}

/**
 * @brief 判断分数是否落在筛选区间文本内。
 *
 * 区间约定为左闭右开 [lower, upper)；
 * 为了让满分 100 能被 "100-110" 命中，当 upper >= 100 时把上界视为闭区间。
 *
 * @return 1 命中；0 不命中或区间文本非法。
 */
static int data_score_match(float score, const char *option)
{
    float lower;
    float upper;

    if(option == NULL) return 0;
    if(sscanf(option, "%f-%f", &lower, &upper) != 2) return 0;
    if(score >= lower && score < upper) return 1;
    return (upper >= 100.0f && score == upper);
}

/**
 * @brief 判断一名学生是否满足当前筛选条件。
 * @return 1 命中（未筛选时全部命中）；0 不命中。
 */
static int data_match_filter(const StudentCSV *student)
{
    char value[MAX_FIELD_LEN];

    if(!data_filter_active()) return 1;                                     /* 未筛选 → 全部显示 */
    if(filter_field == STU_FIELD_SCORE) return data_score_match(student->score, filter_option);

    /* 其余可筛选字段都是“文本相等”比较，先取出该字段的显示文本 */
    switch(filter_field) {
    case STU_FIELD_GRADE:
        snprintf(value, sizeof(value), "%s", student->grade);
        break;
    case STU_FIELD_CLASS:
        snprintf(value, sizeof(value), "%s", student->class_name);
        break;
    case STU_FIELD_GENDER:
        snprintf(value, sizeof(value), "%s", student->gender ? "男" : "女");
        break;
    default:
        return 1;   /* 该字段不支持筛选，视为不过滤 */
    }

    return strcmp(value, filter_option) == 0;
}

/**
 * @brief 重建“显示行 → 数组下标”映射表。
 *
 * 数据发生任何变化（导入、删除、修改、排序、筛选）后都要调用本函数，
 * 否则 data_get_display_index() 会返回过期下标，界面显示就会错乱。
 */
static void data_rebuild_display(void)
{
    display_count = 0;

    for(int i = 0; i < student_count && display_count < MAX_STUDENTS; i++) {
        if(data_match_filter(&students[i])) display_indexes[display_count++] = i;
    }
}

/**
 * @brief 首次被 UI 调用时自动加载默认 CSV 数据（懒加载）。
 *
 * 依次尝试几个常见路径，全部失败则从空数据开始，
 * 此时 data_get_display_count() 会返回 200，UI 显示 200 行空网格。
 */
static void data_load_default(void)
{
    static const char * const default_paths[] = {
        "Data/students.csv",   /* 工作区数据目录 */
        "lvgl/students.csv",   /* lvgl 演示目录 */
        "students.csv"         /* 程序当前目录 */
    };
    StudentCSV loaded[MAX_STUDENTS];
    int count = -1;

    for(size_t i = 0; i < sizeof(default_paths) / sizeof(default_paths[0]); i++) {
        count = read_students_csv(default_paths[i], loaded, MAX_STUDENTS,
                                  csv_header, sizeof(csv_header));
        if(count >= 0) break;
    }

    if(count < 0) {
        replaceStudents(NULL, 0);
        csv_header[0] = '\0';
    } else {
        replaceStudents(loaded, count);
        /* 排名是派生数据：无论 CSV 里写了什么，都按分数重新计算一遍 */
        recalculate_ranking_by_score(getStudentArray(), getStudentCount());
    }

    filter_field = -1;
    filter_option[0] = '\0';
    data_rebuild_display();
    data_initialized = 1;
}

/** 保证数据已加载。所有 data_* 接口的入口都会先调用它。 */
static void data_ensure_initialized(void)
{
    if(!data_initialized) data_load_default();
}

/* ---- 基础增删改查 ---- */

/**
 * 清空内存中的学生数据，并复位筛选状态与显示行映射。
 * 复位 data_initialized 是为了让后续 UI 首次访问时仍能自动加载默认 CSV。
 */
void stuInit(void)
{
    student_count = 0;
    filter_field = -1;
    filter_option[0] = '\0';
    data_rebuild_display();
    data_initialized = 0;
}

/**
 * 用新数组整体替换内存中的学生数据（不做任何校验，属于底层覆盖操作）。
 * @return 1 成功；0 参数非法。
 */
int replaceStudents(const StudentCSV *source, int count)
{
    if(count < 0 || count > MAX_STUDENTS || (count > 0 && source == NULL)) return 0;

    if(count > 0) memcpy(students, source, (size_t)count * sizeof(StudentCSV));
    student_count = count;
    data_rebuild_display();   /* 数据变了，显示行映射必须跟着重建 */
    return 1;
}

/**
 * 追加一名学生（会做完整的数据校验）。
 * @return 1 成功；0 校验失败。
 */
int addStudent(const StudentCSV *student)
{
    if(student == NULL || student->id == 0 || student->name[0] == '\0' ||
       student->score < 0.0f || student->score > 100.0f ||
       student->gpa < 0.0f || student->gpa > 4.0f || student_count >= MAX_STUDENTS) {
        return 0;
    }

    if(findById(student->id) != NULL) return 0;   /* 学号必须唯一 */
    students[student_count++] = *student;
    data_rebuild_display();
    return 1;
}

/**
 * 按学号删除一名学生，后面的元素整体前移一位。
 * @return 1 成功；0 未找到该学号。
 */
int deleteStudent(uint64_t id)
{
    StudentCSV *student = findById(id);
    int index;

    if(student == NULL) return 0;
    index = (int)(student - students);                     /* 指针相减得到数组下标 */
    for(int i = index; i + 1 < student_count; i++) students[i] = students[i + 1];
    student_count--;
    data_rebuild_display();
    return 1;
}

/** 获取内存中学生总数（未筛选状态）。 */
int getStudentCount(void)
{
    return student_count;
}

/** 获取学生结构体数组首地址（长度等于 getStudentCount()）。 */
StudentCSV *getStudentArray(void)
{
    return students;
}

/**
 * 按内存数组下标取得学生指针。
 * @return 下标越界时返回 NULL。
 */
StudentCSV *getStudent(int index)
{
    if(index < 0 || index >= student_count) return NULL;
    return &students[index];
}

/**
 * 按学号查找学生。
 * @return 找到返回学生指针；未找到返回 NULL。
 */
StudentCSV *findById(uint64_t id)
{
    for(int i = 0; i < student_count; i++) {
        if(students[i].id == id) return &students[i];
    }
    return NULL;
}

/**
 * 按姓名精确查找（区分大小写），可一次返回多条结果。
 * @return 实际匹配人数，可能大于 max_count。
 */
int findByName(const char *name, StudentCSV result[], int max_count)
{
    int found_count = 0;

    if(name == NULL || result == NULL || max_count <= 0) return 0;
    for(int i = 0; i < student_count; i++) {
        if(strcmp(students[i].name, name) == 0) {
            if(found_count < max_count) result[found_count] = students[i];
            found_count++;
        }
    }
    return found_count;
}

/**
 * 按学号整体替换一名学生的信息，学号本身保持为参数 id。
 * @return 1 成功；0 失败（未找到学生或新数据非法）。
 */
int modifyStudent(uint64_t id, const StudentCSV *new_info)
{
    StudentCSV *student = findById(id);

    if(student == NULL || new_info == NULL || new_info->score < 0.0f ||
       new_info->score > 100.0f || new_info->gpa < 0.0f || new_info->gpa > 4.0f ||
       new_info->name[0] == '\0') return 0;

    *student = *new_info;
    student->id = id;
    return 1;
}

/**
 * 按字段对内存数组排序（qsort），排序规则见 compare_students()。
 * @param field     字段编号（0~7）
 * @param ascending true = 正序，false = 倒序
 */
void sortStudents(int field, bool ascending)
{
    if(field < 0 || field >= STU_FIELD_COUNT) return;
    sort_field = field;
    sort_direction = ascending ? 1 : -1;
    qsort(students, (size_t)student_count, sizeof(StudentCSV), compare_students);
}

/**
 * 统计分数信息：平均分、最高分、最低分、及格人数。
 * @return 1 成功；0 失败（无数据或指针为空）。
 */
int getStatistics(float *average, float *highest, float *lowest, int *pass_count)
{
    float total = 0.0f;

    if(student_count == 0 || average == NULL || highest == NULL ||
       lowest == NULL || pass_count == NULL) return 0;

    *highest = students[0].score;
    *lowest = students[0].score;
    *pass_count = 0;

    for(int i = 0; i < student_count; i++) {
        total += students[i].score;
        if(students[i].score > *highest) *highest = students[i].score;
        if(students[i].score < *lowest) *lowest = students[i].score;
        if(students[i].score >= 60.0f) (*pass_count)++;
    }

    *average = total / student_count;
    return 1;
}

/* ==================================================================
 * 二、UI 数据层接口实现（《UI函数需求文档.md》第 4 节）
 *
 * 这一层在基础数组之上工作，所有函数都遵守两条约定：
 *   1. 进入函数先调用 data_ensure_initialized()，保证数据已加载；
 *   2. 只要数组内容或筛选条件发生变化，就调用 data_rebuild_display()
 *      重建显示行映射，使 data_get_display_count() /
 *      data_get_display_index() 立刻返回最新结果。
 * ================================================================== */

/* -------------------- 内部工具：文本解析 -------------------- */

/** 跳过 *end 位置起连续出现的空白字符（空格、Tab、回车等）。 */
static void data_skip_trailing_space(char **end)
{
    while(**end != '\0' && isspace((unsigned char)**end)) (*end)++;
}

/**
 * @brief 解析非负整数文本（用于学号、排名）。
 * @return 1 成功；0 失败（空串、含非数字字符、负数）。
 */
static int data_parse_uint64(const char *text, uint64_t *out)
{
    char *end;
    unsigned long long value;

    if(text == NULL || out == NULL || text[0] == '\0') return 0;
    if(text[0] == '-') return 0;   /* strtoull 会把 "-5" 转成极大正数，必须先挡掉 */
    value = strtoull(text, &end, 10);
    if(end == text) return 0;      /* 一个数字都没有 */
    data_skip_trailing_space(&end);
    if(*end != '\0') return 0;     /* 仍有非法字符，例如 "12abc" */
    *out = (uint64_t)value;
    return 1;
}

/**
 * @brief 解析浮点数文本（用于分数、绩点）。
 * @return 1 成功；0 失败（空串或含非法字符）。
 */
static int data_parse_float(const char *text, float *out)
{
    char *end;
    float value;

    if(text == NULL || out == NULL || text[0] == '\0') return 0;
    value = strtof(text, &end);
    if(end == text) return 0;
    data_skip_trailing_space(&end);
    if(*end != '\0') return 0;
    *out = value;
    return 1;
}

/**
 * @brief 解析性别文本。
 * 支持：“男 / M / m / 1” → true，“女 / F / f / 0” → false。
 * @return 1 成功；0 无法识别。
 */
static int data_parse_gender(const char *text, bool *out)
{
    if(text == NULL || out == NULL) return 0;

    if(strcmp(text, "男") == 0 || strcmp(text, "M") == 0 ||
       strcmp(text, "m") == 0 || strcmp(text, "1") == 0) {
        *out = true;
        return 1;
    }
    if(strcmp(text, "女") == 0 || strcmp(text, "F") == 0 ||
       strcmp(text, "f") == 0 || strcmp(text, "0") == 0) {
        *out = false;
        return 1;
    }
    return 0;
}

/* ------------------------- 4.1 数据访问 ------------------------- */

/** 获取当前内存中学生总数（未筛选状态）。 */
int data_get_count(void)
{
    data_ensure_initialized();
    return student_count;
}

/** 返回学生结构体数组首地址，数组长度为 data_get_count()。 */
StudentCSV *data_get_all(void)
{
    data_ensure_initialized();
    return students;
}

/**
 * 返回网格当前应显示的数据行数。
 * 未筛选且没有任何学生时返回 200（默认空网格），其余情况返回显示行数。
 */
int data_get_display_count(void)
{
    data_ensure_initialized();

    if(student_count == 0 && !data_filter_active()) return DATA_DEFAULT_DISPLAY_ROWS;
    return display_count;
}

/**
 * 把显示行号（从 0 开始）映射为内存数组下标。
 * @return 对应下标；该行为空（超出显示行数）时返回 -1。
 */
int data_get_display_index(int display_row)
{
    data_ensure_initialized();

    if(display_row < 0 || display_row >= display_count) return -1;
    return display_indexes[display_row];
}

/* ---------------------- 4.2 导入 / 导出 ---------------------- */

/**
 * 从 CSV 导入数据，整体覆盖内存中的数据。
 *
 * 处理流程：
 *   1. 读取 CSV（表头单独存入 csv_header，导出时原样写回）；
 *   2. 覆盖学生数组；
 *   3. 按分数重新计算排名（数组会因此变成按分数降序）；
 *   4. 清除旧筛选条件并重建显示行映射。
 *
 * @return 成功返回导入条数；路径为空或读取失败返回 -1。
 */
int data_import_csv(const char *path)
{
    StudentCSV loaded[MAX_STUDENTS];
    int count;

    if(path == NULL || path[0] == '\0') return -1;

    count = read_students_csv(path, loaded, MAX_STUDENTS, csv_header, sizeof(csv_header));
    if(count < 0) return -1;

    replaceStudents(loaded, count);
    recalculate_ranking_by_score(getStudentArray(), getStudentCount());

    /* 整套数据都换了，旧筛选条件已无意义，直接清除 */
    filter_field = -1;
    filter_option[0] = '\0';
    data_rebuild_display();
    data_initialized = 1;
    return student_count;
}

/**
 * 把内存中的全部学生数据写成 CSV（与当前筛选状态无关）。
 * @return 成功返回导出条数；路径为空或写入失败返回 -1。
 */
int data_export_csv(const char *path)
{
    data_ensure_initialized();

    if(path == NULL || path[0] == '\0') return -1;
    /* csv_header 为空时 write_students_csv 会自动使用默认表头 */
    return write_students_csv(path, students, student_count, csv_header);
}

/* ---------------------- 4.3 删除 / 修改 ---------------------- */

/**
 * 按内存数组下标删除一名学生，并把后面的元素前移一位。
 * 删除成功后重算排名（数组会按分数降序重排）并重建显示行映射。
 *
 * @param index 内存数组下标（不是显示行号，需要先用 data_get_display_index() 换算）
 * @return 1 成功；0 失败（下标越界）。
 */
int data_delete_student(int index)
{
    data_ensure_initialized();

    if(index < 0 || index >= student_count) return 0;
    if(!deleteStudent(students[index].id)) return 0;   /* 内部已重建显示映射 */

    recalculate_ranking_by_score(getStudentArray(), getStudentCount());
    data_rebuild_display();
    return 1;
}

/**
 * 修改指定学生的某个字段：把用户输入的文本解析成正确的类型后写回。
 *
 * 各字段解析规则与合法范围：
 *   年级 / 班级 / 姓名 → 字符串（姓名不允许为空，超长自动截断）
 *   学号               → 非负整数，且不能与其他学生重复
 *   性别               → 男/M/m/1 或 女/F/f/0
 *   分数               → 0.0 ~ 100.0（改完会重算排名，数组按分数降序重排）
 *   绩点               → 0.0 ~ 4.0
 *   排名               → 0 ~ 65535
 *
 * @param index 内存数组下标
 * @param field 字段编号（stu_field_t / ui_field_t，0~7）
 * @param text  用户输入的新内容
 * @return 1 成功；0 失败（下标或字段非法、文本为空、解析/范围校验失败）。
 */
int data_update_student_field(int index, int field, const char *text)
{
    StudentCSV *student;
    uint64_t value;

    data_ensure_initialized();

    if(index < 0 || index >= student_count || text == NULL) return 0;
    if(field < 0 || field >= STU_FIELD_COUNT) return 0;

    student = &students[index];

    switch(field) {
    case STU_FIELD_GRADE:                       /* 年级：纯字符串 */
        snprintf(student->grade, sizeof(student->grade), "%s", text);
        break;

    case STU_FIELD_CLASS:                       /* 班级：纯字符串 */
        snprintf(student->class_name, sizeof(student->class_name), "%s", text);
        break;

    case STU_FIELD_ID:                          /* 学号：非负整数且必须唯一 */
        if(!data_parse_uint64(text, &value) || value == 0) return 0;
        for(int i = 0; i < student_count; i++) {
            if(i != index && students[i].id == value) return 0;   /* 学号重复，拒绝修改 */
        }
        student->id = value;
        break;

    case STU_FIELD_NAME:                        /* 姓名：非空字符串 */
        if(text[0] == '\0') return 0;
        snprintf(student->name, sizeof(student->name), "%s", text);
        break;

    case STU_FIELD_GENDER:                      /* 性别：解析为 bool */
        if(!data_parse_gender(text, &student->gender)) return 0;
        break;

    case STU_FIELD_SCORE: {                     /* 分数：0 ~ 100 的浮点数 */
        float score;
        if(!data_parse_float(text, &score)) return 0;
        if(score < 0.0f || score > 100.0f) return 0;
        student->score = score;
        /* 分数变了，排名要跟着重算（注意：该函数会按分数降序重排数组） */
        recalculate_ranking_by_score(getStudentArray(), getStudentCount());
        break;
    }

    case STU_FIELD_GPA: {                       /* 绩点：0 ~ 4 的浮点数 */
        float gpa;
        if(!data_parse_float(text, &gpa)) return 0;
        if(gpa < 0.0f || gpa > 4.0f) return 0;
        student->gpa = gpa;
        break;
    }

    case STU_FIELD_RANK:                        /* 排名：0 ~ 65535（uint16_t 范围） */
        if(!data_parse_uint64(text, &value) || value > UINT16_MAX) return 0;
        student->rank = (uint16_t)value;
        break;

    default:
        return 0;
    }

    data_rebuild_display();
    return 1;
}

/* ------------------------- 4.4 筛选 ------------------------- */

/**
 * 获取某个字段可筛选的不同选项。
 *
 * 规则：
 *   年级 / 班级 → 数据中出现过的不同取值；
 *   性别        → "男" / "女"；
 *   分数        → 每 10.0 分一段，例如 "0-10"、"80-90"、"100-110"；
 *   其他字段    → 不支持筛选，返回 0。
 *
 * 选项按“数字部分优先”的自然顺序升序排列。
 * 若选项个数超过 max_opts，只写入前 max_opts 个，但返回值仍是真实选项数。
 *
 * @return 选项个数。
 */
int data_get_filter_options(int field, char options[][MAX_FIELD_LEN], int max_opts)
{
    int option_count = 0;
    int visible;

    data_ensure_initialized();

    if(options == NULL || max_opts <= 0) return 0;
    if(field != STU_FIELD_GRADE && field != STU_FIELD_CLASS &&
       field != STU_FIELD_GENDER && field != STU_FIELD_SCORE) return 0;

    /* 第一步：遍历所有学生，收集去重后的候选选项 */
    for(int i = 0; i < student_count; i++) {
        char candidate[MAX_FIELD_LEN];
        int exists = 0;

        switch(field) {
        case STU_FIELD_SCORE:
            data_score_range_text(students[i].score, candidate, sizeof(candidate));
            break;
        case STU_FIELD_GENDER:
            snprintf(candidate, sizeof(candidate), "%s", students[i].gender ? "男" : "女");
            break;
        case STU_FIELD_GRADE:
            snprintf(candidate, sizeof(candidate), "%s", students[i].grade);
            break;
        default:   /* STU_FIELD_CLASS */
            snprintf(candidate, sizeof(candidate), "%s", students[i].class_name);
            break;
        }

        if(candidate[0] == '\0') continue;   /* 空字段不作为筛选选项 */

        for(int j = 0; j < option_count; j++) {
            if(strcmp(options[j], candidate) == 0) {
                exists = 1;
                break;
            }
        }
        if(exists) continue;

        if(option_count < max_opts) {
            snprintf(options[option_count], MAX_FIELD_LEN, "%s", candidate);
        }
        option_count++;
    }

    /* 第二步：把已写入缓冲区的部分按自然顺序插入排序，让下拉列表更好读 */
    visible = option_count < max_opts ? option_count : max_opts;
    for(int i = 1; i < visible; i++) {
        char key[MAX_FIELD_LEN];
        int j;

        snprintf(key, sizeof(key), "%s", options[i]);
        for(j = i - 1; j >= 0 && compare_text_natural(options[j], key) > 0; j--) {
            snprintf(options[j + 1], MAX_FIELD_LEN, "%s", options[j]);
        }
        snprintf(options[j + 1], MAX_FIELD_LEN, "%s", key);
    }

    return option_count;
}

/**
 * 应用筛选：只显示 field 等于 option 的数据（分数段按区间判断）。
 * 调用后 data_get_display_count() / data_get_display_index() 立即反映筛选结果。
 *
 * @param field  仅支持 STU_FIELD_GRADE / CLASS / GENDER / SCORE
 * @param option 选项文本，通常来自 data_get_filter_options()
 * @return 1 成功；0 失败（字段不支持筛选或选项为空）。
 */
int data_apply_filter(int field, const char *option)
{
    data_ensure_initialized();

    if(field != STU_FIELD_GRADE && field != STU_FIELD_CLASS &&
       field != STU_FIELD_GENDER && field != STU_FIELD_SCORE) return 0;
    if(option == NULL || option[0] == '\0') return 0;

    filter_field = field;
    snprintf(filter_option, sizeof(filter_option), "%s", option);
    data_rebuild_display();
    return 1;
}

/** 清除筛选条件，恢复显示全部数据。 */
void data_clear_filter(void)
{
    data_ensure_initialized();

    filter_field = -1;
    filter_option[0] = '\0';
    data_rebuild_display();
}

/* ------------------------- 4.5 搜索 ------------------------- */

/**
 * 在当前显示行中搜索关键字，返回匹配到的“显示行号”。
 * 搜索只用于界面高亮，不会修改筛选状态，也不会改变显示行数。
 *
 * 匹配范围是 8 个数据列，其中数值列会先格式化成文本再匹配，
 * 例如关键字 "88" 可以同时命中 "88.5" 分的学生。
 *
 * @param keyword  关键字，允许为空串（空串匹配所有显示行）
 * @param out_rows 输出匹配到的显示行号数组
 * @param max_rows out_rows 容量
 * @return 匹配数量（可能大于 max_rows，此时只写入前 max_rows 个）。
 */
int data_find_matches(const char *keyword, int *out_rows, int max_rows)
{
    char id_text[32];
    char score_text[32];
    char gpa_text[32];
    char rank_text[32];
    int match_count = 0;

    data_ensure_initialized();

    if(keyword == NULL) keyword = "";
    if(out_rows == NULL || max_rows <= 0) return 0;

    for(int row = 0; row < display_count; row++) {
        const StudentCSV *student = &students[display_indexes[row]];
        int hit;

        snprintf(id_text, sizeof(id_text), "%" PRIu64, student->id);
        snprintf(score_text, sizeof(score_text), "%.1f", student->score);
        snprintf(gpa_text, sizeof(gpa_text), "%.1f", student->gpa);
        snprintf(rank_text, sizeof(rank_text), "%" PRIu16, student->rank);

        hit = strstr(student->grade, keyword) != NULL ||
              strstr(student->class_name, keyword) != NULL ||
              strstr(id_text, keyword) != NULL ||
              strstr(student->name, keyword) != NULL ||
              strstr(student->gender ? "男" : "女", keyword) != NULL ||
              strstr(score_text, keyword) != NULL ||
              strstr(gpa_text, keyword) != NULL ||
              strstr(rank_text, keyword) != NULL;

        if(hit) {
            if(match_count < max_rows) out_rows[match_count] = row;
            match_count++;
        }
    }

    return match_count;
}

/* ------------------------- 4.6 排序 ------------------------- */

/**
 * 按字段排序学生数组并刷新显示行映射。
 * 排序规则见 compare_students()（自然字符串 / 数值 / bool 三种比较方式）。
 *
 * @param field     字段编号（0~7）
 * @param ascending true = 正序，false = 倒序
 */
void data_sort_students(int field, bool ascending)
{
    data_ensure_initialized();

    if(field < 0 || field >= STU_FIELD_COUNT) return;

    sortStudents(field, ascending);   /* 直接对内存数组排序 */
    data_rebuild_display();           /* 数组顺序变了，显示行映射必须重建 */
}

/* ---------------------- 4.7 帮助图片 ---------------------- */

/**
 * 帮助图片路径缓存。
 * 使用二维数组保存每一张图片的完整路径，这样返回给 UI 的指针
 * 在程序运行期间一直有效（UI 会把该指针保存到 imagebutton 中）。
 */
static char help_image_path[DATA_HELP_IMAGE_COUNT][64];
static int  help_image_path_ready;   /* 路径是否已经生成过 */

/** 返回帮助图片总张数。 */
int ui_help_get_image_count(void)
{
    return DATA_HELP_IMAGE_COUNT;
}

/**
 * 返回第 index 张帮助图片源（形如 "A:help/help1.png"）。
 * @return 路径字符串指针；index 越界时返回 NULL。
 */
const void *ui_help_get_image_src(int index)
{
    if(index < 0 || index >= DATA_HELP_IMAGE_COUNT) return NULL;

    /* 只在第一次调用时生成路径，之后复用同一批静态缓冲区 */
    if(!help_image_path_ready) {
        for(int i = 0; i < DATA_HELP_IMAGE_COUNT; i++) {
            snprintf(help_image_path[i], sizeof(help_image_path[i]),
                     "%shelp%d.png", DATA_HELP_IMAGE_FOLDER, i + 1);
        }
        help_image_path_ready = 1;
    }

    return help_image_path[index];
}
