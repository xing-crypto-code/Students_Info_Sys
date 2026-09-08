#include "stu.h"
#include <stddef.h>

/* 学生数据 */
static Student students[MAX_STUDENT];

/* 当前学生数量 */
static int studentCount = 0;

/* 检查学生信息是否合法 */
static int isStudentValid(const Student *stu)
{
    if (stu == NULL)//防止传入空指针
    {
        return 0;
    }

    if (stu->id <= 0)//防止id违法
    {
        return 0;
    }

    if (stu->name[0] == '\0')//防止姓名为空
    {
        return 0;
    }
    {
        return 0;
    }

    if (stu->score < 0.0f || stu->score > 100.0f)//防止成绩不合法
    {
        return 0;
    }

    return 1;
}


/* 初始化学生信息 */
void stuInit(void)
{
    studentCount = 0;
}

/* 添加学生 */
int addStudent(const Student *stu)
{
    /* 检查学生信息是否合法 */
    if (!isStudentValid(stu))
    {
        return 0;
    }

    /* 检查学生人数是否超过数组容量 */
    if (studentCount >= MAX_STUDENT)
    {
        return 0;
    }

    /* 检查学号是否重复 */
    for (int i = 0; i < studentCount; i++)
    {
        if (students[i].id == stu->id)
        {
            return 0;
        }
    }

    /* 添加学生 */
    students[studentCount] = *stu;
    studentCount++;

    return 1;
}

/* 获取当前学生数量 */
int getStudentCount(void)
{
    return studentCount;
}


/* 获取指定下标的学生 */
Student *getStudent(int index)
{
    if (index < 0 || index >= studentCount)
    {
        return NULL;
    }

    return &students[index];
}

/* 根据学号查找学生 */
Student *findById(int id)
{
    for (int i = 0; i < studentCount; i++)
    {
        if (students[i].id == id)
        {
            return &students[i];
        }
    }

    return NULL;
}