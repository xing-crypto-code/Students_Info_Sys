/**
 * @file core.c
 * @brief 学生成绩信息管理系统 —— 数据核心层实现（纯 C，不依赖 LVGL）。
 *
 * 本文件由原来的 stu.c / file.c / read_students_csv.c / write_students_csv.c
 * 整理合并而来，去掉了重复和用不到的控制台接口，只保留：
 *   1. 学生结构体数组；
 *   2. CSV 读取、写入；
 *   3. UI 层需要的 data_* / ui_help_* 接口；
 *   4. 筛选、搜索、排序、删除、编辑等核心逻辑。
 *
 * 所有 data_* 函数在访问数据前都会调用 data_ensure_initialized()，
 * 第一次调用时自动尝试从默认 CSV 路径加载数据，找不到就从空数据开始。
 */

#include "core.h"

#include <ctype.h>
#include <inttypes.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

 /* ==================================================================
  * 一、内部状态
  * ================================================================== */

  /** 学生数组：整个数据层唯一的数据源。 */
static StudentCSV s_students[MAX_STUDENTS];

/** 当前学生人数，范围 0 ~ MAX_STUDENTS。 */
static int s_count = 0;

/** 显示行映射表：s_display_indexes[row] = 该显示行对应的 s_students 下标。 */
static int s_display_indexes[MAX_STUDENTS];

/** 当前显示行数，等于 s_display_indexes 的有效元素个数。 */
static int s_display_count = 0;

/** 是否已经完成过首次自动加载。 */
static int s_initialized = 0;

/** 当前筛选字段；-1 表示未筛选。 */
static int s_filter_field = -1;

/** 当前筛选选项文本。 */
static char s_filter_option[MAX_FIELD_LEN] = { 0 };

/** 导入 CSV 时保存的表头；导出时优先写回该表头。 */
static char s_csv_header[MAX_LINE_LEN] = STUDENT_CSV_HEADER;

/** qsort 比较函数使用的排序字段。 */
static int s_sort_field = 0;

/** qsort 比较函数使用的排序方向：1=正序，-1=倒序。 */
static int s_sort_direction = 1;

/* ==================================================================
 * 二、通用字符串 / 文本工具
 * ================================================================== */

 /**
  * @brief 去掉字符串首尾空白字符（直接在原字符串上修改）。
  *
  * 空白包括空格、Tab、回车、换行等，CSV 字段解析和用户输入解析都会用到。
  */
static void core_trim_whitespace(char* text)
{
    char* start = text;
    char* end;

    if (text == NULL) return;

    /* 跳过开头的空白字符 */
    while (*start != '\0' && isspace((unsigned char)*start)) start++;

    /* 全部是空白时直接变成空字符串 */
    if (*start == '\0') {
        text[0] = '\0';
        return;
    }

    /* 从末尾向前跳过空白字符 */
    end = start + strlen(start) - 1;
    while (end >= start && isspace((unsigned char)*end)) end--;
    end[1] = '\0';

    /* 如果开头有空白，把有效内容移动到数组首位 */
    if (start != text) memmove(text, start, strlen(start) + 1);
}

/**
 * @brief 去掉 UTF-8 BOM（EF BB BF）。
 *
 * Windows 记事本等编辑器保存 UTF-8 文件时可能在开头添加 BOM，
 * 它会让第一列表头解析出现异常，因此导入前需要去掉。
 */
static void core_remove_utf8_bom(char* text)
{
    unsigned char* p = (unsigned char*)text;

    if (text == NULL) return;
    if (p[0] == 0xEF && p[1] == 0xBB && p[2] == 0xBF) {
        memmove(text, text + 3, strlen(text + 3) + 1);
    }
}

/**
 * @brief 把字符串安全复制到目标缓冲区，并去掉首尾空白。
 *
 * 该函数用于把 UI 输入写入学生字段，保证不会越界且不会带空白。
 */
static void core_copy_trimmed(char* dst, size_t dst_size, const char* src)
{
    size_t i = 0;

    if (dst == NULL || dst_size == 0) return;
    dst[0] = '\0';
    if (src == NULL) return;

    /* 跳过源字符串开头的空白 */
    while (*src != '\0' && isspace((unsigned char)*src)) src++;

    /* 复制有效字符，最多复制 dst_size - 1 个，保留 '\0' 位置 */
    while (*src != '\0' && i + 1 < dst_size) {
        dst[i] = *src;
        i++;
        src++;
    }
    dst[i] = '\0';

    /* 去掉复制结果末尾的空白 */
    while (i > 0 && isspace((unsigned char)dst[i - 1])) {
        dst[i - 1] = '\0';
        i--;
    }
}

