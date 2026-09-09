#include "console.h"
#include "file.h"
#include "stu.h"
#include <stdio.h>
#include <string.h>

#ifdef _WIN32
#include <windows.h>
#endif

void consoleInit(void)
{
#ifdef _WIN32
    /* 设置 Windows 控制台编码，避免中文输出乱码。 */
    SetConsoleOutputCP(CP_UTF8);
    SetConsoleCP(CP_UTF8);
#endif
}

/* 读取一整行输入，避免 scanf 留下换行符影响下一次输入。 */
static int readLine(char *buffer, size_t size)
{
    if (fgets(buffer, (int)size, stdin) == NULL)
    {
        return 0;
    }

    buffer[strcspn(buffer, "\r\n")] = '\0';
    return 1;
}

/* 读取整数；输入格式不正确时返回 0。 */
static int readInt(const char *prompt, int *value)
{
    char line[64];

    printf("%s", prompt);
    if (!readLine(line, sizeof(line)) || sscanf(line, "%d", value) != 1)
    {
        printf("输入格式错误。\n");
        return 0;
    }

    return 1;
}

/* 读取成绩；输入格式不正确时返回 0。 */
static int readFloat(const char *prompt, float *value)
{
    char line[64];

    printf("%s", prompt);
    if (!readLine(line, sizeof(line)) || sscanf(line, "%f", value) != 1)
    {
        printf("输入格式错误。\n");
        return 0;
    }

    return 1;
}

/* 读取学生的文字字段。 */
static int readText(const char *prompt, char *text, size_t size)
{
    printf("%s", prompt);
    if (!readLine(text, size))
    {
        printf("读取输入失败。\n");
        return 0;
    }

    return 1;
}

/* 显示一名学生的完整信息。 */
static void printStudent(const Student *student)
{
        printf("学号：%d | 姓名：%s | 年级：%s | 专业：%s | 班级：%s\n",
           student->id,
           student->name,
            student->year,
           student->major,
            student->class_number);
        printf("性别：%c | 电话：%s | 总分：%.2f | 平均分：%.2f | GPA：%.2f | 专业排名：%d\n",
            student->gender == '\0' ? '-' : student->gender,
                student->phone,
                student->totalScore,
                student->averageScore,
                student->gpa,
                student->majorRank);

        for (int i = 0; i < COURSE_COUNT; i++)
        {
         printf("  %s：%.2f（%.1f 学分）\n",
             student->courses[i].name,
             student->courses[i].score,
             student->courses[i].credit);
        }
}

/* 显示全部学生。 */
static void showAllStudents(void)
{
    if (getStudentCount() == 0)
    {
        printf("当前没有学生信息。\n");
        return;
    }

    for (int i = 0; i < getStudentCount(); i++)
    {
        Student *student = getStudent(i);
        printf("[%d] ", i + 1);
        printStudent(student);
    }
}

/* 添加一名学生。 */
static void addStudentFromConsole(void)
{
    Student student = {0};
    char gender[8];

    if (!readInt("请输入学号：", &student.id)
        || !readText("请输入姓名：", student.name, sizeof(student.name))
        || !readText("请输入年级：", student.year, sizeof(student.year))
        || !readText("请输入专业：", student.major, sizeof(student.major))
        || !readText("请输入班级：", student.class_number, sizeof(student.class_number))
        || !readText("请输入性别（M/F）：", gender, sizeof(gender))
        || !readText("请输入电话：", student.phone, sizeof(student.phone)))
    {
        return;
    }

    student.gender = gender[0];

    if (!setCoursesByMajor(&student))
    {
        printf("暂不支持该专业，目前支持电子信息工程、通信工程、计算机科学与技术。\n");
        return;
    }

    for (int i = 0; i < COURSE_COUNT; i++)
    {
        printf("请输入%s成绩：", student.courses[i].name);
        if (!readFloat("", &student.courses[i].score))
        {
            return;
        }
    }

    if (addStudent(&student))
    {
        calculateStudentResults(&student);
        printf("添加成功，平均分 %.2f，GPA %.2f。\n",
               student.averageScore, student.gpa);
    }
    else
    {
        printf("添加失败，请检查学号是否重复或学生信息是否合法。\n");
    }
}

/* 按学号查询一名学生。 */
static void findStudentByIdFromConsole(void)
{
    int id;

    if (!readInt("请输入要查询的学号：", &id))
    {
        return;
    }

    Student *student = findById(id);
    if (student == NULL)
    {
        printf("没有找到该学生。\n");
        return;
    }

    printStudent(student);
}

/* 按姓名查询，可以显示多名同名学生。 */
static void findStudentsByNameFromConsole(void)
{
    char name[20];
    Student results[MAX_STUDENT];

    if (!readText("请输入要查询的姓名：", name, sizeof(name)))
    {
        return;
    }

    int count = findByName(name, results, MAX_STUDENT);
    printf("共找到 %d 名学生。\n", count);
    for (int i = 0; i < count && i < MAX_STUDENT; i++)
    {
        printStudent(&results[i]);
    }
}

