#ifndef CONSOLE_H
#define CONSOLE_H

/* 初始化控制台，确保中文输出使用 UTF-8。 */
void consoleInit(void);

/* 启动控制台菜单，返回后程序结束。 */
void consoleRunMenu(void);

#endif