/**
 * @brief 判断字符串是否包含关键字，ASCII 字符不区分大小写。
 *
 * 中文等多字节字符按原始字节比较，因此中文搜索仍然是精确匹配。
 *
 * @return 1 包含；0 不包含。
 */
static int core_contains_ci(const char* haystack, const char* needle)
{
    size_t needle_len;

    if (haystack == NULL || needle == NULL) return 0;
    needle_len = strlen(needle);
    if (needle_len == 0) return 0;

    for (; *haystack != '\0'; haystack++) {
        size_t i = 0;

        /* 逐字符比较，直到 keyword 全部匹配或遇到不一致 */
        while (i < needle_len && haystack[i] != '\0' &&
            tolower((unsigned char)haystack[i]) ==
            tolower((unsigned char)needle[i])) {
            i++;
        }

        if (i == needle_len) return 1;
    }

    return 0;
}

/**
 * @brief 自然字符串比较：优先比较字符串中出现的数字部分。
 *
 * 例如 "计算机2班" 会排在 "计算机10班" 之前。
 * 如果两串数字相同或没有数字，则按普通字符顺序比较。
 *
 * @return <0 表示 left 在前；0 表示相等；>0 表示 left 在后。
 */
static int core_compare_text_natural(const char* left, const char* right)
{
    while (*left != '\0' && *right != '\0') {
        /* 两边同时遇到数字时，先提取完整数字按数值比较 */
        if (*left >= '0' && *left <= '9' && *right >= '0' && *right <= '9') {
            char* left_end = NULL;
            char* right_end = NULL;
            unsigned long left_number = strtoul(left, &left_end, 10);
            unsigned long right_number = strtoul(right, &right_end, 10);

            if (left_number != right_number) {
                return left_number < right_number ? -1 : 1;
            }

            left = left_end;
            right = right_end;
            continue;
        }

        /* 非数字位置按普通字节比较 */
        if ((unsigned char)*left != (unsigned char)*right) {
            return (unsigned char)*left < (unsigned char)*right ? -1 : 1;
        }

        left++;
        right++;
    }

    if (*left == *right) return 0;
    return *left == '\0' ? -1 : 1;
}

/* ==================================================================
 * 三、文本解析工具（用户输入 / CSV 字段）
 * ================================================================== */

 /**
  * @brief 解析一个完整的非负整数文本。
  *
  * 允许前后空白，但不允许负号、多余字符或空字符串。
  *
  * @return 1 成功；0 失败。
  */
static int core_parse_uint64_text(const char* text, uint64_t* out)
{
    char* end = NULL;
    unsigned long long value;

    if (text == NULL || out == NULL) return 0;
    while (*text != '\0' && isspace((unsigned char)*text)) text++;
    if (*text == '\0' || *text == '-') return 0;

    value = strtoull(text, &end, 10);
    if (end == text) return 0;

    while (*end != '\0' && isspace((unsigned char)*end)) end++;
    if (*end != '\0') return 0;

    *out = (uint64_t)value;
    return 1;
}

/**
 * @brief 解析一个完整的浮点数文本（用于分数、绩点）。
 *
 * @return 1 成功；0 失败。
 */
static int core_parse_float_text(const char* text, float* out)
{
    char* end = NULL;
    float value;

    if (text == NULL || out == NULL) return 0;
    while (*text != '\0' && isspace((unsigned char)*text)) text++;
    if (*text == '\0') return 0;

    value = strtof(text, &end);
    if (end == text) return 0;

    while (*end != '\0' && isspace((unsigned char)*end)) end++;
    if (*end != '\0') return 0;

    *out = value;
    return 1;
}

/**
 * @brief 解析性别文本。
 * 支持：男/女、M/F、m/f、1/0、true/false。
 *
 * @return 1 成功；0 无法识别。
 */
