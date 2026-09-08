#ifndef STU_H
#define STU_H

#define MAX_STUDENT 100

typedef struct
{
    int id;             // 学号
    char name[20];      // 姓名
    char major[20];     // 专业
    char class_number[20]; // 班级
    float score;         // 成绩
    char grade;         // 成绩等级
    char gender;        // 性别
    char phone[20];     // 电话
    
} Student;

/* ===== 学生信息管理 ===== */

/* 初始化学生信息 */
void stuInit(void);

/* 添加学生 */
int addStudent(const Student *stu);

/* 获取当前学生数量 */
int getStudentCount(void);

/* 根据学号查找学生 */
Student *findById(int id);



/* 获取指定下标的学生 */
Student *getStudent(int index);


/* ===== 成绩相关 ===== */

/* 成绩排序 */
void sortByScore(int descending);

/* 计算成绩等级 */
char calculateGrade(float score);

/* 成绩统计 */
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