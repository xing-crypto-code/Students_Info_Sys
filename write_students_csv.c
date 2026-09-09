/**
 * @file write_students_csv.c
 * @brief 将学生结构体数组输出/保存为 CSV 文件的实现。
 *
 * 使用方式示例：
 *   StudentCSV students[MAX_STUDENTS];
 *   char header[MAX_LINE_LEN] = { 0 };
 *   int n = read_students_csv("students.csv", students, MAX_STUDENTS,
 *                             header, sizeof(header));
 *
 *   recalculate_ranking_by_score(students, n);
 *
 *   // header 可以传入读到的表头，也可以传 NULL 使用默认表头
 *   write_students_csv("output.csv", students, n, header);
 */

#include "write_students_csv.h"

#include <inttypes.h>
#include <stdio.h>

/**
 * 将学生结构体数组写入 CSV 文件。
 *
 * 本函数直接读取传入的 students 数组中的当前内容并输出，
 * 不会重新读取原 CSV 文件。
 *
 * @param filename  输出 CSV 文件名
 * @param students  学生结构体数组（只读取，不修改）
 * @param count     学生人数
 * @param header    表头；NULL 或空字符串时使用默认表头
 * @return 成功返回写入的学生人数；失败返回 -1
 */
int write_students_csv(const char *filename, const StudentCSV *students,
                       int count, const char *header)
{
    FILE *fp;

    /* 参数检查 */
    if(filename == NULL) return -1;
    if(count < 0) return -1;
    if(count > 0 && students == NULL) return -1;

    /* 未指定表头时使用与实例一致的默认表头 */
    if(header == NULL || header[0] == '\0') {
        header = STUDENT_CSV_HEADER;
    }

    /* 用二进制写模式，方便在文件开头写 UTF-8 BOM */
    fp = fopen(filename, "wb");
    if(fp == NULL) return -1;

    /* UTF-8 BOM，避免 Windows 下 Excel 打开中文乱码 */
    fputs("\xEF\xBB\xBF", fp);

    /* 写表头 */
    fputs(header, fp);
    fputs("\r\n", fp);

    /* 逐行写学生数据 */
    for(int i = 0; i < count; i++) {
        fprintf(fp,
                "%s,%s,%" PRIu64 ",%s,%s,%.1f,%.1f,%" PRIu16 "\r\n",
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
