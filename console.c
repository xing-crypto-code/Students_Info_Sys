#include "console.h"

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
