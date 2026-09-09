#include "stu.h"
#include <stddef.h>
#include <string.h>

/* 学生数据仅由本文件管理，其他模块通过 stu.h 中的 API 访问 */
static Student students[MAX_STUDENT];

/* 当前有效学生数量，取值范围为 0 到 MAX_STUDENT */
static int studentCount = 0;

/* 常见 4.0 制分段，最后按课程学分加权平均。 */
static float calculatePoint(float score)
{
    if (score >= 90.0f) return 4.0f;
    if (score >= 85.0f) return 3.7f;
    if (score >= 82.0f) return 3.3f;
    if (score >= 78.0f) return 3.0f;
    if (score >= 75.0f) return 2.7f;
    if (score >= 72.0f) return 2.3f;
    if (score >= 68.0f) return 2.0f;
    if (score >= 64.0f) return 1.5f;
    if (score >= 60.0f) return 1.0f;
    return 0.0f;
}

/* 根据三个专业设置各自的四门专业课，每门课暂定 3 学分。 */
int setCoursesByMajor(Student *student)
{
    static const char *electronic[] = {"电路原理", "模拟电子技术", "数字电子技术", "信号与系统"};
    static const char *communication[] = {"通信原理", "数字信号处理", "电磁场与电磁波", "移动通信"};
    static const char *computer[] = {"C语言程序设计", "数据结构", "计算机组成原理", "操作系统"};
    const char **courseNames = NULL;

    if (student == NULL) return 0;
    if (strcmp(student->major, "电子信息工程") == 0) courseNames = electronic;
    else if (strcmp(student->major, "通信工程") == 0) courseNames = communication;
    else if (strcmp(student->major, "计算机科学与技术") == 0) courseNames = computer;
    else return 0;

    for (int i = 0; i < COURSE_COUNT; i++)
    {
        strcpy(student->courses[i].name, courseNames[i]);
        student->courses[i].credit = 3.0f;
        student->courses[i].score = 0.0f;
    }
    return 1;
}

/* 计算四门课总分、平均分和学分加权 GPA。 */
void calculateStudentResults(Student *student)
{
    float totalCredits = 0.0f;
    float weightedPoints = 0.0f;

    if (student == NULL) return;
    student->totalScore = 0.0f;
    for (int i = 0; i < COURSE_COUNT; i++)
    {
        student->totalScore += student->courses[i].score;
        totalCredits += student->courses[i].credit;
        weightedPoints += calculatePoint(student->courses[i].score)
                          * student->courses[i].credit;
    }
    student->averageScore = student->totalScore / COURSE_COUNT;
    student->gpa = totalCredits == 0.0f ? 0.0f : weightedPoints / totalCredits;
}

/* 检查学生基本信息和四门成绩是否合法。 */
static int isStudentValid(const Student *student)
{
    if (student == NULL || student->id <= 0 || student->name[0] == '\0'
        || student->major[0] == '\0') return 0;

    for (int i = 0; i < COURSE_COUNT; i++)
        if (student->courses[i].score < 0.0f || student->courses[i].score > 100.0f)
            return 0;
    return 1;
}

/* 按平均分为每个专业重新计算并列排名。 */
static void updateMajorRanks(void)
{
    for (int i = 0; i < studentCount; i++)
    {
        int rank = 1;

        for (int j = 0; j < studentCount; j++)
        {
            if (strcmp(students[i].major, students[j].major) == 0
                && students[j].averageScore > students[i].averageScore)
            {
                rank++;
            }
        }

        students[i].majorRank = rank;
    }
}

/* 根据分数计算等级，调用者应先确保分数在 0 到 100 之间 */
char calculateGrade(float score)
{
    if (score >= 90.0f)
    {
        return 'A';
    }

    if (score >= 80.0f)
    {
        return 'B';
    }

    if (score >= 70.0f)
    {
        return 'C';
    }

    if (score >= 60.0f)
    {
        return 'D';
    }

    return 'F';
}


/* 初始化学生信息 */
void stuInit(void)
{
    studentCount = 0;
}

/* 添加学生，成功返回 1，失败返回 0 */
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

    /* 将新学生放入当前有效数据的末尾，并计算综合成绩。 */
    students[studentCount] = *stu;
    calculateStudentResults(&students[studentCount]);
    studentCount++;
    updateMajorRanks();

    return 1;
}

/* 获取当前学生数量 */
int getStudentCount(void)
{
    return studentCount;
}


/* 获取指定下标的学生，下标无效时返回 NULL */
Student *getStudent(int index)
{
    if (index < 0 || index >= studentCount)
    {
        return NULL;
    }

    return &students[index];
}

/* 根据学号查找学生，找不到时返回 NULL */
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

/* 根据姓名查找学生 */
int findByName(const char *name, Student result[], int maxCount)
{
    if (name == NULL || result == NULL || maxCount <= 0)
    {
        return 0;
    }

    int foundCount = 0;

    for (int i = 0; i < studentCount; i++)
    {
        if (strcmp(students[i].name, name) == 0)
        {
            /* 继续统计全部匹配项，但避免写出 result 的容量 */
            if (foundCount < maxCount)
            {
                result[foundCount] = students[i];
            }

            foundCount++;
        }
    }

    return foundCount;
}