static int core_parse_gender_text(const char* text, bool* out)
{
    char buf[16];
    size_t i = 0;

    if (text == NULL || out == NULL) return 0;

    while (*text != '\0' && isspace((unsigned char)*text)) text++;
    while (*text != '\0' && i + 1 < sizeof(buf)) {
        buf[i] = *text;
        i++;
        text++;
    }
    buf[i] = '\0';
    while (i > 0 && isspace((unsigned char)buf[i - 1])) {
        buf[i - 1] = '\0';
        i--;
    }

    if (strcmp(buf, "男") == 0 || strcmp(buf, "M") == 0 ||
        strcmp(buf, "m") == 0 || strcmp(buf, "1") == 0 ||
        strcmp(buf, "true") == 0 || strcmp(buf, "TRUE") == 0) {
        *out = true;
        return 1;
    }

    if (strcmp(buf, "女") == 0 || strcmp(buf, "F") == 0 ||
        strcmp(buf, "f") == 0 || strcmp(buf, "0") == 0 ||
        strcmp(buf, "false") == 0 || strcmp(buf, "FALSE") == 0) {
        *out = false;
        return 1;
    }

    return 0;
}

/**
 * @brief 把一个学生的某个字段格式化成文本。
 *
 * 搜索、筛选选项收集都要把数值字段先转换成字符串，因此统一放在这里。
 *
 * @param student 学生指针
 * @param field   字段编号（stu_field_t）
 * @param buf     输出缓冲区
 * @param size    输出缓冲区大小
 * @return 返回 buf，方便直接作为表达式使用。
 */
static const char* core_field_text(const StudentCSV* student, int field,
    char* buf, size_t size)
{
    if (buf == NULL || size == 0) return "";
    buf[0] = '\0';
    if (student == NULL) return buf;

    switch (field) {
    case STU_FIELD_GRADE:
        snprintf(buf, size, "%s", student->grade);
        break;
    case STU_FIELD_CLASS:
        snprintf(buf, size, "%s", student->class_name);
        break;
    case STU_FIELD_ID:
        snprintf(buf, size, "%" PRIu64, student->id);
        break;
    case STU_FIELD_NAME:
        snprintf(buf, size, "%s", student->name);
        break;
    case STU_FIELD_GENDER:
        snprintf(buf, size, "%s", student->gender ? "男" : "女");
        break;
    case STU_FIELD_SCORE:
        snprintf(buf, size, "%.1f", student->score);
        break;
    case STU_FIELD_GPA:
        snprintf(buf, size, "%.1f", student->gpa);
        break;
    case STU_FIELD_RANK:
        snprintf(buf, size, "%" PRIu16, student->rank);
        break;
    default:
        break;
    }

    return buf;
}

/**
 * @brief 构造“年级-班级”组合筛选文本。
 *
 * 用于班级筛选，使不同年级的同名班级可以区分开来。
 */
static void core_grade_class_text(const StudentCSV* student,
    char* buf, size_t size)
{
    if (buf == NULL || size == 0) return;
    if (student == NULL) {
        buf[0] = '\0';
        return;
    }

    snprintf(buf, size, "%s-%s", student->grade, student->class_name);
}

/* ==================================================================
 * 四、CSV 读取与写入
 * ================================================================== */

#define CORE_CSV_COLUMNS 8   /* CSV 共 8 列：年级、班级、学号、姓名、性别、分数、绩点、排名 */

 /**
  * @brief 把一行 CSV 按逗号拆分成 8 个字段。
  *
  * 本实现面向课程设计的常见 CSV 格式，不处理字段内部带逗号的引号转义。
  *
  * @param line       待拆分的一行（会被修改，逗号替换成 '\0'）
  * @param fields     输出字段指针数组
  * @param max_fields 字段数组容量
  * @return 实际拆出的字段数量
  */
static int core_split_csv_line(char* line, char** fields, int max_fields)
{
    int count = 0;
    char* p = line;

    if (line == NULL || fields == NULL) return 0;

    while (count < max_fields && *p != '\0') {
        /* 跳过连续逗号产生的空字段 */
        if (*p == ',') {
            p++;
            continue;
        }

        fields[count] = p;
        count++;

        /* 找到当前字段结尾的逗号，并把它替换成 '\0' */
        while (*p != '\0' && *p != ',') p++;
        if (*p == ',') {
            *p = '\0';
            p++;
        }
    }

    return count;
}

/**
 * @brief 把一行 CSV 文本解析成 StudentCSV 结构体。
 *
 * @return 1 成功；0 失败（字段数不足或类型解析失败）。
 */
