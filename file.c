#include "file.h"
#include "stu.h"
#include <stdio.h>
#include <string.h>

#define FILE_FIELD_COUNT 11
#define FILE_LINE_LENGTH 256

/*
 * 文件中每一行学生数据的字段顺序：
 * 学号 | 姓名 | 年级 | 专业 | 班级 | 性别 | 电话 | 四门课程成绩
 *
 * 这里使用 | 作为分隔符，是因为姓名、专业和班级一般不会包含 |，
 * 读取时可以比较容易地把一整行拆成 11 个字段。
 */
int saveToFile(const char *filename)
{
	/* 文件名为空时无法确定要保存到哪里。 */
	if (filename == NULL)
	{
		return 0;
	}

	FILE *file = fopen(filename, "w");
	if (file == NULL)
	{
		return 0;
	}

	/* 第一行写入表头，方便用记事本或 Excel 查看文件内容。 */
	fprintf(file, "id|name|year|major|class_number|gender|phone|score1|score2|score3|score4\n");

	/* 通过公开 API 逐个获取学生，避免直接访问 stu.c 中的私有数组。 */
	for (int i = 0; i < getStudentCount(); i++)
	{
		Student *student = getStudent(i);

		/* 空字符不能直接写入文本文件，用 - 表示“这个字段为空”。 */
		char gender = student == NULL || student->gender == '\0'
			? '-' : student->gender;

		if (student == NULL
			|| fprintf(file,
					   "%d|%s|%s|%s|%s|%c|%s|%.2f|%.2f|%.2f|%.2f\n",
					   student->id,
					   student->name,
					   student->year,
					   student->major,
					   student->class_number,
					   gender,
					   student->phone,
					   student->courses[0].score,
					   student->courses[1].score,
					   student->courses[2].score,
					   student->courses[3].score) < 0)
		{
			/* 写入过程中出错时关闭文件，避免占用文件句柄。 */
			fclose(file);
			return 0;
		}
	}

	/* fclose 返回 0 表示文件成功关闭，也说明保存过程正常结束。 */
	return fclose(file) == 0;
}

/*
 * 将一行文本按 | 拆分成多个字段，并保留空字段。
 *
 * 例如：
 * 1001|张三|||92.00|A|M|
 * 会被拆成多个字段，其中专业、班级和电话可以是空字符串。
 * 函数会直接修改 line，把 | 替换成字符串结束符 '\0'。
 */
static int splitLine(char *line, char *fields[], int maxFields)
{
	int fieldCount = 0;
	char *start = line;

	while (fieldCount < maxFields)
	{
		/* 查找当前字段后面的分隔符。 */
		char *separator = strchr(start, '|');

		if (separator != NULL)
		{
			/* 把分隔符改成 '\0'，当前字段就成为一个独立字符串。 */
			*separator = '\0';
			fields[fieldCount++] = start;
			start = separator + 1;
		}
		else
		{
			/* 最后一个字段后面没有 |，去掉行尾的换行符。 */
			start[strcspn(start, "\r\n")] = '\0';
			fields[fieldCount++] = start;
			break;
		}
	}

	return fieldCount;
}

/*
 * 解析一行学生数据，不直接修改后端数组。
 *
 * 函数只负责把文本转换成 Student 结构体，真正加入系统的工作
 * 由 loadFromFile() 中的 addStudent() 完成。
 */
static int parseStudentLine(char *line, Student *student)
{
	char *fields[FILE_FIELD_COUNT];

	/* 一行必须正好包含 11 个字段，否则认为文件格式错误。 */
	if (splitLine(line, fields, FILE_FIELD_COUNT) != FILE_FIELD_COUNT)
	{
		return 0;
	}

	*student = (Student){0};

	/* 学号、性别和四门成绩必须能够正确读取。 */
	if (sscanf(fields[0], "%d", &student->id) != 1
		|| fields[5][0] == '\0'
		|| sscanf(fields[7], "%f", &student->courses[0].score) != 1
		|| sscanf(fields[8], "%f", &student->courses[1].score) != 1
		|| sscanf(fields[9], "%f", &student->courses[2].score) != 1
		|| sscanf(fields[10], "%f", &student->courses[3].score) != 1)
	{
		return 0;
	}

	/* 复制字符串字段，最多复制数组容量减 1 个字符，保证末尾有 '\0'。 */
	strncpy(student->name, fields[1], sizeof(student->name) - 1);
	strncpy(student->year, fields[2], sizeof(student->year) - 1);
	strncpy(student->major, fields[3], sizeof(student->major) - 1);
	strncpy(student->class_number, fields[4], sizeof(student->class_number) - 1);
	strncpy(student->phone, fields[6], sizeof(student->phone) - 1);

	/* 把保存时使用的 - 占位符还原为空字符，并恢复课程名称和学分。 */
	if (!setCoursesByMajor(student)) return 0;
	student->gender = fields[5][0] == '-' ? '\0' : fields[5][0];

	/* 配置课程会初始化成绩，因此再把文件中的四门成绩写回课程数组。 */
	sscanf(fields[7], "%f", &student->courses[0].score);
	sscanf(fields[8], "%f", &student->courses[1].score);
	sscanf(fields[9], "%f", &student->courses[2].score);
	sscanf(fields[10], "%f", &student->courses[3].score);

	return 1;
}

/*
 * 读取文件，并通过 addStudent 统一执行合法性和重复学号检查。
 *
 * 读取分两步进行：
 * 1. 先把整个文件解析到临时数组 loadedStudents；
 * 2. 全部解析成功后，再清空旧数据并加入后端。
 *
 * 这样如果文件中间出现错误，就不会把当前内存中的学生数据读成一半。
 */
int loadFromFile(const char *filename)
{
	/* 文件名为空时无法读取。 */
	if (filename == NULL)
	{
		return 0;
	}

	FILE *file = fopen(filename, "r");
	if (file == NULL)
	{
		return 0;
	}

	Student loadedStudents[MAX_STUDENT];
	int loadedCount = 0;
	char line[FILE_LINE_LENGTH];

	/* 一次读取一行，直到文件结束。 */
	while (fgets(line, sizeof(line), file) != NULL)
	{
		/* 跳过表头和空行，表头不是学生数据。 */
		if (strncmp(line, "id|name|", 8) == 0
			|| line[0] == '\n' || line[0] == '\r')
		{
			continue;
		}

		/* 超过最大人数，或当前行格式错误时，整个读取操作失败。 */
		if (loadedCount >= MAX_STUDENT || !parseStudentLine(line, &loadedStudents[loadedCount]))
		{
			fclose(file);
			return 0;
		}

		loadedCount++;
	}

	fclose(file);

	/* 文件已经完整解析，接下来才替换内存中的旧学生数据。 */
	stuInit();

	for (int i = 0; i < loadedCount; i++)
	{
		/* addStudent 会再次检查学号、成绩范围和重复学号。 */
		if (!addStudent(&loadedStudents[i]))
		{
			stuInit();
			return 0;
		}
	}

	return 1;
}
