/*
 * @file core.c
 * @brief 学生成绩信息管理系统 —— 数据核心层实现（纯 C，不依赖 LVGL）。
 *
 * 代码分区：
 *   一、文件级数据与配置；
 *   二、通用字符串 / 文本工具；
 *   三、文本解析工具；
 *   四、CSV 读取与写入；
 *   五、排序比较与排名；
 *   六、筛选与显示行映射；
 *   七、数据初始化；
 *   八、UI 数据层接口实现（数据访问、成绩计算、导入导出、路径解析、
 *       增删改、筛选、搜索、排序、帮助图片、首次运行标志）。
 *
 * 所有 data_* 函数在访问数据前都会调用 data_ensure_initialized()；
 * 程序启动时数据初始化为空，CSV 数据由用户通过“导入”主动加载。
 */

#include "core.h"

#include <ctype.h>
#include <inttypes.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <windows.h>
#include <shlobj.h>
#include <wchar.h>

/* ==================================================================
 * 一、文件级数据与配置
 * ================================================================== */

/* 学生数组：整个数据层唯一的数据源。 */
static StudentCSV s_students[200];

/* 当前学生人数，范围 0 ~ 200。 */
static int s_count = 0;

/* 显示行映射表：s_display_indexes[row] = 该显示行对应的 s_students 下标。 */
static int s_display_indexes[200];

/* 当前显示行数，等于 s_display_indexes 的有效元素个数。 */
static int s_display_count = 0;

/* 是否已经完成首次初始化。 */
static int s_initialized = 0;

/* 当前筛选字段；-1 表示未筛选。 */
static int s_filter_field = -1;

/* 当前筛选选项文本。 */
static char s_filter_option[64] = { 0 };

/* 导入 CSV 时保存的表头；导出时优先写回该表头。 */
static char s_csv_header[1024] = "年级,班级,学号,姓名,性别,平时,期中,期末,总分,绩点,排名";

/* qsort 比较函数使用的排序字段。 */
static int s_sort_field = 0;

/* qsort 比较函数使用的排序方向：1=正序，-1=倒序。 */
static int s_sort_direction = 1;

/* 平时/期中/期末权重，单位：百分数，默认 30/20/50。 */
static float s_weight_regular = 30.0f;
static float s_weight_midterm = 20.0f;
static float s_weight_final = 50.0f;

static void core_update_all_totals(void);

/* ==================================================================
 * 二、通用字符串 / 文本工具
 * ================================================================== */

/*
 * 去掉字符串首尾空白字符（直接在原字符串上修改）。
 *
 * 主要逻辑：
 *   1. 跳过开头的空白字符
 *   2. 全部是空白时直接变成空字符串
 *   3. 从末尾向前跳过空白字符
 *   4. 如果开头有空白，把有效内容移动到数组首位
 */
static void core_trim_whitespace(char* text)
{
    char* start = text;
    char* end;

    if (text == NULL) return;

    // 跳过开头的空白字符
    while (*start != '\0' && isspace((unsigned char)*start)) start++;

    // 全部是空白时直接变成空字符串
    if (*start == '\0') {
        text[0] = '\0';
        return;
    }

    // 从末尾向前跳过空白字符
    end = start + strlen(start) - 1;
    while (end >= start && isspace((unsigned char)*end)) end--;
    end[1] = '\0';

    // 如果开头有空白，把有效内容移动到数组首位
    if (start != text) memmove(text, start, strlen(start) + 1);
}

/*
 * 去掉 UTF-8 BOM（EF BB BF）。
 *
 * 主要逻辑：
 *   1. 调用 memmove()
 *   2. 调用 strlen()
 */
static void core_remove_utf8_bom(char* text)
{
    unsigned char* p = (unsigned char*)text;

    if (text == NULL) return;
    if (p[0] == 0xEF && p[1] == 0xBB && p[2] == 0xBF) {
        memmove(text, text + 3, strlen(text + 3) + 1);
    }
}

/*
 * 把字符串安全复制到目标缓冲区，并去掉首尾空白。
 *
 * 主要逻辑：
 *   1. 跳过源字符串开头的空白
 *   2. 复制有效字符，最多复制 dst_size - 1 个，保留 '\0' 位置
 *   3. 去掉复制结果末尾的空白
 */
static void core_copy_trimmed(char* dst, size_t dst_size, const char* src)
{
    size_t i = 0;

    if (dst == NULL || dst_size == 0) return;
    dst[0] = '\0';
    if (src == NULL) return;

    // 跳过源字符串开头的空白
    while (*src != '\0' && isspace((unsigned char)*src)) src++;

    // 复制有效字符，最多复制 dst_size - 1 个，保留 '\0' 位置
    while (*src != '\0' && i + 1 < dst_size) {
        dst[i] = *src;
        i++;
        src++;
    }
    dst[i] = '\0';

    // 去掉复制结果末尾的空白
    while (i > 0 && isspace((unsigned char)dst[i - 1])) {
        dst[i - 1] = '\0';
        i--;
    }
}

/*
 * 判断字符串是否包含关键字，ASCII 字符不区分大小写。
 *
 * 主要逻辑：
 *   1. 逐字符比较，直到 keyword 全部匹配或遇到不一致
 *   2. 调用 strlen()
 *   3. 循环处理：; *haystack != '\0'; haystack++
 *   4. 调用 tolower()
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

        // 逐字符比较，直到 keyword 全部匹配或遇到不一致
        while (i < needle_len && haystack[i] != '\0' &&
            tolower((unsigned char)haystack[i]) ==
            tolower((unsigned char)needle[i])) {
            i++;
        }

        if (i == needle_len) return 1;
    }

    return 0;
}

/*
 * 自然字符串比较：优先比较字符串中出现的数字部分。
 *
 * 主要逻辑：
 *   1. 两边同时遇到数字时，先提取完整数字按数值比较
 *   2. 非数字位置按普通字节比较
 *
 * @return <0 表示 left 在前；0 表示相等；>0 表示 left 在后。
 */