/* 根据学号修改学生信息，学号仍由原记录保留。 */
static void modifyStudentFromConsole(void)
{
    int id;
    Student newInfo = {0};
    char gender[8];

    if (!readInt("请输入要修改的学号：", &id)
        || !readInt("请输入新的学号（不会被使用）：", &newInfo.id)
        || !readText("请输入新姓名：", newInfo.name, sizeof(newInfo.name))
        || !readText("请输入新年级：", newInfo.year, sizeof(newInfo.year))
        || !readText("请输入新专业：", newInfo.major, sizeof(newInfo.major))
        || !readText("请输入新班级：", newInfo.class_number, sizeof(newInfo.class_number))
        || !readText("请输入新性别（M/F）：", gender, sizeof(gender))
        || !readText("请输入新电话：", newInfo.phone, sizeof(newInfo.phone)))
    {
        return;
    }

    newInfo.gender = gender[0];

    if (!setCoursesByMajor(&newInfo))
    {
        printf("暂不支持该专业。\n");
        return;
    }

    for (int i = 0; i < COURSE_COUNT; i++)
    {
        printf("请输入%s成绩：", newInfo.courses[i].name);
        if (!readFloat("", &newInfo.courses[i].score))
        {
            return;
        }
    }

    printf("修改结果：%s\n", modifyStudent(id, &newInfo) ? "成功" : "失败");
}

/* 删除学生。 */
static void deleteStudentFromConsole(void)
{
    int id;

    if (readInt("请输入要删除的学号：", &id))
    {
        printf("删除结果：%s\n", deleteStudent(id) ? "成功" : "未找到该学生");
    }
}

/* 显示成绩统计。 */
static void showStatistics(void)
{
    float average;
    float highest;
    float lowest;
    int passCount;

    if (!getStatistics(&average, &highest, &lowest, &passCount))
    {
        printf("暂无成绩数据。\n");
        return;
    }

    printf("平均分：%.2f，最高分：%.2f，最低分：%.2f，及格人数：%d\n",
           average, highest, lowest, passCount);
}

/*
 * 读取用户选择的排序条件，并调用后端完成按专业分组的成绩排序。
 * 控制台层只负责输入校验和菜单显示，具体的分组及比较规则由 stu.c 处理。
 */
static void sortStudentsFromConsole(void)
{
    int field;
    int courseIndex = -1;
    int direction;

    /* 没有学生时没有可排序的数据，也不能读取第一个学生的课程名称。 */
    if (getStudentCount() == 0)
    {
        printf("当前没有学生信息，无法排序。\n");
        return;
    }

    printf("1. 总成绩\n");
    printf("2. 单个科目成绩\n");
    printf("3. GPA\n");
    /* 菜单编号与 ScoreSortField 的枚举值保持一致。 */
    if (!readInt("请选择排序字段：", &field)
        || (field < SORT_TOTAL_SCORE || field > SORT_GPA))
    {
        printf("排序字段无效。\n");
        return;
    }

    if (field == SORT_COURSE_SCORE)
    {
        /* 所有学生按专业设置相同课程顺序，因此显示第一名学生的课程名称即可。 */
        for (int i = 0; i < COURSE_COUNT; i++)
        {
            printf("%d. %s\n", i + 1, getStudent(0)->courses[i].name);
        }

        if (!readInt("请选择科目：", &courseIndex)
            || courseIndex < 1 || courseIndex > COURSE_COUNT)
        {
            printf("科目无效。\n");
            return;
        }
        /* 菜单中的科目编号从 1 开始，数组下标从 0 开始，需要转换。 */
        courseIndex--;
    }

    printf("1. 升序\n");
    printf("2. 降序\n");
    /* direction 为 2 时传入非 0 值，后端按降序处理；1 则表示升序。 */
    if (!readInt("请选择排序方向：", &direction)
        || (direction != 1 && direction != 2))
    {
        printf("排序方向无效。\n");
        return;
    }

    sortByMajor((ScoreSortField)field, courseIndex, direction == 2);
    printf("已按专业分组完成排序。\n");
    printf("\n========== 排序后的学生名单 ==========\n");
    showAllStudents();
}

/* 打印菜单并根据用户选择调用对应功能。 */
void consoleRunMenu(void)
{
    char line[32];
    int choice;

    do
    {
        printf("\n========== 学生信息管理系统 ==========\n");
        printf("1. 显示全部学生\n");
        printf("2. 添加学生\n");
        printf("3. 按学号查询\n");
        printf("4. 按姓名查询\n");
        printf("5. 修改学生\n");
        printf("6. 删除学生\n");
        printf("7. 成绩排序（按专业分组）\n");
        printf("8. 成绩统计\n");
        printf("9. 保存到文件\n");
        printf("10. 从文件读取\n");
        printf("0. 退出系统\n");
        printf("请选择功能：");

        if (!readLine(line, sizeof(line)) || sscanf(line, "%d", &choice) != 1)
        {
            printf("输入格式错误。\n");
            continue;
        }

        switch (choice)
        {
            case 1:
                showAllStudents();
                break;
            case 2:
                addStudentFromConsole();
                break;
            case 3:
                findStudentByIdFromConsole();
                break;
            case 4:
                findStudentsByNameFromConsole();
                break;
            case 5:
                modifyStudentFromConsole();
                break;
            case 6:
                deleteStudentFromConsole();
                break;
            case 7:
                sortStudentsFromConsole();
                break;
            case 8:
                showStatistics();
                break;
            case 9:
                printf("保存结果：%s\n",
                       saveToFile("Data/students_backend.txt") ? "成功" : "失败");
                break;
            case 10:
                printf("读取结果：%s\n",
                       loadFromFile("Data/students_backend.txt") ? "成功" : "失败");
                break;
            case 0:
                printf("系统已退出。\n");
                break;
            default:
                printf("没有这个功能选项。\n");
                break;
        }
    } while (choice != 0);
}