static int core_parse_csv_student(char* line, StudentCSV* student)
{
    char* fields[CORE_CSV_COLUMNS];
    int field_count;

    if (line == NULL || student == NULL) return 0;

    field_count = core_split_csv_line(line, fields, CORE_CSV_COLUMNS);
    if (field_count < CORE_CSV_COLUMNS) return 0;

    /* 逐个字段去掉首尾空白 */
    for (int i = 0; i < field_count; i++) {
        core_trim_whitespace(fields[i]);
    }

    /* 字符串字段：复制到结构体中 */
    core_copy_trimmed(student->grade, sizeof(student->grade), fields[0]);
    core_copy_trimmed(student->class_name, sizeof(student->class_name), fields[1]);
    core_copy_trimmed(student->name, sizeof(student->name), fields[3]);

    /* 数值字段：分别解析为正确类型 */
    if (!core_parse_uint64_text(fields[2], &student->id)) return 0;
    if (!core_parse_gender_text(fields[4], &student->gender)) return 0;
    if (!core_parse_float_text(fields[5], &student->score)) return 0;
    if (!core_parse_float_text(fields[6], &student->gpa)) return 0;

    {
        uint64_t rank_value = 0;
        {
            uint64_t rank_value = 0;

            /* 排名允许为空，空值时按 0 处理 */
            if (fields[7][0] != '\0') {
                if (!core_parse_uint64_text(fields[7], &rank_value)) return 0;
                if (rank_value > UINT16_MAX) return 0;
            }

            student->rank = (uint16_t)rank_value;
        }
    }

    return 1;
}

/**
 * @brief 从 CSV 文件读取学生数据。
 *
 * 第一行作为表头写入 header；后续行逐行解析，解析失败的行直接跳过。
 *
 * @param filename    CSV 文件路径
 * @param out         输出学生数组
 * @param capacity    输出数组容量
 * @param header      表头输出缓冲区
 * @param header_size 表头缓冲区大小
 * @return 成功返回读取到的学生条数；文件打不开返回 -1。
 */
static int core_read_csv(const char* filename, StudentCSV* out, int capacity,
    char* header, size_t header_size)
{
    FILE* fp;
    char line[MAX_LINE_LEN];
    int count = 0;

    if (filename == NULL || out == NULL || header == NULL) return -1;

    fp = fopen(filename, "r");
    if (fp == NULL) {
        perror(filename);
        return -1;
    }
    /* 读取第一行作为表头 */
    if (header_size > 0) {
        if (fgets(header, (int)header_size, fp) != NULL) {
            core_remove_utf8_bom(header);
            core_trim_whitespace(header);
        }
        else {
            header[0] = '\0';
        }
    }

    /* 逐行读取学生数据 */
    while (count < capacity && fgets(line, sizeof(line), fp) != NULL) {
        core_trim_whitespace(line);

        /* 跳过空行 */
        if (line[0] == '\0') continue;

        /* 解析失败的行直接跳过，避免因为一条脏数据导致导入失败 */
        if (!core_parse_csv_student(line, &out[count])) continue;
        count++;
    }

    fclose(fp);
    return count;
}

/**
 * @brief 把学生数组写入 CSV 文件（UTF-8 with BOM）。
 *
 * @return 成功返回写入条数；文件打不开返回 -1。
 */
static int core_write_csv(const char* filename, const StudentCSV* students,
    int count, const char* header)
{
    FILE* fp;

    if (filename == NULL) return -1;
    if (count < 0) return -1;
    if (count > 0 && students == NULL) return -1;
    if (header == NULL || header[0] == '\0') header = STUDENT_CSV_HEADER;

    /* 用二进制模式写，方便在文件开头写 UTF-8 BOM */
    fp = fopen(filename, "wb");
    if (fp == NULL) return -1;

    /* UTF-8 BOM，方便 Windows 记事本 / Excel 正确识别中文 */
    fputs("\xEF\xBB\xBF", fp);
    fputs(header, fp);
    fputs("\r\n", fp);

    /* 逐条写数据行 */
    for (int i = 0; i < count; i++) {
        fprintf(fp, "%s,%s,%" PRIu64 ",%s,%s,%.1f,%.1f,%" PRIu16 "\r\n",
            students[i].grade,
            students[i].class_name,
            students[i].id,
            students[i].name,
            students[i].gender ? "男" : "女",
            students[i].score,
            students[i].gpa,
            students[i].rank);
    }

    fclose(fp);
    return count;
}

/* ==================================================================
 * 五、排序比较函数
 * ================================================================== */

 /**
  * @brief 按分数从高到低排序（内部排名计算使用）。
  */
