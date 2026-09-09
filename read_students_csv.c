/**
 * @file read_students_csv.c
 * @brief 读取 students.csv，将每一行学生数据依次保存到结构体数组。
 *
 * CSV 约定（8 列，第一行是表头）：
 *   年级,班级,学号,姓名,性别,分数,绩点,排名
 *   2023级,计算机1班,202300000001,张三,男,88.5,3.5,0
 *
 * 功能：
 *   1. 第一行（表头）单独保存到调用者提供的 char 数组中；
 *   2. 后续每一行解析为一个 StudentCSV 结构体，放入结构体数组；
 *   3. 性别使用 bool 表示：true = 男，false = 女；
 *   4. CSV 中的“排名”列只是初始/占位数据，最终排名以导入后
 *      调用 recalculate_ranking_by_score() 按分数重新计算为准。
 *
 * 编译运行示例（在文件所在目录执行）：
 *   gcc -std=c99 -Wall -Wextra -o read_students_csv read_students_csv.c
 *   ./read_students_csv
 *   （Windows 下为 read_students_csv.exe）
 */

#include "read_students_csv.h"

#include <ctype.h>
#include <inttypes.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

/* ========== 内部常量 ========== */

#define MAX_COLUMNS      8     /* CSV 列数：含排名列 */

/* ========== 工具函数 ========== */

/**
 * 去掉字符串首尾的空白字符（空格、Tab、回车、换行等）。
 * 直接在原字符串上修改。
 */
static void trim_whitespace(char *s)
{
    char *start = s;
    char *end;

    if(s == NULL) return;

    /* 去掉开头空白 */
    while(*start != '\0' && isspace((unsigned char)*start)) start++;

    if(*start == '\0') {
        s[0] = '\0';
        return;
    }

    /* 去掉结尾空白 */
    end = start + strlen(start) - 1;
    while(end >= start && isspace((unsigned char)*end)) end--;
    end[1] = '\0';

    /* 如果开头有空白，把有效内容移到数组头 */
    if(start != s) {
        memmove(s, start, strlen(start) + 1);
    }
}

/**
 * 去除 UTF-8 BOM（EF BB BF）。
 * 有些 Windows 编辑器保存 UTF-8 文件时会自动添加 BOM，
 * 它会影响第一列表头的解析，因此开头若检测到就跳过去。
 */
static void remove_utf8_bom(char *s)
{
    if(s == NULL) return;

    unsigned char *p = (unsigned char *)s;
    if(p[0] == 0xEF && p[1] == 0xBB && p[2] == 0xBF) {
        memmove(s, s + 3, strlen(s + 3) + 1);
    }
}

/**
 * 把一行 CSV 文本按逗号拆分成若干字段。
 *
 * @param line  待拆分的行（会被修改，逗号替换为 '\0'）
 * @param fields 输出字段指针数组，容量至少 max_fields
 * @param max_fields 最多拆分出的字段数
 * @return 实际拆分出的字段个数
 *
 * 说明：本实现针对“姓名/班级不含逗号”的常见课程设计 CSV，
 * 未处理引号转义；如果字段本身包含逗号，可以在此基础上扩展。
 */
static int split_csv_line(char *line, char **fields, int max_fields)
{
    int count = 0;
    char *p = line;

    if(line == NULL || fields == NULL) return 0;

    while(count < max_fields && *p != '\0') {
        /* 跳过连续逗号产生的空字段（这里按常见情况不把空字段计入） */
        if(*p == ',') {
            p++;
            continue;
        }

        fields[count++] = p;

        /* 找到本字段结尾的逗号或行尾 */
        while(*p != '\0' && *p != ',') p++;

        if(*p == ',') {
            *p = '\0';
            p++;
        }
    }

    return count;
}

/**
 * 把性别文本转换成 bool。
 * 支持：男/女、M/F、m/f、1/0、true/false。
 * 解析失败时返回 false，并由调用者按错误行处理。
 *
 * @param s 性别文本
 * @param out 输出结果：true=男，false=女
 * @return 1 表示解析成功，0 表示无法识别
 */
static int parse_gender(const char *s, bool *out)
{
    if(s == NULL || out == NULL) return 0;

    if(strcmp(s, "男") == 0 || strcmp(s, "M") == 0 ||
       strcmp(s, "m") == 0 || strcmp(s, "1") == 0 ||
       strcmp(s, "true") == 0 || strcmp(s, "TRUE") == 0) {
        *out = true;
        return 1;
    }

    if(strcmp(s, "女") == 0 || strcmp(s, "F") == 0 ||
       strcmp(s, "f") == 0 || strcmp(s, "0") == 0 ||
       strcmp(s, "false") == 0 || strcmp(s, "FALSE") == 0) {
        *out = false;
        return 1;
    }

    return 0;
}

