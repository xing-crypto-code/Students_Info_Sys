#ifndef FILE_H
#define FILE_H

/* 保存和读取当前学生数据，成功返回 1，失败返回 0 */
int saveToFile(const char *filename);
int loadFromFile(const char *filename);

#endif
