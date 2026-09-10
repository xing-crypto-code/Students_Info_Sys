#include "console.h"
#include "file.h"
#include "stu.h"

#include <stdio.h>
#include <inttypes.h>
#include <string.h>

#ifdef _WIN32
#include <windows.h>
#endif

void consoleInit(void)
{
#ifdef _WIN32
    SetConsoleOutputCP(CP_UTF8);
    SetConsoleCP(CP_UTF8);
#endif
}

static int readLine(char *buffer, size_t size)
{
    if(fgets(buffer, (int)size, stdin) == NULL) return 0;
    buffer[strcspn(buffer, "\r\n")] = '\0';
    return 1;
}

static int readInt(const char *prompt, int *value)
{
    char line[64];
    printf("%s", prompt);
    if(!readLine(line, sizeof(line)) || sscanf(line, "%d", value) != 1) {
        printf("输入格式错误。\n");
        return 0;
    }
    return 1;
}

static int readUint64(const char *prompt, uint64_t *value)
{
    char line[64];
    uint64_t parsed;

    printf("%s", prompt);
    if(!readLine(line, sizeof(line)) || sscanf(line, "%" SCNu64, &parsed) != 1) {
        printf("输入格式错误。\n");
        return 0;
    }
    *value = (uint64_t)parsed;
    return 1;
}

static int readFloat(const char *prompt, float *value)
{
    char line[64];
    printf("%s", prompt);
    if(!readLine(line, sizeof(line)) || sscanf(line, "%f", value) != 1) {
        printf("输入格式错误。\n");
        return 0;
    }
    return 1;
}

static int readText(const char *prompt, char *text, size_t size)
{
    printf("%s", prompt);
    if(!readLine(text, size)) {
        printf("读取输入失败。\n");
        return 0;
    }
    return 1;
}

static void printStudent(const StudentCSV *student)
{
        printf("学号：%" PRIu64 " | 姓名：%s | 年级：%s | 班级：%s\n",
            student->id, student->name,
           student->grade, student->class_name);
    printf("性别：%s | 分数：%.1f | 绩点：%.1f | 排名：%u\n",
           student->gender ? "男" : "女", student->score,
           student->gpa, (unsigned int)student->rank);
}

static int readStudent(StudentCSV *student, const char *prefix)
{
    char gender[16];

    memset(student, 0, sizeof(*student));
    if(!readUint64("请输入学号：", &student->id) ||
       !readText(prefix, student->name, sizeof(student->name)) ||
       !readText("请输入年级：", student->grade, sizeof(student->grade)) ||
       !readText("请输入班级：", student->class_name, sizeof(student->class_name)) ||
       !readText("请输入性别（男/M 或 女/F）：", gender, sizeof(gender)) ||
       !readFloat("请输入分数：", &student->score) ||
       !readFloat("请输入绩点：", &student->gpa)) return 0;

    if(strcmp(gender, "男") == 0 || strcmp(gender, "M") == 0 || strcmp(gender, "1") == 0) {
        student->gender = true;
    } else if(strcmp(gender, "女") == 0 || strcmp(gender, "F") == 0 || strcmp(gender, "0") == 0) {
        student->gender = false;
    } else {
        printf("性别格式错误。\n");
        return 0;
    }
    return 1;
}

static void showAllStudents(void)
{
    if(getStudentCount() == 0) {
        printf("当前没有学生信息。\n");
        return;
    }
    for(int i = 0; i < getStudentCount(); i++) {
        printf("[%d] ", i + 1);
        printStudent(getStudent(i));
    }
}

static void addStudentFromConsole(void)
{
    StudentCSV student;
    if(readStudent(&student, "请输入姓名：")) {
        printf("添加结果：%s\n", addStudent(&student) ? "成功" : "失败，请检查数据或学号是否重复");
    }
}

static void findStudentByIdFromConsole(void)
{
    uint64_t id;
    if(readUint64("请输入要查询的学号：", &id)) {
        StudentCSV *student = findById(id);
        if(student == NULL) printf("没有找到该学生。\n");
        else printStudent(student);
    }
}

static void findStudentsByNameFromConsole(void)
{
    char name[MAX_FIELD_LEN];
    StudentCSV results[MAX_STUDENTS];
    int count;

    if(!readText("请输入要查询的姓名：", name, sizeof(name))) return;
    count = findByName(name, results, MAX_STUDENTS);
    printf("共找到 %d 名学生。\n", count);
    for(int i = 0; i < count && i < MAX_STUDENTS; i++) printStudent(&results[i]);
}

static void modifyStudentFromConsole(void)
{
    uint64_t id;
    StudentCSV student;

    if(!readUint64("请输入要修改的学号：", &id) || !readStudent(&student, "请输入新姓名：")) return;
    printf("修改结果：%s\n", modifyStudent(id, &student) ? "成功" : "失败");
}

static void deleteStudentFromConsole(void)
{
    uint64_t id;
    if(readUint64("请输入要删除的学号：", &id)) {
        printf("删除结果：%s\n", deleteStudent(id) ? "成功" : "未找到该学生");
    }
}

static void sortStudentsFromConsole(void)
{
    int field;
    int direction;

    if(getStudentCount() == 0) {
        printf("当前没有学生信息，无法排序。\n");
        return;
    }
    printf("0. 年级  1. 班级  2. 学号  3. 姓名\n");
    printf("4. 性别  5. 分数  6. 绩点  7. 排名\n");
    if(!readInt("请选择排序字段：", &field) || field < 0 || field > 7) return;
    if(!readInt("1. 升序  2. 降序，请选择：", &direction) ||
       (direction != 1 && direction != 2)) return;
    sortStudents(field, direction == 1);
    showAllStudents();
}

static void showStatistics(void)
{
    float average;
    float highest;
    float lowest;
    int pass_count;

    if(!getStatistics(&average, &highest, &lowest, &pass_count)) {
        printf("暂无成绩数据。\n");
        return;
    }
    printf("平均分：%.2f，最高分：%.2f，最低分：%.2f，及格人数：%d\n",
           average, highest, lowest, pass_count);
}

void consoleRunMenu(void)
{
    char line[32];
    int choice;

    do {
        printf("\n========== 学生信息管理系统 ==========\n");
        printf("1. 显示全部学生\n2. 添加学生\n3. 按学号查询\n4. 按姓名查询\n");
        printf("5. 修改学生\n6. 删除学生\n7. 排序\n8. 成绩统计\n");
        printf("9. 保存到 CSV\n10. 从 CSV 读取\n0. 退出系统\n请选择功能：");

        if(!readLine(line, sizeof(line)) || sscanf(line, "%d", &choice) != 1) {
            printf("输入格式错误。\n");
            continue;
        }

        switch(choice) {
        case 1: showAllStudents(); break;
        case 2: addStudentFromConsole(); break;
        case 3: findStudentByIdFromConsole(); break;
        case 4: findStudentsByNameFromConsole(); break;
        case 5: modifyStudentFromConsole(); break;
        case 6: deleteStudentFromConsole(); break;
        case 7: sortStudentsFromConsole(); break;
        case 8: showStatistics(); break;
        case 9: printf("保存结果：%s\n", saveToFile("Data/students.csv") ? "成功" : "失败"); break;
        case 10: printf("读取结果：%s\n", loadFromFile("Data/students.csv") ? "成功" : "失败"); break;
        case 0: printf("系统已退出。\n"); break;
        default: printf("没有这个功能选项。\n"); break;
        }
    } while(choice != 0);
}