/**
 * 把一个字符串解析为 uint64_t。
 *
 * @param s 输入字符串
 * @param out 输出值
 * @return 1 表示成功；0 表示格式错误或超出 uint64_t 范围
 */
static int parse_uint64(const char *s, uint64_t *out)
{
    char *end = NULL;
    unsigned long long value;

    if(s == NULL || out == NULL || s[0] == '\0') return 0;

    value = strtoull(s, &end, 10);

    /* end == s 表示没有数字；*end != '\0' 表示后面还有多余字符 */
    if(end == s || *end != '\0') return 0;

    *out = (uint64_t)value;
    return 1;
}

/**
 * 把一个字符串解析为 uint16_t。
 */
static int parse_uint16(const char *s, uint16_t *out)
{
    char *end = NULL;
    unsigned long value;

    if(s == NULL || out == NULL || s[0] == '\0') return 0;

    value = strtoul(s, &end, 10);
    if(end == s || *end != '\0') return 0;
    if(value > UINT16_MAX) return 0;

    *out = (uint16_t)value;
    return 1;
}

/**
 * 把一个字符串解析为 float。
 */
static int parse_float(const char *s, float *out)
{
    char *end = NULL;
    float value;

    if(s == NULL || out == NULL || s[0] == '\0') return 0;

    value = strtof(s, &end);
    if(end == s || *end != '\0') return 0;

    *out = value;
    return 1;
}

/* ========== CSV 解析 ========== */

/**
 * 将一行解析为一个 StudentCSV。
 *
 * @param line 原始 CSV 行（会被拆分函数修改）
 * @param stu  输出结构体
 * @return 1 表示成功；0 表示字段数不足或类型转换失败
 */
static int parse_student_line(char *line, StudentCSV *stu)
{
    char *fields[MAX_COLUMNS];
    int field_count;

    if(stu == NULL) return 0;

    field_count = split_csv_line(line, fields, MAX_COLUMNS);
    if(field_count < MAX_COLUMNS) return 0;

    /* 去掉每个字段首尾空白 */
    for(int i = 0; i < field_count; i++) {
        trim_whitespace(fields[i]);
    }

    /* 复制字符串字段，限制长度并确保以 '\0' 结尾 */
    strncpy(stu->grade, fields[0], MAX_FIELD_LEN - 1);
    stu->grade[MAX_FIELD_LEN - 1] = '\0';

    strncpy(stu->class_name, fields[1], MAX_FIELD_LEN - 1);
    stu->class_name[MAX_FIELD_LEN - 1] = '\0';

    strncpy(stu->name, fields[3], MAX_FIELD_LEN - 1);
    stu->name[MAX_FIELD_LEN - 1] = '\0';

    /* 解析数值字段 */
    if(!parse_uint64(fields[2], &stu->id))       return 0;
    if(!parse_gender(fields[4], &stu->gender))   return 0;
    if(!parse_float(fields[5], &stu->score))     return 0;
    if(!parse_float(fields[6], &stu->gpa))       return 0;
    if(!parse_uint16(fields[7], &stu->rank))     return 0;

    return 1;
}

/**
 * 读取 CSV 文件到结构体数组。
 *
 * @param filename    CSV 文件名
 * @param students    调用者提供的结构体数组
 * @param capacity    数组容量（最多能存多少个学生）
 * @param header      用于单独保存表头的 char 数组（第一行写入这里）
 * @param header_size header 数组大小
 * @return 成功读取的学生行数；文件打不开返回 -1；空文件返回 0
 *
 * 约定：
 *  - 第一行是表头，会存入 header 数组中（不会存入学生结构体）；
 *  - 第二行开始是学生数据；
 *  - 空行自动跳过；
 *  - 某行解析失败时打印提示并跳过该行。
 */
