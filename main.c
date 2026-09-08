#include <stdio.h>
#include "console.h"
#include "stu.h"

int main(void)
{
    consoleInit();
    stuInit();

    Student s1 = {0};

    s1.id = 1001;
    snprintf(s1.name, sizeof(s1.name), "张三");
    snprintf(s1.major, sizeof(s1.major), "电子信息工程");
    snprintf(s1.class_number, sizeof(s1.class_number), "电信1班");
    s1.score = 92.0f;
    s1.gender = 'M';

    Student s2 = {0};

    s2.id = 1002;
    snprintf(s2.name, sizeof(s2.name), "李四");
    snprintf(s2.major, sizeof(s2.major), "电子信息工程");
    snprintf(s2.class_number, sizeof(s2.class_number), "电信1班");
    s2.score = 85.0f;
    s2.gender = 'F';

    addStudent(&s1);
    addStudent(&s2);

    printf("当前学生数量：%d\n\n", getStudentCount());

    /* 查询1002 */
    Student *p = findById(1002);

    if (p != NULL)
    {
        printf("查询成功！\n");
        printf("学号：%d\n", p->id);
        printf("姓名：%s\n", p->name);
        printf("专业：%s\n", p->major);
        printf("班级：%s\n", p->class_number);
        printf("成绩：%.2f\n", p->score);
    }
    else
    {
        printf("未找到该学生。\n");
    }

    /* 查询不存在的学生 */
    p = findById(9999);

    if (p != NULL)
    {
        printf("查询成功！\n");
    }
    else
    {
        printf("\n学号9999不存在。\n");
    }

    return 0;
}