static int core_cmp_score_desc(const void* a, const void* b)
{
    const StudentCSV* left = (const StudentCSV*)a;
    const StudentCSV* right = (const StudentCSV*)b;

    if (left->score < right->score) return 1;
    if (left->score > right->score) return -1;
    return 0;
}

/**
 * @brief 按分数从高到低重新计算所有学生的 rank。
 *
 * 本函数只用于导入、删除、修改分数后的排名维护，不属于 UI 接口。
 */
static void core_recalc_rank(StudentCSV* students, int count)
{
    if (students == NULL || count <= 0) return;

    qsort(students, (size_t)count, sizeof(StudentCSV), core_cmp_score_desc);
    for (int i = 0; i < count; i++) {
        students[i].rank = (uint16_t)(i + 1);
    }
}

/**
 * @brief qsort 使用的通用学生比较函数。
 *
 * 读取文件级变量 s_sort_field / s_sort_direction 决定排序规则：
 *   年级/班级/姓名 → 自然字符串比较；
 *   学号/分数/绩点/排名 → 数值比较；
 *   性别 → true(男)=1、false(女)=0。
 */
static int core_compare_students(const void* left_ptr, const void* right_ptr)
{
    const StudentCSV* left = (const StudentCSV*)left_ptr;
    const StudentCSV* right = (const StudentCSV*)right_ptr;
    int result = 0;

    switch (s_sort_field) {
    case STU_FIELD_GRADE:
        result = core_compare_text_natural(left->grade, right->grade);
        break;
    case STU_FIELD_CLASS:
        /* 先按班级排序 */
        result = core_compare_text_natural(left->class_name, right->class_name);

        /* 班级相同时，再按年级排序，保证不同年级的同名班级顺序稳定 */
        if (result == 0) {
            result = core_compare_text_natural(left->grade, right->grade);
        }
        break;
    case STU_FIELD_ID:
        result = left->id < right->id ? -1 : (left->id > right->id ? 1 : 0);
        break;
    case STU_FIELD_NAME:
        result = core_compare_text_natural(left->name, right->name);
        break;
    case STU_FIELD_GENDER:
        result = left->gender == right->gender ? 0 : (left->gender ? -1 : 1);
        break;
    case STU_FIELD_SCORE:
        result = left->score < right->score ? -1 : (left->score > right->score ? 1 : 0);
        break;
    case STU_FIELD_GPA:
        result = left->gpa < right->gpa ? -1 : (left->gpa > right->gpa ? 1 : 0);
        break;
    case STU_FIELD_RANK:
        result = left->rank < right->rank ? -1 : (left->rank > right->rank ? 1 : 0);
        break;
    default:
        break;
    }

    return result * s_sort_direction;
}

/* ==================================================================
 * 六、筛选与显示行映射
 * ================================================================== */

 /** 当前是否存在有效的筛选条件。 */
static int core_filter_active(void)
{
    return s_filter_field >= 0 && s_filter_option[0] != '\0';
}

/**
 * @brief 把分数转换成每 10.0 分一段的区间文本。
 * 例如 85.5 -> "80-90"，0 -> "0-10"，100 -> "100-110"。
 */
static void core_score_range_text(float score, char* buf, size_t buf_size)
{
    int lower;
    int upper;

    if (score < 0.0f) score = 0.0f;
    lower = (int)(score / 10.0f) * 10;
    upper = lower + 10;
    snprintf(buf, buf_size, "%d-%d", lower, upper);
}

/**
 * @brief 判断分数是否落在筛选选项描述的区间内。
 *
 * 区间约定为左闭右开 [lower, upper)；对于 100 分，
 * 允许其命中 "100-110" 区间。
 */
static int core_score_match(float score, const char* option)
{
    float lower = 0.0f;
    float upper = 0.0f;

    if (option == NULL) return 0;
    if (sscanf(option, "%f-%f", &lower, &upper) != 2) return 0;
    if (score >= lower && score < upper) return 1;
    return (upper >= 100.0f && score == upper);
}

/**
 * @brief 判断一名学生是否满足当前筛选条件。
 * @return 1 命中；0 不命中。
 */
static int core_match_filter(const StudentCSV* student)
{
    char value[MAX_FIELD_LEN];

    if (student == NULL) return 0;
    if (!core_filter_active()) return 1;
    if (s_filter_field == STU_FIELD_SCORE) {
        return core_score_match(student->score, s_filter_option);
    }

    switch (s_filter_field) {
    case STU_FIELD_GRADE:
        core_copy_trimmed(value, sizeof(value), student->grade);
        break;
    case STU_FIELD_CLASS:
        core_grade_class_text(student, value, sizeof(value));
        break;
    case STU_FIELD_GENDER:
        snprintf(value, sizeof(value), "%s", student->gender ? "男" : "女");
        break;
    default:
        return 1;
    }

    return strcmp(value, s_filter_option) == 0;
}

