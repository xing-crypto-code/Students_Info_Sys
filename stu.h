#ifndef STU_H
#define STU_H

#define MAX_STUDENT 100
#define COURSE_COUNT 4
#define MAX_COURSE_NAME 32

typedef struct
{
    char name[MAX_COURSE_NAME];
    float score;              // 课程成绩
    float credit;             // 课程学分
} CourseScore;

typedef struct
{
    int id;                    // 学号
    char name[20];             // 姓名
    char year[20];             // 年级
    char major[40];            // 专业
    char class_number[20];     // 班级
    char gender;               // 性别
    char phone[20];            // 电话

    CourseScore courses[COURSE_COUNT];
    float totalScore;          // 总分
    float averageScore;        // 平均分
    float gpa;                 // GPA
    int majorRank;             // 专业排名
} Student;

/* ===== 学生信息管理 ===== */

/* 初始化学生信息 */
void stuInit(void);

/* 添加学生，成功返回 1，失败返回 0 */
int addStudent(const Student *stu);

/* 根据学号删除学生，成功返回 1，未找到时返回 0 */
int deleteStudent(int id);

/* 获取当前学生数量 */
int getStudentCount(void);

/* 根据学号查找学生，找不到时返回 NULL */
Student *findById(int id);

/*
 * 根据姓名查找学生。
 * 返回实际匹配数量，但最多只向 result 写入 maxCount 个结果。
 */
int findByName(const char *name, Student result[], int maxCount);

/* 根据学号修改学生信息，学号字段保持不变 */
int modifyStudent(int id, const Student *newInfo);



/* 获取指定下标的学生，下标无效时返回 NULL */
Student *getStudent(int index);

/* 根据专业设置对应的四门课程和课程学分 */
int setCoursesByMajor(Student *student);

/* 重新计算总分、平均分、GPA 和专业排名 */
void calculateStudentResults(Student *student);


/* ===== 成绩相关 ===== */

typedef enum
{
    /* 使用总成绩（四门课程成绩之和）作为排序依据。 */
    SORT_TOTAL_SCORE = 1,
    /* 使用某一门课程的成绩作为排序依据，具体课程由 courseIndex 指定。 */
    SORT_COURSE_SCORE,
    /* 使用学生的 GPA 作为排序依据。 */
    SORT_GPA
} ScoreSortField;

/*
 * 按专业分组，并在每个专业内按指定成绩字段排序。
 *
 * field：排序字段，只能使用 ScoreSortField 中定义的值。
 * courseIndex：当 field 为 SORT_COURSE_SCORE 时，表示课程在 courses 数组中的下标；
 *              排序总成绩或 GPA 时应传入 -1。
 * descending：非 0 表示降序，0 表示升序。
 */
void sortByMajor(ScoreSortField field, int courseIndex, int descending);

/* 按总成绩分组排序：descending 为非 0 时降序，为 0 时升序 */
void sortByScore(int descending);

/* 计算成绩等级 */
char calculateGrade(float score);

/*
 * 成绩统计。
 * 成功返回 1，并通过输出参数返回平均分、最高分、最低分和及格人数。
 * 没有学生或输出参数为空时返回 0。
 */
int getStatistics(
    float *average,
    float *highest,
    float *lowest,
    int *passCount
);


/* ===== 文件相关 ===== */

/* 保存数据 */
int saveToFile(const char *filename);

/* 读取数据 */
int loadFromFile(const char *filename);


#endif