static int core_compare_text_natural(const char* left, const char* right)
{
    while (*left != '\0' && *right != '\0') {
        // 两边同时遇到数字时，先提取完整数字按数值比较
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

        // 非数字位置按普通字节比较
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

/*
 * 解析一个完整的非负整数文本。 允许前后空白，但不允许负号、多余字符或空字符串。
 *
 * 主要逻辑：
 *   1. 循环处理：*text != '\0' && isspace((unsigned char
 *   2. 调用 strtoull()
 *   3. 循环处理：*end != '\0' && isspace((unsigned char
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

/*
 * 解析一个完整的浮点数文本（用于分数、绩点）。
 *
 * 主要逻辑：
 *   1. 循环处理：*text != '\0' && isspace((unsigned char
 *   2. 调用 strtof()
 *   3. 循环处理：*end != '\0' && isspace((unsigned char
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

/*
 * 解析性别文本。 支持：男/女、M/F、m/f、1/0、true/false。
 *
 * 主要逻辑：
 *   1. 循环处理：*text != '\0' && isspace((unsigned char
 *   2. 循环处理：*text != '\0' && i + 1 < sizeof(buf
 *   3. 循环处理：i > 0 && isspace((unsigned char
 *   4. 调用 strcmp()
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

/*
 * 把一个学生的某个字段格式化成文本。 搜索、筛选选项收集都要把数值字段先转换成字符串，因此统一放在这里。
 *
 * 主要逻辑：
 *   1. 按 field 分支处理
 *   2. 调用 snprintf()
 *   3. 调用 data_calc_total()
 *   4. 调用 data_calc_gpa()
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
    case STU_FIELD_REGULAR:
        snprintf(buf, size, "%.1f", student->regular_score);
        break;
    case STU_FIELD_MIDTERM:
        snprintf(buf, size, "%.1f", student->midterm_score);
        break;
    case STU_FIELD_FINAL:
        snprintf(buf, size, "%.1f", student->final_score);
        break;
    case STU_FIELD_SCORE: {
        float total = data_calc_total(student->regular_score,
            student->midterm_score, student->final_score);
        snprintf(buf, size, "%.1f", total);
        break;
    }
    case STU_FIELD_GPA: {
        float total = data_calc_total(student->regular_score,
            student->midterm_score, student->final_score);
        snprintf(buf, size, "%.2f", data_calc_gpa(total));
        break;
    }
    case STU_FIELD_RANK:
        snprintf(buf, size, "%" PRIu16, student->rank);
        break;
    default:
        break;
    }

    return buf;
}

/*
 * 构造“年级-班级”组合筛选文本。 用于班级筛选，使不同年级的同名班级可以区分开来。
 *
 * 主要逻辑：
 *   1. 调用 snprintf()
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

/*
 * 把一行 CSV 按逗号拆分成 8 个字段。
 *
 * 主要逻辑：
 *   1. 跳过连续逗号产生的空字段
 *   2. 找到当前字段结尾的逗号，并把它替换成 '\0'
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
        // 跳过连续逗号产生的空字段
        if (*p == ',') {
            p++;
            continue;
        }

        fields[count] = p;
        count++;

        // 找到当前字段结尾的逗号，并把它替换成 '\0'
        while (*p != '\0' && *p != ',') p++;
        if (*p == ',') {
            *p = '\0';
            p++;
        }
    }

    return count;
}

/*
 * 把一行 CSV 文本解析成 StudentCSV 结构体。
 *
 * 主要逻辑：
 *   1. 兼容旧 8 列格式和新 11 列格式
 *   2. 逐个字段去掉首尾空白
 *   3. 公共字段：年级、班级、学号、姓名、性别
 *   4. 新格式：...,平时,期中,期末,总分,绩点,排名
 *   5. 旧格式：...,分数,绩点,排名。旧总分同时填到三项，保证总分不变
 *
 * @return 1 成功；0 失败（字段数不足或类型解析失败）。
 */
static int core_parse_csv_student(char* line, StudentCSV* student)
{
    char* fields[11];
    int field_count;
    uint64_t rank_value = 0;
    float regular = 0.0f;
    float midterm = 0.0f;
    float final_score = 0.0f;

    if (line == NULL || student == NULL) return 0;

    student->is_new = false;
    student->new_filled_mask = 0;

    field_count = core_split_csv_line(line, fields, 11);

    // 兼容旧 8 列格式和新 11 列格式
    if (field_count < 8) return 0;

    // 逐个字段去掉首尾空白
    for (int i = 0; i < field_count; i++) {
        core_trim_whitespace(fields[i]);
    }

    // 公共字段：年级、班级、学号、姓名、性别
    core_copy_trimmed(student->grade, sizeof(student->grade), fields[0]);
    core_copy_trimmed(student->class_name, sizeof(student->class_name), fields[1]);
    core_copy_trimmed(student->name, sizeof(student->name), fields[3]);

    if (!core_parse_uint64_text(fields[2], &student->id)) return 0;
    if (!core_parse_gender_text(fields[4], &student->gender)) return 0;

    if (field_count >= 11) {
        // 新格式：...,平时,期中,期末,总分,绩点,排名
        if (!core_parse_float_text(fields[5], &regular)) return 0;
        if (!core_parse_float_text(fields[6], &midterm)) return 0;
        if (!core_parse_float_text(fields[7], &final_score)) return 0;

        if (fields[10][0] != '\0') {
            if (!core_parse_uint64_text(fields[10], &rank_value)) return 0;
            if (rank_value > UINT16_MAX) return 0;
        }
    }
    else {
        // 旧格式：...,分数,绩点,排名。旧总分同时填到三项，保证总分不变
        float old_total = 0.0f;
        if (!core_parse_float_text(fields[5], &old_total)) return 0;
        regular = old_total;
        midterm = old_total;
        final_score = old_total;

        if (fields[7][0] != '\0') {
            if (!core_parse_uint64_text(fields[7], &rank_value)) return 0;
            if (rank_value > UINT16_MAX) return 0;
        }
    }

    student->regular_score = regular;
    student->midterm_score = midterm;
    student->final_score = final_score;
    student->score = data_calc_total(regular, midterm, final_score);
    student->gpa = data_calc_gpa(student->score);
    student->rank = (uint16_t)rank_value;

    return 1;
}

/*
 * 用 UTF-8 路径打开文件（Windows 下转成宽字符路径再 _wfopen）。
 *
 * 主要逻辑：
 *   1. 先按调用时的当前工作目录打开
 *   2. 相对路径：再相对 exe 所在目录找一次
 */
static FILE* core_fopen_utf8(const char* path, const char* mode)
{
    wchar_t wpath[1024];
    wchar_t wmode[16];

    if (path == NULL || mode == NULL) return NULL;

    if (MultiByteToWideChar(CP_UTF8, 0, path, -1, wpath,
        (int)(sizeof(wpath) / sizeof(wpath[0]))) <= 0) {
        return NULL;
    }
    if (MultiByteToWideChar(CP_UTF8, 0, mode, -1, wmode,
        (int)(sizeof(wmode) / sizeof(wmode[0]))) <= 0) {
        return NULL;
    }

    // 先按调用时的当前工作目录打开
    FILE* fp = _wfopen(wpath, wmode);
    if (fp != NULL) return fp;

    // 相对路径：再相对 exe 所在目录找一次
    if (path[0] != '\\' && path[0] != '/' &&
        !(path[0] != '\0' && path[1] == ':')) {
        wchar_t exe_path[1024];
        DWORD n = GetModuleFileNameW(NULL, exe_path,
            (DWORD)(sizeof(exe_path) / sizeof(exe_path[0])));

        if (n > 0 && n < (DWORD)(sizeof(exe_path) / sizeof(exe_path[0]))) {
            wchar_t* slash = wcsrchr(exe_path, L'\\');
            if (slash != NULL) {
                wchar_t full[2048];
                slash[1] = L'\0';
                wcscpy(full, exe_path);
                if (wcslen(full) + wcslen(wpath) < 2048) {
                    wcscat(full, wpath);
                    fp = _wfopen(full, wmode);
                }
            }
        }
    }
    return fp;
}

/*
 * 从 CSV 文件读取学生数据。 第一行作为表头写入 header；后续行逐行解析，解析失败的行直接跳过。
 *
 * 主要逻辑：
 *   1. 输出错误信息
 *   2. 读取第一行作为表头
 *   3. 逐行读取学生数据
 *   4. 跳过空行
 *   5. 解析失败的行直接跳过，避免因为一条脏数据导致导入失败
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
    char line[1024];
    int count = 0;

    if (filename == NULL || out == NULL || header == NULL) return -1;

    fp = core_fopen_utf8(filename, "r");
    if (fp == NULL) {
        perror(filename);  // 输出错误信息
        return -1;
    }
    // 读取第一行作为表头
    if (header_size > 0) {
        if (fgets(header, (int)header_size, fp) != NULL) {
            core_remove_utf8_bom(header);
            core_trim_whitespace(header);
        }
        else {
            header[0] = '\0';
        }
    }

    // 逐行读取学生数据
    while (count < capacity && fgets(line, sizeof(line), fp) != NULL) {
        core_trim_whitespace(line);

        // 跳过空行
        if (line[0] == '\0') continue;

        // 解析失败的行直接跳过，避免因为一条脏数据导致导入失败
        if (!core_parse_csv_student(line, &out[count])) continue;
        count++;
    }

    fclose(fp);
    return count;
}

/*
 * 把学生数组写入 CSV 文件（UTF-8 with BOM）。
 *
 * 主要逻辑：
 *   1. 用二进制模式写，方便在文件开头写 UTF-8 BOM
 *   2. UTF-8 BOM，方便 Windows 记事本 / Excel 正确识别中文
 *   3. 逐条写数据行
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
    if (header == NULL || header[0] == '\0') header = "年级,班级,学号,姓名,性别,平时,期中,期末,总分,绩点,排名";

    // 用二进制模式写，方便在文件开头写 UTF-8 BOM
    fp = core_fopen_utf8(filename, "wb");
    if (fp == NULL) return -1;

    // UTF-8 BOM，方便 Windows 记事本 / Excel 正确识别中文
    fputs("\xEF\xBB\xBF", fp);
    fputs(header, fp);
    fputs("\r\n", fp);

    // 逐条写数据行
    for (int i = 0; i < count; i++) {
        fprintf(fp, "%s,%s,%" PRIu64 ",%s,%s,%.1f,%.1f,%.1f,%.1f,%.2f,%" PRIu16 "\r\n",
            students[i].grade,
            students[i].class_name,
            students[i].id,
            students[i].name,
            students[i].gender ? "男" : "女",
            students[i].regular_score,
            students[i].midterm_score,
            students[i].final_score,
            data_calc_total(students[i].regular_score,
                students[i].midterm_score, students[i].final_score),
            data_calc_gpa(students[i].score),
            students[i].rank);
    }

    fclose(fp);
    return count;
}

/* ==================================================================
 * 五、排序比较函数
 * ================================================================== */

/*
 * 按分数从高到低排序（内部排名计算使用）。
 *
 * 主要逻辑：
 *   1. 比较两个学生的 score：前者小返回 1，前者大返回 -1，相等返回 0（降序）。
 */
static int core_cmp_score_desc(const void* a, const void* b)
{
    const StudentCSV* left = (const StudentCSV*)a;
    const StudentCSV* right = (const StudentCSV*)b;

    if (left->score < right->score) return 1;
    if (left->score > right->score) return -1;
    return 0;
}

/*
 * 按分数从高到低排序并重新计算所有学生的 rank。
 *
 * 主要逻辑：
 *   1. 调用 qsort()
 *   2. 循环处理：int i = 0; i < count; i++
 */
static void core_recalc_rank(StudentCSV* students, int count)
{
    if (students == NULL || count <= 0) return;

    qsort(students, (size_t)count, sizeof(StudentCSV), core_cmp_score_desc);
    for (int i = 0; i < count; i++) {
        students[i].rank = (uint16_t)(i + 1);
    }
}

/*
 * 按分数从高到低重新计算所有学生的 rank，但不移动学生位置。
 *
 * 主要逻辑：
 *   1. 统计排在当前学生前面的人数
 *   2. 分数更高，名次更靠前
 *   3. 同分时数组靠前者名次更靠前
 *   4. 写入名次
 */
static void core_recalc_rank_inplace(StudentCSV* students, int count)
{
    if (students == NULL || count <= 0) return;

    for (int i = 0; i < count; i++) {
        int better = 0;  // 统计排在当前学生前面的人数

        for (int j = 0; j < count; j++) {
            if (students[j].score > students[i].score) {
                better++;  // 分数更高，名次更靠前
            }
            else if (students[j].score == students[i].score && j < i) {
                better++;  // 同分时数组靠前者名次更靠前
            }
        }

        students[i].rank = (uint16_t)(better + 1);  // 写入名次
    }
}

/*
 * qsort 使用的通用学生比较函数。 读取文件级变量 s_sort_field / s_sort_direction 决定排序规则： 年级/班级/姓名 → 自然字符串比较； 学号/分数/绩点/排名 → 数值比较； 性别 → true(男)=1、false(女)=0。
 *
 * 主要逻辑：
 *   1. 先按班级排序
 *   2. 班级相同时，再按年级排序，保证不同年级的同名班级顺序稳定
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
        // 先按班级排序
        result = core_compare_text_natural(left->class_name, right->class_name);

        // 班级相同时，再按年级排序，保证不同年级的同名班级顺序稳定
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
    case STU_FIELD_REGULAR:
        result = left->regular_score < right->regular_score ? -1 : (left->regular_score > right->regular_score ? 1 : 0);
        break;
    case STU_FIELD_MIDTERM:
        result = left->midterm_score < right->midterm_score ? -1 : (left->midterm_score > right->midterm_score ? 1 : 0);
        break;
    case STU_FIELD_FINAL:
        result = left->final_score < right->final_score ? -1 : (left->final_score > right->final_score ? 1 : 0);
        break;
    case STU_FIELD_SCORE:
        result = left->score < right->score ? -1 : (left->score > right->score ? 1 : 0);
        break;
    case STU_FIELD_GPA: {
        float left_gpa = data_calc_gpa(left->score);
        float right_gpa = data_calc_gpa(right->score);
        result = left_gpa < right_gpa ? -1 : (left_gpa > right_gpa ? 1 : 0);
        break;
    }
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

/*
 * 当前是否存在有效的筛选条件。
 *
 * 主要逻辑：
 *   1. 判断筛选字段有效且筛选选项非空。
 */
static int core_filter_active(void)
{
    return s_filter_field >= 0 && s_filter_option[0] != '\0';
}

/*
 * 把分数转换成每 10.0 分一段的区间文本。
 *
 * 主要逻辑：
 *   1. 调用 snprintf()
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

/*
 * 判断分数是否落在筛选选项描述的区间内。 区间约定为左闭右开 [lower, upper)；对于 100 分， 允许其命中 "100-110" 区间。
 *
 * 主要逻辑：
 *   1. 调用 sscanf()
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

/*
 * 判断一名学生是否满足当前筛选条件。
 *
 * 主要逻辑：
 *   1. 调用 core_filter_active()
 *   2. 按 s_filter_field 分支处理
 *   3. 调用 core_copy_trimmed()
 *   4. 调用 core_grade_class_text()
 *   5. 调用 snprintf()
 *
 * @return 1 命中；0 不命中。
 */
static int core_match_filter(const StudentCSV* student)
{
    char value[64];

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

/*
 * 重建显示行映射表。 导入、删除、修改、排序、筛选之后都必须调用本函数， 保证 data_get_display_count() / data_get_display_index() 返回最新结果。
 *
 * 主要逻辑：
 *   1. 循环处理：int i = 0; i < s_count && s_display_count < 200; i++
 *   2. 调用 core_match_filter()
 */
static void core_rebuild_display(void)
{
    s_display_count = 0;

    for (int i = 0; i < s_count && s_display_count < 200; i++) {
        if (core_match_filter(&s_students[i])) {
            s_display_indexes[s_display_count] = i;
            s_display_count++;
        }
    }
}

/* ==================================================================
 * 七、数据初始化
 * ================================================================== */

/*
 * 首次被 UI 访问时把学生数据初始化为空。
 *
 * 主要逻辑：
 *   1. 清空筛选条件，显示行映射重建后自然为空。
 *   2. 调用 snprintf()
 *   3. 调用 core_rebuild_display()
 */
static void data_init_empty(void)
{
    s_count = 0;
    snprintf(s_csv_header, sizeof(s_csv_header), "%s", "年级,班级,学号,姓名,性别,平时,期中,期末,总分,绩点,排名");

    // 清空筛选条件，显示行映射重建后自然为空。
    s_filter_field = -1;
    s_filter_option[0] = '\0';
    core_rebuild_display();
    s_initialized = 1;
}

/*
 * 保证首次访问时数据已初始化。
 *
 * 主要逻辑：
 *   1. 调用 data_init_empty()
 */
static void data_ensure_initialized(void)
{
    if (!s_initialized) data_init_empty();
}

/* ==================================================================
 * 八、UI 数据层接口实现
 * ================================================================== */

/* ---------- 8.1 数据访问 ---------- */

/*
 * 获取当前内存中的学生总数。
 *
 * 主要逻辑：
 *   1. 调用 data_ensure_initialized()
 */
int data_get_count(void)
{
    data_ensure_initialized();
    return s_count;
}

/*
 * 获取学生数组首地址。
 *
 * 主要逻辑：
 *   1. 调用 data_ensure_initialized()
 */
StudentCSV* data_get_all(void)
{
    data_ensure_initialized();
    return s_students;
}

/*
 * 获取网格当前应显示的数据行数。
 *
 * 主要逻辑：
 *   1. 没有筛选时，表格固定保留 200 行：
 *   2. 有数据的行显示数据，不足 200 行的部分显示为空行
 *   3. 有筛选时，只显示筛选结果的行数
 */
int data_get_display_count(void)
{
    data_ensure_initialized();

    // 没有筛选时，表格固定保留 200 行：
    // 有数据的行显示数据，不足 200 行的部分显示为空行
    if (!core_filter_active()) return 200;

    // 有筛选时，只显示筛选结果的行数
    return s_display_count;
}

/*
 * 把显示行号映射为学生数组下标。
 *
 * 主要逻辑：
 *   1. 调用 data_ensure_initialized()
 */
int data_get_display_index(int display_row)
{
    data_ensure_initialized();

    if (display_row < 0 || display_row >= s_display_count) return -1;
    return s_display_indexes[display_row];
}

/* ---------- 8.2 成绩计算与权重 ---------- */

/*
 * 根据总分计算绩点：100 分对应 5.0，每低 1 分减 0.1，低于 60 分统一为 0。
 *
 * 主要逻辑：
 *   1. 低于 60 分：绩点直接为 0
 *   2. 100 分对应 5.0，每低 1 分降 0.1
 */
float data_calc_gpa(float score)
{
    // 低于 60 分：绩点直接为 0
    if (score < 60.0f) return 0.0f;

    // 100 分对应 5.0，每低 1 分降 0.1
    float gpa = 5.0f - (100.0f - score) * 0.1f;

    if (gpa < 0.0f) gpa = 0.0f;
    if (gpa > 5.0f) gpa = 5.0f;

    return gpa;
}

/*
 * 按当前权重计算总分：(平时×平时权重 + 期中×期中权重 + 期末×期末权重) ÷ 100。
 *
 * 主要逻辑：
 *   1. 按三项权重对平时、期中、期末成绩做加权平均并返回。
 */
float data_calc_total(float regular, float midterm, float final_score)
{
    return (regular * s_weight_regular +
        midterm * s_weight_midterm +
        final_score * s_weight_final) / 100.0f;
}

/*
 * 恢复默认权重：平时 30%、期中 20%、期末 50%。
 *
 * 主要逻辑：
 *   1. 把平时、期中、期末权重分别设置为 30、20、50。
 */
void data_reset_weights(void)
{
    s_weight_regular = 30.0f;
    s_weight_midterm = 20.0f;
    s_weight_final = 50.0f;
}

/*
 * 解析文本并把某一项权重设置为 0~100 之间的数值。
 *
 * 主要逻辑：
 *   1. 调用 core_parse_float_text()
 */
int data_set_weight_from_text(int which, const char* text)
{
    float v = 0.0f;

    if (text == NULL || text[0] == '\0') return 0;
    if (!core_parse_float_text(text, &v)) return 0;
    if (v < 0.0f || v > 100.0f) return 0;

    if (which == 0) s_weight_regular = v;
    else if (which == 1) s_weight_midterm = v;
    else if (which == 2) s_weight_final = v;
    else return 0;

    return 1;
}

/*
 * 一次解析三项权重文本（支持空格、逗号、斜杠分隔）并写入权重变量。
 *
 * 主要逻辑：
 *   1. 逐个跳过空格、逗号、斜杠等分隔符，再解析数字
 *   2. 循环处理：*p != '\0' && count < 3
 *   3. 调用 strtod()
 *   4. 循环处理：int i = 0; i < 3; i++
 */
int data_set_weights_from_text(const char* text)
{
    float values[3] = { 0.0f, 0.0f, 0.0f };
    int count = 0;
    const char* p = text;

    if (text == NULL) return 0;

    // 逐个跳过空格、逗号、斜杠等分隔符，再解析数字
    while (*p != '\0' && count < 3) {
        char* end = NULL;
        float v;

        while (*p != '\0' &&
            !((*p >= '0' && *p <= '9') || *p == '.' || *p == '-' || *p == '+')) {
            p++;
        }
        if (*p == '\0') break;

        v = (float)strtod(p, &end);
        if (end == p) break;

        values[count] = v;
        count++;
        p = end;
    }

    if (count != 3) return 0;

    for (int i = 0; i < 3; i++) {
        if (values[i] < 0.0f || values[i] > 100.0f) return 0;
    }

    s_weight_regular = values[0];
    s_weight_midterm = values[1];
    s_weight_final = values[2];
    return 1;
}

/*
 * 读取指定项的权重值（百分数）。
 *
 * 主要逻辑：
 *   1. 按 which 返回对应权重；which 非法时返回 0。
 */
float data_get_weight(int which)
{
    if (which == 0) return s_weight_regular;
    if (which == 1) return s_weight_midterm;
    if (which == 2) return s_weight_final;
    return 0.0f;
}

/*
 * 计算平时、期中、期末三项权重之和，用于校验是否等于 100。
 *
 * 主要逻辑：
 *   1. 返回平时、期中、期末三项权重之和。
 */
float data_get_weight_sum(void)
{
    return s_weight_regular + s_weight_midterm + s_weight_final;
}

/*
 * 内部使用：按当前权重重算所有学生总分、绩点、排名，并重建显示映射。
 *
 * 主要逻辑：
 *   1. 循环处理：int i = 0; i < s_count; i++
 *   2. 调用 data_calc_total()
 *   3. 调用 data_calc_gpa()
 *   4. 调用 core_recalc_rank()
 *   5. 调用 core_rebuild_display()
 */
static void core_update_all_totals(void)
{
    for (int i = 0; i < s_count; i++) {
        s_students[i].score = data_calc_total(s_students[i].regular_score,
            s_students[i].midterm_score,
            s_students[i].final_score);
        s_students[i].gpa = data_calc_gpa(s_students[i].score);
    }

    core_recalc_rank(s_students, s_count);
    core_rebuild_display();
}

/*
 * 按当前权重重新计算全部学生的总分、绩点、排名，并重建显示行映射。
 *
 * 主要逻辑：
 *   1. 调用 data_ensure_initialized()
 *   2. 调用 core_update_all_totals()
 */
void data_recalc_all_totals(void)
{
    data_ensure_initialized();
    core_update_all_totals();
}

/* ---------- 8.3 导入 / 导出与统计 ---------- */

/*
 * 从 CSV 导入数据，整体覆盖内存中的数据。
 *
 * 主要逻辑：
 *   1. 先读入临时数组，读取失败时不破坏当前数据
 *   2. 按当前权重计算总分、绩点和排名
 *   3. 数据整体换掉后，旧筛选条件已无意义
 */
int data_import_csv(const char* path)
{
    StudentCSV loaded[200];
    int count;

    if (path == NULL || path[0] == '\0') return -1;

    // 先读入临时数组，读取失败时不破坏当前数据
    count = core_read_csv(path, loaded, 200, s_csv_header, sizeof(s_csv_header));
    if (count < 0) return -1;

    memcpy(s_students, loaded, (size_t)count * sizeof(StudentCSV));
    s_count = count;

    // 按当前权重计算总分、绩点和排名
    core_update_all_totals();
    snprintf(s_csv_header, sizeof(s_csv_header), "%s", "年级,班级,学号,姓名,性别,平时,期中,期末,总分,绩点,排名");

    // 数据整体换掉后，旧筛选条件已无意义
    s_filter_field = -1;
    s_filter_option[0] = '\0';
    core_rebuild_display();
    s_initialized = 1;
    return s_count;
}

/*
 * 把全部学生数据导出为 CSV。
 *
 * 主要逻辑：
 *   1. 调用 data_ensure_initialized()
 */
int data_export_csv(const char* path)
{
    data_ensure_initialized();

    if (path == NULL || path[0] == '\0') return -1;
    return core_write_csv(path, s_students, s_count, s_csv_header);
}

/* ---------- 8.3.1 统计结果导出 ---------- */

/* 年级 + 班级分组键，用于按班级统计。 */
typedef struct {
    char grade[64];       /* 年级，例如 "2022级" */
    char class_name[64];  /* 班级，例如 "计算机1班" */
} CoreClassKey;

/*
 * 分组键排序比较函数：先按年级，再按班级。
 *
 * 主要逻辑：
 *   1. 调用 strcmp()
 */
static int core_cmp_class_key(const void* a, const void* b)
{
    const CoreClassKey* left = (const CoreClassKey*)a;
    const CoreClassKey* right = (const CoreClassKey*)b;
    int cmp = strcmp(left->grade, right->grade);

    if (cmp != 0) return cmp;
    return strcmp(left->class_name, right->class_name);
}

/* 单个统计项的累计结果（平时/期中/期末/总分各一份）。 */
typedef struct {
    double sum;     /* 分数总和，用 double 减少累计误差 */
    float  max;     /* 最高成绩 */
    float  min;     /* 最低成绩 */
    int    count;   /* 参与统计的人数 */
    int    pass;    /* 及格人数（>=60） */
} CoreStatItem;

/*
 * 把分数格式化为文本：最多保留 1 位小数，整数不显示 ".0"。
 *
 * 主要逻辑：
 *   1. 调用 snprintf()
 */
static void core_format_score(char* buf, size_t size, float value)
{
    long tenths, whole, frac;

    if (buf == NULL || size == 0) return;

    tenths = (long)(value * 10.0f + (value >= 0.0f ? 0.5f : -0.5f));
    whole = tenths / 10;
    frac = tenths % 10;
    if (frac < 0) frac = -frac;

    if (frac == 0) {
        snprintf(buf, size, "%ld", whole);
    }
    else {
        snprintf(buf, size, "%ld.%ld", whole, frac);
    }
}

/*
 * 按“年级-班级”导出统计结果 TXT。 每个班级输出 4 行：平时、期中、期末、总分的平均分， 每行末尾附带该统计项的最高成绩、最低成绩、及格人数和及格率； 不同班级之间空一行。
 *
 * 主要逻辑：
 *   1. 第一步：收集所有出现过的“年级+班级”组合（保持出现顺序，稍后再排序）。
 *   2. 按年级、班级排序，保证每次导出的班级顺序一致。
 *   3. 第二步：创建文件（UTF-8 BOM + CRLF，方便记事本/Excel 正确识别中文）。
 *   4. 第三步：逐个班级统计并写出一段结果。
 *   5. 扫描所有学生，把属于当前班级的分数累加到 4 个统计项中。
 *   6. 班级标题，例如：2022级-计算机1班统计结果：
 *
 * @param path 目标 TXT 文件路径
 * @return 成功返回写入的班级数（>=0）；路径为空或文件打不开返回 -1。
 */
int data_export_statistics(const char* path)
{
    CoreClassKey groups[200];
    int group_count = 0;
    FILE* fp;

    data_ensure_initialized();

    if (path == NULL || path[0] == '\0') return -1;

    // 第一步：收集所有出现过的“年级+班级”组合（保持出现顺序，稍后再排序）。
    for (int i = 0; i < s_count; i++) {
        int found = 0;

        for (int g = 0; g < group_count; g++) {
            if (strcmp(groups[g].grade, s_students[i].grade) == 0 &&
                strcmp(groups[g].class_name, s_students[i].class_name) == 0) {
                found = 1;
                break;
            }
        }

        if (!found && group_count < 200) {
            snprintf(groups[group_count].grade, 64, "%s", s_students[i].grade);
            snprintf(groups[group_count].class_name, 64, "%s", s_students[i].class_name);
            group_count++;
        }
    }

    // 按年级、班级排序，保证每次导出的班级顺序一致。
    if (group_count > 1) {
        qsort(groups, (size_t)group_count, sizeof(groups[0]), core_cmp_class_key);
    }

    // 第二步：创建文件（UTF-8 BOM + CRLF，方便记事本/Excel 正确识别中文）。
    fp = core_fopen_utf8(path, "wb");
    if (fp == NULL) return -1;
    fputs("\xEF\xBB\xBF", fp);

    // 第三步：逐个班级统计并写出一段结果。
    for (int g = 0; g < group_count; g++) {
        CoreStatItem items[4];
        static const char* item_names[4] = { "平时", "期中", "期末", "总分" };

        for (int k = 0; k < 4; k++) {
            items[k].sum = 0.0;
            items[k].max = 0.0f;
            items[k].min = 0.0f;
            items[k].count = 0;
            items[k].pass = 0;
        }

        // 扫描所有学生，把属于当前班级的分数累加到 4 个统计项中。
        for (int i = 0; i < s_count; i++) {
            float values[4];

            if (strcmp(groups[g].grade, s_students[i].grade) != 0 ||
                strcmp(groups[g].class_name, s_students[i].class_name) != 0) {
                continue;
            }

            values[0] = s_students[i].regular_score;
            values[1] = s_students[i].midterm_score;
            values[2] = s_students[i].final_score;
            values[3] = data_calc_total(values[0], values[1], values[2]);

            for (int k = 0; k < 4; k++) {
                if (items[k].count == 0 || values[k] > items[k].max) items[k].max = values[k];
                if (items[k].count == 0 || values[k] < items[k].min) items[k].min = values[k];
                items[k].sum += values[k];
                items[k].count++;
                if (values[k] >= 60.0f) items[k].pass++;
            }
        }

        // 班级标题，例如：2022级-计算机1班统计结果：
        fprintf(fp, "%s-%s统计结果：\r\n", groups[g].grade, groups[g].class_name);

        // 每个统计项输出一行：平均分 + 最高/最低 + 及格人数 + 及格率。
        for (int k = 0; k < 4; k++) {
            char avg[32] = "0";
            char max_text[32] = "0";
            char min_text[32] = "0";
            int rate = 0;

            if (items[k].count > 0) {
                core_format_score(avg, sizeof(avg),
                    (float)(items[k].sum / (double)items[k].count));
                core_format_score(max_text, sizeof(max_text), items[k].max);
                core_format_score(min_text, sizeof(min_text), items[k].min);
                rate = (int)((items[k].pass * 100.0 / (double)items[k].count) + 0.5);
            }

            fprintf(fp,
                "%s成绩平均分：%s    最高成绩：%s    最低成绩：%s   及格人数：%d    及格率：%d%%\r\n",
                item_names[k], avg, max_text, min_text, items[k].pass, rate);
        }

        // 不同班级之间空一行（最后一个班级后面不空行）。
        if (g + 1 < group_count) {
            fputs("\r\n", fp);
        }
    }

    fclose(fp);
    return group_count;
}

/* ---------- 8.4 自动文件路径解析 ---------- */

/*
 * 判断 UTF-8 路径是否是已存在的文件夹。
 *
 * 主要逻辑：
 *   1. 调用 MultiByteToWideChar()
 *   2. 调用 GetFileAttributesW()
 */
static int core_path_is_dir_utf8(const char* path)
{
    wchar_t wpath[1024];
    DWORD attr;

    if (path == NULL || path[0] == '\0') return 0;
    if (MultiByteToWideChar(CP_UTF8, 0, path, -1, wpath,
        (int)(sizeof(wpath) / sizeof(wpath[0]))) <= 0) {
        return 0;
    }

    attr = GetFileAttributesW(wpath);
    if (attr == INVALID_FILE_ATTRIBUTES) return 0;
    return (attr & FILE_ATTRIBUTE_DIRECTORY) ? 1 : 0;
}

/*
 * 把宽字符路径转换为 UTF-8 文本；成功返回 1。
 *
 * 主要逻辑：
 *   1. 调用 WideCharToMultiByte 把宽字符路径转换为 UTF-8 并写入输出缓冲区。
 */
static int core_wide_to_utf8(const wchar_t* wpath, char* out, size_t out_size)
{
    if (wpath == NULL || out == NULL || out_size == 0) return 0;
    return WideCharToMultiByte(CP_UTF8, 0, wpath, -1, out,
        (int)out_size, NULL, NULL) > 0 ? 1 : 0;
}

/*
 * 获取桌面文件夹的 UTF-8 路径；成功返回 1。
 *
 * 主要逻辑：
 *   1. 优先用系统 API，能正确处理桌面被重定向或重命名的情况。
 *   2. 退回到 %USERPROFILE%\Desktop。
 */
static int core_get_desktop_dir(char* out, size_t out_size)
{
    wchar_t wpath[MAX_PATH];

    if (out == NULL || out_size == 0) return 0;

    // 优先用系统 API，能正确处理桌面被重定向或重命名的情况。
    if (SUCCEEDED(SHGetFolderPathW(NULL,
        CSIDL_DESKTOPDIRECTORY | CSIDL_FLAG_CREATE, NULL,
        SHGFP_TYPE_CURRENT, wpath))) {
        if (core_wide_to_utf8(wpath, out, out_size)) return 1;
    }

    // 退回到 %USERPROFILE%\Desktop。
    if (GetEnvironmentVariableW(L"USERPROFILE", wpath, MAX_PATH) > 0) {
        if (wcslen(wpath) + 9 < MAX_PATH) {
            wcscat(wpath, L"\\Desktop");
            if (core_wide_to_utf8(wpath, out, out_size)) return 1;
        }
    }
    return 0;
}

/*
 * 在指定文件夹中查找最后修改时间最新的 students_*.csv。
 *
 * 主要逻辑：
 *   1. 去掉末尾分隔符，避免拼出 "dir\\students_*.csv"。
 *   2. 循环开始
 *   3. 取最后修改时间最大的文件；时间相同时取文件名更大的，保证结果稳定。
 *   4. 循环条件判断
 *
 * @return 1 找到并写入 out_path；0 未找到。
 */
static int core_find_newest_students_csv(const char* dir_utf8,
    char* out_path, size_t out_size)
{
    wchar_t wdir[1024];
    wchar_t wpattern[1200];
    wchar_t wbest[MAX_PATH];
    WIN32_FIND_DATAW fd;
    HANDLE h;
    ULARGE_INTEGER best_time;
    int found = 0;
    char dir_copy[1024];

    if (dir_utf8 == NULL || out_path == NULL || out_size == 0) return 0;
    out_path[0] = '\0';

    snprintf(dir_copy, sizeof(dir_copy), "%s", dir_utf8);
    // 去掉末尾分隔符，避免拼出 "dir\\students_*.csv"。
    {
        size_t len = strlen(dir_copy);
        while (len > 1 && (dir_copy[len - 1] == '\\' || dir_copy[len - 1] == '/')) {
            if (len == 3 && dir_copy[1] == ':') break;
            dir_copy[--len] = '\0';
        }
    }

    if (MultiByteToWideChar(CP_UTF8, 0, dir_copy, -1, wdir,
        (int)(sizeof(wdir) / sizeof(wdir[0]))) <= 0) {
        return 0;
    }

    _snwprintf(wpattern, (size_t)(sizeof(wpattern) / sizeof(wpattern[0])),
        L"%ls\\students_*.csv", wdir);
    wpattern[(sizeof(wpattern) / sizeof(wpattern[0])) - 1] = L'\0';

    best_time.QuadPart = 0;
    wbest[0] = L'\0';

    h = FindFirstFileW(wpattern, &fd);
    if (h == INVALID_HANDLE_VALUE) return 0;

    do {  // 循环开始
        ULARGE_INTEGER t;

        if (fd.dwFileAttributes & FILE_ATTRIBUTE_DIRECTORY) continue;

        t.LowPart = fd.ftLastWriteTime.dwLowDateTime;
        t.HighPart = fd.ftLastWriteTime.dwHighDateTime;

        // 取最后修改时间最大的文件；时间相同时取文件名更大的，保证结果稳定。
        if (!found || t.QuadPart > best_time.QuadPart ||
            (t.QuadPart == best_time.QuadPart && wcscmp(fd.cFileName, wbest) > 0)) {
            best_time = t;
            wcscpy(wbest, fd.cFileName);
            found = 1;
        }
    } while (FindNextFileW(h, &fd));  // 循环条件判断

    FindClose(h);
    if (!found) return 0;

    _snwprintf(wpattern, (size_t)(sizeof(wpattern) / sizeof(wpattern[0])),
        L"%ls\\%ls", wdir, wbest);
    wpattern[(sizeof(wpattern) / sizeof(wpattern[0])) - 1] = L'\0';

    if (!core_wide_to_utf8(wpattern, out_path, out_size)) return 0;
    return 1;
}

/*
 * 判断字符串是否以 suffix 结尾，ASCII 字符不区分大小写。
 *
 * 主要逻辑：
 *   1. 调用 strlen()
 *   2. 循环处理：i = 0; i < suffix_len; i++
 *   3. 调用 tolower()
 */
static int core_ends_with_ci(const char* str, const char* suffix)
{
    size_t str_len, suffix_len, i;

    if (str == NULL || suffix == NULL) return 0;
    str_len = strlen(str);
    suffix_len = strlen(suffix);
    if (str_len < suffix_len) return 0;

    for (i = 0; i < suffix_len; i++) {
        unsigned char a = (unsigned char)str[str_len - suffix_len + i];
        unsigned char b = (unsigned char)suffix[i];
        if (tolower(a) != tolower(b)) return 0;
    }
    return 1;
}

/*
 * 去掉路径末尾的 '\' 和 '/'（保留 "C:\" 这种根目录）。
 *
 * 主要逻辑：
 *   1. 调用 strlen()
 *   2. 循环处理：len > 1 && (path[len - 1] == '\\' || path[len - 1] == '/'
 */
static void core_strip_trailing_sep(char* path)
{
    size_t len;

    if (path == NULL) return;
    len = strlen(path);
    while (len > 1 && (path[len - 1] == '\\' || path[len - 1] == '/')) {
        if (len == 3 && path[1] == ':') break;
        path[--len] = '\0';
    }
}

/*
 * 把 path 改成它所在的文件夹路径（去掉最后一级文件名）。
 *
 * 主要逻辑：
 *   1. "C:\foo" -> "C:\"；"/foo" -> "/"
 *   2. 调用 core_strip_trailing_sep()
 *   3. 调用 strlen()
 *   4. 循环处理：len > 0 && path[len - 1] != '\\' && path[len - 1] != '/'
 */
static void core_get_parent_dir(char* path)
{
    size_t len;

    if (path == NULL) return;
    core_strip_trailing_sep(path);
    len = strlen(path);

    while (len > 0 && path[len - 1] != '\\' && path[len - 1] != '/') {
        len--;
    }
    if (len == 0) return;

    if (len == 1 || (len == 3 && path[1] == ':')) {
        // "C:\foo" -> "C:\"；"/foo" -> "/"
        path[len] = '\0';
    }
    else {
        path[len - 1] = '\0';
    }
}

/*
 * 把用户输入解析为文件夹路径：空=桌面；目录=本身；带文件名=所在目录；其它按目录处理。
 *
 * 主要逻辑：
 *   1. 调用 core_copy_trimmed()
 *   2. 调用 core_get_desktop_dir()
 *   3. 调用 snprintf()
 *   4. 调用 core_get_parent_dir()
 *   5. 调用 core_strip_trailing_sep()
 */
static int core_resolve_user_dir(const char* user_path, char* out, size_t out_size)
{
    char input[1024];

    if (out == NULL || out_size == 0) return 0;
    out[0] = '\0';
    input[0] = '\0';
    if (user_path != NULL) {
        core_copy_trimmed(input, sizeof(input), user_path);
    }

    if (input[0] == '\0') {
        if (!core_get_desktop_dir(out, out_size)) return 0;
    }
    else if (core_path_is_dir_utf8(input)) {
        snprintf(out, out_size, "%s", input);
    }
    else if (core_ends_with_ci(input, ".csv") || core_ends_with_ci(input, ".txt")) {
        snprintf(out, out_size, "%s", input);
        core_get_parent_dir(out);
    }
    else {
        snprintf(out, out_size, "%s", input);
    }

    core_strip_trailing_sep(out);
    return out[0] != '\0' ? 1 : 0;
}

/*
 * 解析用户输入的目标文件夹，生成 students_年月日时分秒.csv/.txt 完整路径。
 *
 * 主要逻辑：
 *   1. 先解析用户输入对应的文件夹（空地址为桌面）。
 *   2. 文件名固定为 students_年月日时分秒.csv / .txt
 */
int data_resolve_export_path(const char* user_path, int is_statistics,
    char* out_path, size_t out_size)
{
    char dir[1024];
    char name[128];
    SYSTEMTIME st;
    size_t dlen;

    if (out_path == NULL || out_size == 0) return 0;
    out_path[0] = '\0';

    // 先解析用户输入对应的文件夹（空地址为桌面）。
    if (!core_resolve_user_dir(user_path, dir, sizeof(dir))) return 0;

    // 文件名固定为 students_年月日时分秒.csv / .txt
    GetLocalTime(&st);
    snprintf(name, sizeof(name), "students_%04d%02d%02d%02d%02d%02d.%s",
        (int)st.wYear, (int)st.wMonth, (int)st.wDay,
        (int)st.wHour, (int)st.wMinute, (int)st.wSecond,
        is_statistics ? "txt" : "csv");

    dlen = strlen(dir);
    if (dir[dlen - 1] == '\\' || dir[dlen - 1] == '/') {
        snprintf(out_path, out_size, "%s%s", dir, name);
    }
    else {
        snprintf(out_path, out_size, "%s\\%s", dir, name);
    }
    return 1;
}

/*
 * 解析导入路径：空地址用桌面；文件夹查找最新 students_*.csv；.csv 文件直接使用。
 *
 * 主要逻辑：
 *   1. 兼容旧用法：直接输入了 .csv 文件路径。
 *   2. 解析文件夹后查找其中最新修改的 students_*.csv。
 */
int data_resolve_import_path(const char* user_path, char* out_path, size_t out_size)
{
    char input[1024];
    char dir[1024];

    if (out_path == NULL || out_size == 0) return 0;
    out_path[0] = '\0';

    input[0] = '\0';
    if (user_path != NULL) {
        core_copy_trimmed(input, sizeof(input), user_path);
    }

    // 兼容旧用法：直接输入了 .csv 文件路径。
    if (input[0] != '\0' && core_ends_with_ci(input, ".csv") &&
        !core_path_is_dir_utf8(input)) {
        snprintf(out_path, out_size, "%s", input);
        return 1;
    }

    // 解析文件夹后查找其中最新修改的 students_*.csv。
    if (!core_resolve_user_dir(user_path, dir, sizeof(dir))) return 0;
    return core_find_newest_students_csv(dir, out_path, out_size);
}

/* ---------- 8.5 删除 / 修改 ---------- */

/*
 * 按数组下标删除一名学生，后续元素整体前移。
 *
 * 主要逻辑：
 *   1. 从 index 开始，用后一个元素覆盖前一个元素
 *   2. 删除后重新计算排名并刷新显示映射
 */
int data_delete_student(int index)
{
    data_ensure_initialized();

    if (index < 0 || index >= s_count) return 0;

    // 从 index 开始，用后一个元素覆盖前一个元素
    for (int i = index; i + 1 < s_count; i++) {
        s_students[i] = s_students[i + 1];
    }
    s_count--;

    // 删除后重新计算排名并刷新显示映射
    core_recalc_rank(s_students, s_count);
    core_rebuild_display();
    return 1;
}

/*
 * 直接在指定年级和班级下新增一名空白学生，其他字段等待填写。
 *
 * 主要逻辑：
 *   1. 调用 data_ensure_initialized()
 *   2. 调用 memset()
 *   3. 调用 core_copy_trimmed()
 *   4. 调用 core_rebuild_display()
 */
int data_add_student(const char* grade, const char* class_name)
{
    data_ensure_initialized();

    if (grade == NULL || grade[0] == '\0') return -1;
    if (class_name == NULL || class_name[0] == '\0') return -1;
    if (s_count >= 200) return -1;

    StudentCSV* s = &s_students[s_count];
    memset(s, 0, sizeof(*s));

    core_copy_trimmed(s->grade, sizeof(s->grade), grade);
    core_copy_trimmed(s->class_name, sizeof(s->class_name), class_name);

    s->gender = false;
    s->is_new = true;
    s->new_filled_mask = (uint16_t)((1u << STU_FIELD_GRADE) |
        (1u << STU_FIELD_CLASS));

    s_count++;
    core_rebuild_display();

    return s_count - 1;
}

/*
 * 校验学号文本：必须为 12 位数字且不能全为 0。
 *
 * 主要逻辑：
 *   1. 12 位全 0 也不是合法学号
 *   2. 调用 strlen()
 *   3. 循环处理：size_t i = 0; i < len; i++
 */
int data_validate_student_id(const char* id_text)
{
    uint64_t value = 0;
    size_t len;

    if (id_text == NULL) return 0;

    len = strlen(id_text);
    if (len != 12) return 0;

    for (size_t i = 0; i < len; i++) {
        if (id_text[i] < '0' || id_text[i] > '9') return 0;
        value = value * 10u + (uint64_t)(id_text[i] - '0');
    }

    // 12 位全 0 也不是合法学号
    return value != 0;
}

/*
 * 按学号新增学生：依次做格式校验、重复检查和容量检查，通过后初始化新行。
 *
 * 主要逻辑：
 *   1. 解析为数字学号
 *   2. 学号不能重复
 */
int data_add_student_by_id(const char* id_text)
{
    uint64_t id = 0;

    data_ensure_initialized();

    if (!data_validate_student_id(id_text)) return -4;

    // 解析为数字学号
    if (!core_parse_uint64_text(id_text, &id)) return -1;
    if (id == 0) return -1;

    // 学号不能重复
    for (int i = 0; i < s_count; i++) {
        if (s_students[i].id == id) return -2;
    }

    if (s_count >= 200) return -3;

    StudentCSV* s = &s_students[s_count];
    memset(s, 0, sizeof(*s));

    s->id = id;
    s->gender = false;
    s->is_new = true;
    s->new_filled_mask = (uint16_t)(1u << STU_FIELD_ID);

    s_count++;
    core_rebuild_display();

    return s_count - 1;
}

/*
 * 校验新增行指定字段是否已经正确填写；已有老数据一律视为有效。
 *
 * 主要逻辑：
 *   1. 只有新增且还没填完的行参与校验
 *   2. bool 只有男/女两种，填过就有效
 *   3. 总分、绩点、排名由系统实时计算
 */
int data_validate_student_field(int index, int field)
{
    data_ensure_initialized();

    if (index < 0 || index >= s_count) return 0;
    if (field < 0 || field >= STU_FIELD_COUNT) return 0;

    const StudentCSV* s = &s_students[index];

    // 只有新增且还没填完的行参与校验
    if (!s->is_new) return 1;

    if ((s->new_filled_mask & (1u << field)) == 0) return 0;

    switch (field) {
    case STU_FIELD_GRADE:
        return s->grade[0] != '\0';

    case STU_FIELD_CLASS:
        return s->class_name[0] != '\0';

    case STU_FIELD_ID:
        if (s->id == 0) return 0;
        for (int i = 0; i < s_count; i++) {
            if (i != index && s_students[i].id == s->id) return 0;
        }
        return 1;

    case STU_FIELD_NAME:
        return s->name[0] != '\0';

    case STU_FIELD_GENDER:
        return 1;  // bool 只有男/女两种，填过就有效

    case STU_FIELD_REGULAR:
        return s->regular_score >= 0.0f && s->regular_score <= 100.0f;

    case STU_FIELD_MIDTERM:
        return s->midterm_score >= 0.0f && s->midterm_score <= 100.0f;

    case STU_FIELD_FINAL:
        return s->final_score >= 0.0f && s->final_score <= 100.0f;

    case STU_FIELD_SCORE:
    case STU_FIELD_GPA:
    case STU_FIELD_RANK:
        return 1;  // 总分、绩点、排名由系统实时计算

    default:
        return 0;
    }
}

/*
 * 记录新增行某个字段已经填写成功；三项成绩都填写后，总分、绩点、排名视为自动完成。
 *
 * 主要逻辑：
 *   1. 把字段对应位置 1；三项成绩齐全时同时置总分、绩点、排名位。
 */
static void core_mark_new_field_filled(StudentCSV* student, int field)
{
    if (student == NULL || !student->is_new) return;
    if (field < 0 || field >= STU_FIELD_COUNT) return;

    student->new_filled_mask |= (uint16_t)(1u << field);

    if (field == STU_FIELD_REGULAR || field == STU_FIELD_MIDTERM || field == STU_FIELD_FINAL) {
        uint16_t need = (uint16_t)((1u << STU_FIELD_REGULAR) |
            (1u << STU_FIELD_MIDTERM) |
            (1u << STU_FIELD_FINAL));
        if ((student->new_filled_mask & need) == need) {
            student->new_filled_mask |= (uint16_t)((1u << STU_FIELD_SCORE) |
                (1u << STU_FIELD_GPA) |
                (1u << STU_FIELD_RANK));
        }
    }
}

/*
 * 修改指定学生的一个字段。 所有文本先解析成正确类型，校验通过后才写回数据。
 *
 * 主要逻辑：
 *   1. 学号只在新增时输入，之后不允许修改
 *   2. 原地刷新排名，不改变行顺序
 *   3. 总分由平时/期中/期末按权重实时计算，不允许手动修改
 *   4. 绩点由总分实时计算，不允许手动修改
 *   5. 排名由系统根据总分自动计算，不允许手动修改
 */
int data_update_student_field(int index, int field, const char* text)
{
    StudentCSV* student;

    data_ensure_initialized();

    if (index < 0 || index >= s_count || text == NULL) return 0;
    if (field < 0 || field >= STU_FIELD_COUNT) return 0;

    student = &s_students[index];

    switch (field) {
    case STU_FIELD_GRADE:
        core_copy_trimmed(student->grade, sizeof(student->grade), text);
        if (student->is_new && student->grade[0] == '\0') {
            student->new_filled_mask &= (uint16_t)~(1u << STU_FIELD_GRADE);
        }
        else {
            core_mark_new_field_filled(student, STU_FIELD_GRADE);
        }
        break;

    case STU_FIELD_CLASS:
        core_copy_trimmed(student->class_name, sizeof(student->class_name), text);
        if (student->is_new && student->class_name[0] == '\0') {
            student->new_filled_mask &= (uint16_t)~(1u << STU_FIELD_CLASS);
        }
        else {
            core_mark_new_field_filled(student, STU_FIELD_CLASS);
        }
        break;

    case STU_FIELD_ID:
        // 学号只在新增时输入，之后不允许修改
        return 0;

    case STU_FIELD_NAME:
        core_copy_trimmed(student->name, sizeof(student->name), text);
        if (student->name[0] == '\0') return 0;
        core_mark_new_field_filled(student, STU_FIELD_NAME);
        break;

    case STU_FIELD_GENDER:
        if (!core_parse_gender_text(text, &student->gender)) return 0;
        core_mark_new_field_filled(student, STU_FIELD_GENDER);
        break;

    case STU_FIELD_REGULAR:
    case STU_FIELD_MIDTERM:
    case STU_FIELD_FINAL: {
        float v = 0.0f;
        if (!core_parse_float_text(text, &v)) return 0;
        if (v < 0.0f || v > 100.0f) return 0;

        if (field == STU_FIELD_REGULAR) {
            student->regular_score = v;
        }
        else if (field == STU_FIELD_MIDTERM) {
            student->midterm_score = v;
        }
        else {
            student->final_score = v;
        }

        student->score = data_calc_total(student->regular_score,
            student->midterm_score, student->final_score);
        student->gpa = data_calc_gpa(student->score);

        core_mark_new_field_filled(student, field);
        core_recalc_rank_inplace(s_students, s_count);  // 原地刷新排名，不改变行顺序
        break;
    }

    case STU_FIELD_SCORE:
        // 总分由平时/期中/期末按权重实时计算，不允许手动修改
        return 0;

    case STU_FIELD_GPA:
        // 绩点由总分实时计算，不允许手动修改
        return 0;

    case STU_FIELD_RANK:
        // 排名由系统根据总分自动计算，不允许手动修改
        return 0;

    default:
        return 0;
    }

    core_rebuild_display();
    return 1;
}
/* ---------- 8.6 筛选 ---------- */

/*
 * 获取某个字段可用于筛选的不同选项。 返回值可能大于 max_opts，调用者可用返回值判断真实选项数。
 *
 * 主要逻辑：
 *   1. 遍历所有学生，收集去重后的选项
 *   2. 班级筛选使用“年级-班级”组合，避免不同年级的同名班级合并
 *   3. 已经在候选列表中则跳过
 *   4. 只把前 max_opts 个写入缓冲区，但 option_count 统计真实数量
 *   5. 对已写入缓冲区的部分按自然顺序排序，让列表更易读
 *   6. 把 options[j] 整体后移一位；用 memmove 避免源和目标重叠
 */
int data_get_filter_options(int field, char options[][64], int max_opts)
{
    int option_count = 0;
    int visible;

    data_ensure_initialized();

    if (options == NULL || max_opts <= 0) return 0;
    if (field != STU_FIELD_GRADE && field != STU_FIELD_CLASS &&
        field != STU_FIELD_GENDER && field != STU_FIELD_SCORE) return 0;

    // 遍历所有学生，收集去重后的选项
    for (int i = 0; i < s_count; i++) {
        char candidate[64];
        int exists = 0;

        if (field == STU_FIELD_SCORE) {
            core_score_range_text(s_students[i].score, candidate, sizeof(candidate));
        }
        else if (field == STU_FIELD_CLASS) {
            // 班级筛选使用“年级-班级”组合，避免不同年级的同名班级合并
            core_grade_class_text(&s_students[i], candidate, sizeof(candidate));
        }
        else {
            core_field_text(&s_students[i], field, candidate, sizeof(candidate));
        }

        if (candidate[0] == '\0') continue;

        // 已经在候选列表中则跳过
        for (int j = 0; j < option_count && j < max_opts; j++) {
            if (strcmp(options[j], candidate) == 0) {
                exists = 1;
                break;
            }
        }
        if (exists) continue;

        // 只把前 max_opts 个写入缓冲区，但 option_count 统计真实数量
        if (option_count < max_opts) {
            snprintf(options[option_count], 64, "%s", candidate);
        }
        option_count++;
    }

    // 对已写入缓冲区的部分按自然顺序排序，让列表更易读
    visible = option_count < max_opts ? option_count : max_opts;
    for (int i = 1; i < visible; i++) {
        char key[64];
        int j;

        snprintf(key, sizeof(key), "%s", options[i]);
        for (j = i - 1; j >= 0 && core_compare_text_natural(options[j], key) > 0; j--) {
            // 把 options[j] 整体后移一位；用 memmove 避免源和目标重叠
            memmove(options[j + 1], options[j], strlen(options[j]) + 1);
        }
        memmove(options[j + 1], key, strlen(key) + 1);
    }

    return option_count;
}

/*
 * 应用筛选条件。
 *
 * 主要逻辑：
 *   1. 调用 data_ensure_initialized()
 *   2. 调用 core_copy_trimmed()
 *   3. 调用 core_rebuild_display()
 */
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

/*
 * 清除筛选，恢复显示全部数据。
 *
 * 主要逻辑：
 *   1. 调用 data_ensure_initialized()
 *   2. 调用 core_rebuild_display()
 */
void data_clear_filter(void)
{
    data_ensure_initialized();

    s_filter_field = -1;
    s_filter_option[0] = '\0';
    core_rebuild_display();
}

/* ---------- 8.7 搜索 ---------- */

/*
 * 在当前显示行中搜索关键字。 对每个学生的 8 个字段都做一次包含匹配；匹配到的学生只输出一次。
 *
 * 主要逻辑：
 *   1. 依次检查 8 个字段，只要任意字段包含关键字就算命中
 *   2. 调用 data_ensure_initialized()
 *   3. 循环处理：int row = 0; row < s_display_count; row++
 *   4. 循环处理：int field = 0; field < STU_FIELD_COUNT && !hit; field++
 *   5. 调用 core_field_text()
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

        // 依次检查 8 个字段，只要任意字段包含关键字就算命中
        for (int field = 0; field < STU_FIELD_COUNT && !hit; field++) {
            char value[64];
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

/* ---------- 8.8 排序 ---------- */

/*
 * 按指定字段排序学生数组，并刷新显示行映射。
 *
 * 主要逻辑：
 *   1. 调用 data_ensure_initialized()
 *   2. 调用 qsort()
 *   3. 调用 core_rebuild_display()
 */
void data_sort_students(int field, bool ascending)
{
    data_ensure_initialized();

    if (field < 0 || field >= STU_FIELD_COUNT) return;

    s_sort_field = field;
    s_sort_direction = ascending ? 1 : -1;
    qsort(s_students, (size_t)s_count, sizeof(StudentCSV), core_compare_students);
    core_rebuild_display();
}

/* ---------- 8.9 帮助图片 ---------- */

/* 帮助图片张数：增减帮助图片时只改这里的数字，并保证 help_png 目录中有 help1.png...helpN.png。 */
enum { HELP_IMAGE_COUNT = 10 };

/* 帮助图片路径缓存；第一次调用时生成，之后直接复用。 */
static char s_help_image_paths[HELP_IMAGE_COUNT][128];
static int s_help_image_paths_ready = 0;

/*
 * 返回帮助图片总张数。
 *
 * 主要逻辑：
 *   1. 返回帮助图片总张数 HELP_IMAGE_COUNT。
 */
int ui_help_get_image_count(void)
{
    return HELP_IMAGE_COUNT;
}

/*
 * 返回第 index 张帮助图片的路径字符串。
 *
 * 主要逻辑：
 *   1. 循环处理：int i = 0; i < HELP_IMAGE_COUNT; i++
 *   2. 调用 snprintf()
 */
const void* ui_help_get_image_src(int index)
{
    if (index < 0 || index >= HELP_IMAGE_COUNT) return NULL;

    if (!s_help_image_paths_ready) {
        for (int i = 0; i < HELP_IMAGE_COUNT; i++) {
            snprintf(s_help_image_paths[i], sizeof(s_help_image_paths[i]),
                "%shelp%d.png", "A:help_png/", i + 1);
        }
        s_help_image_paths_ready = 1;
    }

    return s_help_image_paths[index];
}

/* ---------- 8.10 首次运行标志文件 ---------- */

/* 首次运行标志文件名：程序启动时检查该文件，不存在则自动弹出帮助。 */
static const char CORE_FLAG_FILE_NAME[] = "help_shown.flag";

/*
 * 获取当前 exe 所在目录的 UTF-8 路径（不带末尾分隔符）；成功返回 1。
 *
 * 主要逻辑：
 *   1. 保存 exe 完整路径
 *   2. 保存路径长度
 *   3. 指向最后一个路径分隔符
 *   4. UTF-8 路径长度
 *   5. 参数检查
 *   6. 初始化输出
 */
static int core_get_exe_dir(char* out, size_t out_size)
{
    wchar_t exe_path[1024];  // 保存 exe 完整路径
    DWORD n;  // 保存路径长度
    wchar_t* slash;  // 指向最后一个路径分隔符
    size_t len;  // UTF-8 路径长度

    if (out == NULL || out_size == 0) return 0;  // 参数检查
    out[0] = '\0';  // 初始化输出
    n = GetModuleFileNameW(NULL, exe_path, (DWORD)(sizeof(exe_path) / sizeof(exe_path[0])));  // 取 exe 路径
    if (n == 0 || n >= (DWORD)(sizeof(exe_path) / sizeof(exe_path[0]))) return 0;  // 获取失败
    slash = wcsrchr(exe_path, L'\\');  // 找最后一个反斜杠
    if (slash == NULL) return 0;  // 没有目录则失败
    slash[1] = L'\0';  // 只保留目录部分
    if (!core_wide_to_utf8(exe_path, out, out_size)) return 0;  // 转成 UTF-8

    /* 去掉末尾分隔符，拼接文件名时统一补反斜杠。 */
    len = strlen(out);  // 取得当前长度
    while (len > 1 && (out[len - 1] == '\\' || out[len - 1] == '/')) {  // 去掉末尾分隔符
        if (len == 3 && out[1] == ':') break;  // 保留 "C:\"
        out[--len] = '\0';  // 截断末尾字符
    }
    return 1;  // 返回成功
}

/*
 * 检查程序所在 bin 目录下是否存在首次运行标志文件。
 *
 * 主要逻辑：
 *   1. exe 所在目录
 *   2. 标志文件完整路径
 *   3. 标志文件宽字符路径
 *   4. 文件属性
 *   5. 取 exe 目录失败
 *   6. 拼接标志文件路径
 */
int core_flag_file_exists(void)
{
    char dir[1024];  // exe 所在目录
    char path[1200];  // 标志文件完整路径
    wchar_t wpath[1200];  // 标志文件宽字符路径
    DWORD attr;  // 文件属性

    if (!core_get_exe_dir(dir, sizeof(dir))) return 0;  // 取 exe 目录失败
    snprintf(path, sizeof(path), "%s\\%s", dir, CORE_FLAG_FILE_NAME);  // 拼接标志文件路径
    if (MultiByteToWideChar(CP_UTF8, 0, path, -1, wpath,
        (int)(sizeof(wpath) / sizeof(wpath[0]))) <= 0) {
        return 0;  // 路径转换失败
    }
    attr = GetFileAttributesW(wpath);  // 读取文件属性
    if (attr == INVALID_FILE_ATTRIBUTES) return 0;  // 文件不存在
    return (attr & FILE_ATTRIBUTE_DIRECTORY) ? 0 : 1;  // 是文件则存在
}

/*
 * 在程序所在 bin 目录下创建首次运行标志文件。
 *
 * 主要逻辑：
 *   1. exe 所在目录
 *   2. 标志文件完整路径
 *   3. 标志文件宽字符路径
 *   4. 文件句柄
 *   5. 取 exe 目录失败
 *   6. 拼接标志文件路径
 */
int core_flag_file_create(void)
{
    char dir[1024];  // exe 所在目录
    char path[1200];  // 标志文件完整路径
    wchar_t wpath[1200];  // 标志文件宽字符路径
    FILE* fp;  // 文件句柄

    if (!core_get_exe_dir(dir, sizeof(dir))) return 0;  // 取 exe 目录失败
    snprintf(path, sizeof(path), "%s\\%s", dir, CORE_FLAG_FILE_NAME);  // 拼接标志文件路径
    if (MultiByteToWideChar(CP_UTF8, 0, path, -1, wpath,
        (int)(sizeof(wpath) / sizeof(wpath[0]))) <= 0) {
        return 0;  // 路径转换失败
    }
    fp = _wfopen(wpath, L"wb");  // 创建或覆盖标志文件
    if (fp == NULL) return 0;  // 创建失败
    fputs("help shown\n", fp);  // 写入标志内容
    fclose(fp);  // 关闭文件
    return 1;  // 创建成功
}
