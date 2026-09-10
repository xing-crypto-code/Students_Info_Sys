#include "file.h"
#include "stu.h"
#include "read_students_csv.h"
#include "write_students_csv.h"

int saveToFile(const char *filename)
{
    return write_students_csv(filename, getStudentArray(), getStudentCount(), NULL) >= 0;
}

int loadFromFile(const char *filename)
{
    StudentCSV loaded[MAX_STUDENTS];
    char header[MAX_LINE_LEN] = {0};
    int count;

    if(filename == NULL) return 0;
    count = read_students_csv(filename, loaded, MAX_STUDENTS, header, sizeof(header));
    if(count < 0 || !replaceStudents(loaded, count)) return 0;

    recalculate_ranking_by_score(getStudentArray(), getStudentCount());
    return 1;
}