/* 根据学号修改学生信息，学号字段保持不变 */
int modifyStudent(int id, const Student *newInfo)
{
    if (newInfo == NULL)
    {
        return 0;
    }

    /* 根据学号查找学生 */
    Student *stu = findById(id);

    if (stu == NULL)
    {
        return 0;
    }

    /* 检查新的学生信息是否合法 */
    if (!isStudentValid(newInfo))
    {
        return 0;
    }

    /*
     * 保留原来的学号，
     * 只修改其他信息
     */
    /* 结构体整体复制前先保存原学号，防止调用者传入不同学号 */
    int oldId = stu->id;

    *stu = *newInfo;

    stu->id = oldId;
    calculateStudentResults(stu);
    updateMajorRanks();

    return 1;
}

/* 根据学号删除学生，成功返回 1，未找到时返回 0 */
int deleteStudent(int id)
{
    int index = -1; // 记录待删除学生在数组中的下标

    for (int i = 0; i < studentCount; i++)
    {
        if (students[i].id == id)
        {
            index = i;
            break;
        }
    }

    if (index == -1)
    {
        return 0;
    }

    /* 将被删除学生后面的元素向前移动，保持数组连续 */
    for (int i = index; i < studentCount - 1; i++)
    {
        students[i] = students[i + 1];
    }

    studentCount--;

    return 1;
}

/*
 * 根据用户选择的排序字段，取得一名学生对应的比较值。
 *
 * 总成绩和 GPA 已经保存在 Student 结构体中，可以直接读取。
 * 单科成绩需要通过 courseIndex 从 courses 数组中取出。
 * 这里统一返回 float，排序函数就不需要分别处理三种成绩类型。
 */
static float getSortValue(const Student *student, ScoreSortField field, int courseIndex)
{
    if (field == SORT_TOTAL_SCORE)
    {
        return student->totalScore;
    }

    if (field == SORT_COURSE_SCORE && courseIndex >= 0 && courseIndex < COURSE_COUNT)
    {
        return student->courses[courseIndex].score;
    }

    return student->gpa;
}

/*
 * 按专业分组，并在每个专业内按指定成绩字段排序。
 *
 * 排序规则有两个层次：
 * 1. 两名学生专业不同时，按专业名称升序排列，使相同专业的学生连续出现；
 * 2. 两名学生专业相同时，才按照总成绩、单科成绩或 GPA 比较。
 *
 * 这样可以保证不同专业不会互相比较成绩，也不会在排序后交错显示。
 * 函数使用冒泡排序，并交换完整的 Student 结构体，避免学生的个人信息
 * 和成绩字段因单独交换某个分数而发生错位。
 */
void sortByMajor(ScoreSortField field, int courseIndex, int descending)
{
    /* 非法字段不会执行排序，防止调用者传入未定义的排序类型。 */
    if (field != SORT_TOTAL_SCORE && field != SORT_COURSE_SCORE && field != SORT_GPA)
    {
        return;
    }

    /* 只有单科排序需要课程下标，且下标必须位于 courses 数组范围内。 */
    if (field == SORT_COURSE_SCORE && (courseIndex < 0 || courseIndex >= COURSE_COUNT))
    {
        return;
    }

    /*
     * 冒泡排序每一轮把当前最大的“应排在后面”的元素向后移动。
     * majorOrder 大于 0 表示前一个专业名称字典序更大，需要交换，
     * 从而先完成专业分组；只有 majorOrder 等于 0 时才比较成绩。
     */
    for (int i = 0; i < studentCount - 1; i++)
    {
        int swapped = 0;

        for (int j = 0; j < studentCount - 1 - i; j++)
        {
            int majorOrder = strcmp(students[j].major, students[j + 1].major);
            int shouldSwap = majorOrder > 0;

            if (majorOrder == 0)
            {
                /* 同专业学生才使用用户选择的成绩字段进行升序或降序比较。 */
                float current = getSortValue(&students[j], field, courseIndex);
                float next = getSortValue(&students[j + 1], field, courseIndex);

                shouldSwap = descending ? current < next : current > next;
            }

            if (shouldSwap)
            {
                /* 交换完整记录，保证学号、姓名、专业和成绩始终属于同一学生。 */
                Student temp = students[j];
                students[j] = students[j + 1];
                students[j + 1] = temp;
                swapped = 1;
            }
        }

        /* 本轮没有发生交换时，说明专业分组和组内成绩都已经有序。 */
        if (!swapped)
        {
            break;
        }
    }

    /* 排序只改变显示顺序，因此重新按照平均分更新每个专业的排名。 */
    updateMajorRanks();
}

/* 按总成绩排序：descending 为非 0 时从高到低。 */
void sortByScore(int descending)
{
    sortByMajor(SORT_TOTAL_SCORE, -1, descending);
}

/* 统计平均分、最高分、最低分和及格人数 */
int getStatistics(float *average,float *highest,float *lowest,int *passCount)
{
    if (studentCount == 0 || average == NULL || highest == NULL|| lowest == NULL || passCount == NULL)
    {
        return 0;
    }

    float totalScore = 0.0f;
    *highest = students[0].averageScore;
    *lowest = students[0].averageScore;
    *passCount = 0;

    for (int i = 0; i < studentCount; i++)
    {
        float score = students[i].averageScore;
        totalScore += score;

        if (score > *highest)
        {
            *highest = score;
        }

        if (score < *lowest)
        {
            *lowest = score;
        }

        if (score >= 60.0f)
        {
            (*passCount)++;
        }
    }

    *average = totalScore / studentCount;

    return 1;
}