int read_students_csv(const char *filename, StudentCSV *students, int capacity,
                      char *header, size_t header_size)
{
    FILE *fp;
    char line[MAX_LINE_LEN];
    int count = 0;

    if(filename == NULL || students == NULL || header == NULL) return -1;

    fp = fopen(filename, "r");
    if(fp == NULL) {
        perror(filename);
        return -1;
    }

    /* 读取第一行作为表头 */
    if(header_size > 0) {
        if(fgets(header, (int)header_size, fp) != NULL) {
            remove_utf8_bom(header);
            trim_whitespace(header);
        } else {
            header[0] = '\0';
        }
    }

    /* 读取后续每一行学生数据 */
    while(count < capacity && fgets(line, sizeof(line), fp) != NULL) {
        trim_whitespace(line);

        /* 跳过空行 */
        if(line[0] == '\0') continue;

        if(!parse_student_line(line, &students[count])) {
            printf("[警告] 第 %d 条数据解析失败，已跳过: %s\n", count + 1, line);
            continue;
        }

        count++;
    }

    fclose(fp);
    return count;
}

/* ========== 排名计算函数 ========== */

/**
 * 按分数从高到低比较两个学生。
 * 供 qsort 使用。
 */
static int cmp_score_desc(const void *a, const void *b)
{
    const StudentCSV *sa = (const StudentCSV *)a;
    const StudentCSV *sb = (const StudentCSV *)b;

    if(sa->score < sb->score) return 1;
    if(sa->score > sb->score) return -1;
    return 0;
}

/**
 * 按照分数重新计算排名。
 *
 * 功能：
 *   1. 读取传入的结构体数组 students；
 *   2. 按分数从高到低排序；
 *   3. 直接修改 students[i].rank，依次设为 1、2、3...
 *
 * 也就是说，无论 CSV 里原来的“排名”列写了什么，
 * 调用本函数后都会直接覆盖结构体数组中的 rank 字段。
 *
 * @param students 学生结构体数组（会被直接修改 rank 字段）
 * @param count    学生人数
 */
void recalculate_ranking_by_score(StudentCSV *students, int count)
{
    if(students == NULL || count <= 0) return;

    /* 按分数从高到低排序 */
    qsort(students, (size_t)count, sizeof(StudentCSV), cmp_score_desc);

    /* 排序后直接修改结构体数组中每个元素的 rank 字段 */
    for(int i = 0; i < count; i++) {
        students[i].rank = (uint16_t)(i + 1);
    }
}

/* ========== 主函数演示 ========== */
/* 如果要在其他程序中把本文件当作数据模块链接，可编译时定义
 * READ_STUDENTS_CSV_NO_MAIN，从而不包含下面的演示 main()。
 */
#ifndef READ_STUDENTS_CSV_NO_MAIN

/**
 * 输出表头。
 */
static void print_header(void)
{
    printf("排名  年级     班级          学号          姓名        性别  分数   绩点\n");
    printf("------------------------------------------------------------------------------------\n");
}

/**
 * 输出一个学生的信息。
 * 性别 true 显示“男”，false 显示“女”。
 */
static void print_student(const StudentCSV *s)
{
    printf("%-4" PRIu16 " %-8s %-12s %12" PRIu64 "  %-10s  %s  %6.1f  %4.1f\n",
           s->rank,
           s->grade,
           s->class_name,
           s->id,
           s->name,
           s->gender ? "男" : "女",
           s->score,
           s->gpa);
}

int main(void)
{
    /* 结构体数组：用于保存每一行学生数据 */
    StudentCSV students[MAX_STUDENTS];

    /* 表头单独存放在这个字符数组中 */
    char csv_header[MAX_LINE_LEN] = { 0 };

    const char *filename = "students.csv";
    int n = read_students_csv(filename, students, MAX_STUDENTS,
                              csv_header, sizeof(csv_header));

    if(n < 0) {
        printf("读取文件 %s 失败，请确认文件是否存在。\n", filename);
        return 1;
    }

    /* 第一步：显示 CSV 原始内容（按文件顺序，排名列只是初始值） */
    printf("CSV 表头: %s\n", csv_header);
    printf("成功读取 %d 条学生数据。\n\n", n);
    printf("===== 导入后的原始数据（CSV 顺序，排名列暂为初始值） =====\n");
    print_header();
    for(int i = 0; i < n; i++) {
        print_student(&students[i]);
    }

    /* 第二步：调用“按分数重新计算排名”函数 */
    recalculate_ranking_by_score(students, n);

    printf("\n===== 调用 recalculate_ranking_by_score() 后（按分数降序） =====\n");
    print_header();
    for(int i = 0; i < n; i++) {
        print_student(&students[i]);
    }

    return 0;
}

#endif /* READ_STUDENTS_CSV_NO_MAIN */
