/**
 * @file write_students_csv.h
 * @brief 将学生结构体数组输出/保存为 CSV 文件。
 *
 * 与 read_students_csv.h 配合使用：
 *   - read_students_csv.h  提供 StudentCSV 结构体和读取函数；
 *   - write_students_csv.h 提供写出 CSV 的函数。
 */

#ifndef WRITE_STUDENTS_CSV_H
#define WRITE_STUDENTS_CSV_H

#include "read_students_csv.h"

/* CSV 默认表头，与示例 students.csv 格式一致 */
#define STUDENT_CSV_HEADER "年级,班级,学号,姓名,性别,分数,绩点,排名"

/**
 * 将学生结构体数组写入 CSV 文件。
 *
 * 本函数不会重新读取原文件，而是直接读取传入的 students 结构体数组
 * 中当前保存的内容，逐条写入 filename。
 *
 * 输出格式与读取时的格式一致：
 *   年级,班级,学号,姓名,性别,分数,绩点,排名
 * 文件会写成 UTF-8 with BOM，便于 Windows 下 Excel/记事本直接打开。
 *
 * @param filename  输出 CSV 文件名
 * @param students  学生结构体数组（函数只读取其内容，不修改数组）
 * @param count     学生人数
 * @param header    表头字符串；如果传 NULL 或空字符串，
 *                  则使用默认表头 STUDENT_CSV_HEADER
 * @return 成功返回写入的学生人数；失败返回 -1
 */
int write_students_csv(const char *filename, const StudentCSV *students,
                       int count, const char *header);

#endif /* WRITE_STUDENTS_CSV_H */
