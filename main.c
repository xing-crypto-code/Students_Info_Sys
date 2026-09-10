#include "console.h"
#include "file.h"
#include "stu.h"
#include <stdio.h>

int main(void)
{
    consoleInit();
    stuInit();

    /* 启动时自动读取已有学生数据，菜单打开后即可直接查看。 */
    if (!loadFromFile("Data/students.csv"))
    {
        printf("提示：未能读取学生数据文件，将从空数据开始。\n");
    }

    consoleRunMenu();
    return 0;
}