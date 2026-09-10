/**
 * @file read_students_csv.h
 * @brief 读取 students.csv 的数据模块头文件。
 *
 * 与 read_students_csv.c 配套使用。
 * 主要提供：
 *   1. 学生信息结构体 StudentCSV；
 *   2. CSV 读取函数 read_students_csv()；
 *   3. 按分数重新计算排名函数 recalculate_ranking_by_score()。
 */

#ifndef READ_STUDENTS_CSV_H
#define READ_STUDENTS_CSV_H

#include "../Inc/stu.h"

/* ========== 函数声明 ========== */

/**
 * 读取 CSV 文件到结构体数组。
 *
 * CSV 格式（8 列，第一行是表头）：
 *   年级,班级,学号,姓名,性别,分数,绩点,排名
 *
 * @param filename    CSV 文件名
 * @param students    调用者提供的结构体数组
 * @param capacity    数组容量（最多能存多少个学生）
 * @param header      用于单独保存表头的 char 数组（第一行写入这里）
 * @param header_size header 数组大小
 * @return 成功读取的学生行数；文件打不开返回 -1；空文件返回 0
 */
int read_students_csv(const char *filename, StudentCSV *students, int capacity,
                      char *header, size_t header_size);

/**
 * 按照分数重新计算排名。
 *
 * 功能：
 *   1. 读取传入的结构体数组 students；
 *   2. 按 score 从高到低排序；
 *   3. 直接修改数组中每个元素对应的 rank 字段，依次设为 1、2、3...
 *
 * 无论 CSV 中原来的“排名”列写了什么，调用后都会覆盖为重新计算的结果。
 *
 * @param students 学生结构体数组（会被直接修改 rank 字段）
 * @param count    学生人数
 */
void recalculate_ranking_by_score(StudentCSV *students, int count);

#endif /* READ_STUDENTS_CSV_H */