/**
 * @brief 重建显示行映射表。
 *
 * 导入、删除、修改、排序、筛选之后都必须调用本函数，
 * 保证 data_get_display_count() / data_get_display_index() 返回最新结果。
 */
static void core_rebuild_display(void)
{
    s_display_count = 0;

    for (int i = 0; i < s_count && s_display_count < MAX_STUDENTS; i++) {
        if (core_match_filter(&s_students[i])) {
            s_display_indexes[s_display_count] = i;
            s_display_count++;
        }
    }
}

/* ==================================================================
 * 七、首次自动加载
 * ================================================================== */

 /**
  * @brief 首次被 UI 访问时自动尝试加载默认 CSV 数据。
  *
  * 依次尝试几个常见路径；全部失败则从空数据开始。
  */
static void data_load_default(void)
{
    static const char* const default_paths[] = {
        "Data/students.csv",   /* 控制台版本常用的数据目录 */
        "students.csv",        /* 程序当前目录 */
        "lvgl/students.csv"    /* 模拟器目录 */
    };
    StudentCSV loaded[MAX_STUDENTS];
    int count = -1;

    for (size_t i = 0; i < sizeof(default_paths) / sizeof(default_paths[0]); i++) {
        count = core_read_csv(default_paths[i], loaded, MAX_STUDENTS,
            s_csv_header, sizeof(s_csv_header));
        if (count >= 0) break;
    }

    if (count < 0) {
        s_count = 0;
        snprintf(s_csv_header, sizeof(s_csv_header), "%s", STUDENT_CSV_HEADER);
    }
    else {
        memcpy(s_students, loaded, (size_t)count * sizeof(StudentCSV));
        s_count = count;
        core_recalc_rank(s_students, s_count);
    }

    s_filter_field = -1;
    s_filter_option[0] = '\0';
    core_rebuild_display();
    s_initialized = 1;
}

/** 保证首次访问时数据已自动加载。 */
static void data_ensure_initialized(void)
{
    if (!s_initialized) data_load_default();
}

/* ==================================================================
 * 八、UI 数据层接口实现
 * ================================================================== */

 /* ------------------------- 8.1 数据访问 ------------------------- */

 /** 获取当前内存中的学生总数。 */
int data_get_count(void)
{
    data_ensure_initialized();
    return s_count;
}

/** 获取学生数组首地址。 */
StudentCSV* data_get_all(void)
{
    data_ensure_initialized();
    return s_students;
}

/** 获取网格当前应显示的数据行数。 */
int data_get_display_count(void)
{
    data_ensure_initialized();

    /* 无数据且无筛选时显示 200 行空网格 */
    if (s_count == 0 && !core_filter_active()) return DATA_DEFAULT_DISPLAY_ROWS;
    return s_display_count;
}

/** 把显示行号映射为学生数组下标。 */
int data_get_display_index(int display_row)
{
    data_ensure_initialized();

    if (display_row < 0 || display_row >= s_display_count) return -1;
    return s_display_indexes[display_row];
}

/* ---------------------- 8.2 导入 / 导出 ---------------------- */

/** 从 CSV 导入数据，整体覆盖内存中的数据。 */
int data_import_csv(const char* path)
{
    StudentCSV loaded[MAX_STUDENTS];
    int count;

    if (path == NULL || path[0] == '\0') return -1;

    /* 先读入临时数组，读取失败时不破坏当前数据 */
    count = core_read_csv(path, loaded, MAX_STUDENTS, s_csv_header, sizeof(s_csv_header));
    if (count < 0) return -1;

    memcpy(s_students, loaded, (size_t)count * sizeof(StudentCSV));
    s_count = count;

    /* CSV 中的排名可能只是占位值，这里统一按分数重新计算 */
    core_recalc_rank(s_students, s_count);

    /* 数据整体换掉后，旧筛选条件已无意义 */
    s_filter_field = -1;
    s_filter_option[0] = '\0';
    core_rebuild_display();
    s_initialized = 1;
    return s_count;
}

/** 把全部学生数据导出为 CSV。 */
int data_export_csv(const char* path)
{
    data_ensure_initialized();

    if (path == NULL || path[0] == '\0') return -1;
    return core_write_csv(path, s_students, s_count, s_csv_header);
}

/* ---------------------- 8.3 删除 / 修改 ---------------------- */

/** 按数组下标删除一名学生，后续元素整体前移。 */
int data_delete_student(int index)
{
    data_ensure_initialized();

    if (index < 0 || index >= s_count) return 0;

    /* 从 index 开始，用后一个元素覆盖前一个元素 */
    for (int i = index; i + 1 < s_count; i++) {
        s_students[i] = s_students[i + 1];
    }
    s_count--;

    /* 删除后重新计算排名并刷新显示映射 */
    core_recalc_rank(s_students, s_count);
    core_rebuild_display();
    return 1;
}

/**
 * 修改指定学生的一个字段。
 * 所有文本先解析成正确类型，校验通过后才写回数据。
 */
int data_update_student_field(int index, int field, const char* text)
{
    StudentCSV* student;
    uint64_t value = 0;

    data_ensure_initialized();

    if (index < 0 || index >= s_count || text == NULL) return 0;
    if (field < 0 || field >= STU_FIELD_COUNT) return 0;

    student = &s_students[index];

    switch (field) {
    case STU_FIELD_GRADE:
        core_copy_trimmed(student->grade, sizeof(student->grade), text);
        break;

    case STU_FIELD_CLASS:
        core_copy_trimmed(student->class_name, sizeof(student->class_name), text);
        break;

    case STU_FIELD_ID:
        if (!core_parse_uint64_text(text, &value) || value == 0) return 0;
        /* 学号必须全局唯一 */
        for (int i = 0; i < s_count; i++) {
            if (i != index && s_students[i].id == value) return 0;
        }
        student->id = value;
        break;

    case STU_FIELD_NAME:
        core_copy_trimmed(student->name, sizeof(student->name), text);
        if (student->name[0] == '\0') return 0;
        break;

    case STU_FIELD_GENDER:
        if (!core_parse_gender_text(text, &student->gender)) return 0;
        break;

    case STU_FIELD_SCORE: {
        float score = 0.0f;
        if (!core_parse_float_text(text, &score)) return 0;
        if (score < 0.0f || score > 100.0f) return 0;
        student->score = score;
        /* 分数变化会影响排名，统一重算 */
        core_recalc_rank(s_students, s_count);
        break;
    }

    case STU_FIELD_GPA: {
        float gpa = 0.0f;
        if (!core_parse_float_text(text, &gpa)) return 0;
        if (gpa < 0.0f || gpa > 4.0f) return 0;
        student->gpa = gpa;
        break;
    }

    case STU_FIELD_RANK:
        if (!core_parse_uint64_text(text, &value) || value > UINT16_MAX) return 0;
        student->rank = (uint16_t)value;
        break;

    default:
        return 0;
    }

    /* 字段可能影响筛选结果，统一重建显示映射 */
    core_rebuild_display();
    return 1;
}

/* ------------------------- 8.4 筛选 ------------------------- */

/**
 * 获取某个字段可用于筛选的不同选项。
 * 返回值可能大于 max_opts，调用者可用返回值判断真实选项数。
 */
int data_get_filter_options(int field, char options[][MAX_FIELD_LEN], int max_opts)
{
    int option_count = 0;
    int visible;

    data_ensure_initialized();

    if (options == NULL || max_opts <= 0) return 0;
    if (field != STU_FIELD_GRADE && field != STU_FIELD_CLASS &&
        field != STU_FIELD_GENDER && field != STU_FIELD_SCORE) return 0;

    /* 遍历所有学生，收集去重后的选项 */
    for (int i = 0; i < s_count; i++) {
        char candidate[MAX_FIELD_LEN];
        int exists = 0;

        if (field == STU_FIELD_SCORE) {
            core_score_range_text(s_students[i].score, candidate, sizeof(candidate));
        }
        else if (field == STU_FIELD_CLASS) {
            /* 班级筛选使用“年级-班级”组合，避免不同年级的同名班级合并 */
            core_grade_class_text(&s_students[i], candidate, sizeof(candidate));
        }
        else {
            core_field_text(&s_students[i], field, candidate, sizeof(candidate));
        }


        if (candidate[0] == '\0') continue;

        /* 已经在候选列表中则跳过 */
        for (int j = 0; j < option_count && j < max_opts; j++) {
            if (strcmp(options[j], candidate) == 0) {
                exists = 1;
                break;
            }
        }
        if (exists) continue;

        /* 只把前 max_opts 个写入缓冲区，但 option_count 统计真实数量 */
        if (option_count < max_opts) {
            snprintf(options[option_count], MAX_FIELD_LEN, "%s", candidate);
        }
        option_count++;
    }

    /* 对已写入缓冲区的部分按自然顺序排序，让列表更易读 */
    visible = option_count < max_opts ? option_count : max_opts;
    for (int i = 1; i < visible; i++) {
        char key[MAX_FIELD_LEN];
        int j;

        snprintf(key, sizeof(key), "%s", options[i]);
        for (j = i - 1; j >= 0 && core_compare_text_natural(options[j], key) > 0; j--) {
            /* 把 options[j] 整体后移一位；用 memmove 避免源和目标重叠 */
            memmove(options[j + 1], options[j], strlen(options[j]) + 1);
        }
        memmove(options[j + 1], key, strlen(key) + 1);
    }

    return option_count;
}

/** 应用筛选条件。 */
int data_apply_filter(int field, const char* option)
{
    data_ensure_initialized();

    if (field != STU_FIELD_GRADE && field != STU_FIELD_CLASS &&
        field != STU_FIELD_GENDER && field != STU_FIELD_SCORE) return 0;
    if (option == NULL || option[0] == '\0') return 0;

    s_filter_field = field;
    core_copy_trimmed(s_filter_option, sizeof(s_filter_option), option);
    core_rebuild_display();
    return 1;
}

/** 清除筛选，恢复显示全部数据。 */
void data_clear_filter(void)
{
    data_ensure_initialized();

    s_filter_field = -1;
    s_filter_option[0] = '\0';
    core_rebuild_display();
}

/* ------------------------- 8.5 搜索 ------------------------- */

/**
 * 在当前显示行中搜索关键字。
 *
 * 对每个学生的 8 个字段都做一次包含匹配；匹配到的学生只输出一次。
 * out_rows 写显示行号，方便 UI 直接给对应网格行着色。
 */
int data_find_matches(const char* keyword, int* out_rows, int max_rows)
{
    int match_count = 0;

    data_ensure_initialized();

    if (keyword == NULL || keyword[0] == '\0') return 0;
    if (out_rows == NULL || max_rows <= 0) return 0;

    for (int row = 0; row < s_display_count; row++) {
        const StudentCSV* student = &s_students[s_display_indexes[row]];
        int hit = 0;

        /* 依次检查 8 个字段，只要任意字段包含关键字就算命中 */
        for (int field = 0; field < STU_FIELD_COUNT && !hit; field++) {
            char value[MAX_FIELD_LEN];
            core_field_text(student, field, value, sizeof(value));
            if (core_contains_ci(value, keyword)) hit = 1;
        }

        if (hit) {
            if (match_count < max_rows) out_rows[match_count] = row;
            match_count++;
        }
    }

    return match_count;
}

/* ------------------------- 8.6 排序 ------------------------- */

/** 按指定字段排序学生数组，并刷新显示行映射。 */
void data_sort_students(int field, bool ascending)
{
    data_ensure_initialized();

    if (field < 0 || field >= STU_FIELD_COUNT) return;

    s_sort_field = field;
    s_sort_direction = ascending ? 1 : -1;
    qsort(s_students, (size_t)s_count, sizeof(StudentCSV), core_compare_students);
    core_rebuild_display();
}

/* ---------------------- 8.7 帮助图片 ---------------------- */

/** 帮助图片路径缓存；第一次调用时生成，之后直接复用。 */
static char s_help_image_paths[CORE_HELP_IMAGE_COUNT][128];
static int s_help_image_paths_ready = 0;

/** 返回帮助图片总张数。 */
int ui_help_get_image_count(void)
{
    return CORE_HELP_IMAGE_COUNT;
}

/** 返回第 index 张帮助图片的路径字符串。 */
const void* ui_help_get_image_src(int index)
{
    if (index < 0 || index >= CORE_HELP_IMAGE_COUNT) return NULL;

    if (!s_help_image_paths_ready) {
        for (int i = 0; i < CORE_HELP_IMAGE_COUNT; i++) {
            snprintf(s_help_image_paths[i], sizeof(s_help_image_paths[i]),
                "%shelp%d.png", CORE_HELP_IMAGE_FOLDER, i + 1);
        }
        s_help_image_paths_ready = 1;
    }

    return s_help_image_paths[index];
}
