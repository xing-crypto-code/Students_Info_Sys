#include <stdint.h>
#include <SDL.h>
#include "lvgl/lvgl.h"

#include "core/core.h"
#include "src/ui/ui.h"
#include "sdl/sdl_hal.h"

int main(int argc, char** argv)
{
    (void)argc;
    (void)argv;

    /* 1. 初始化 LVGL */
    lv_init();

    /* 让 Windows 中文输入法显示候选词窗口 */
    SDL_SetHint(SDL_HINT_IME_SHOW_UI, "1");

    /* 2. 创建 SDL2 窗口、显示和输入设备 */
    lv_display_t* disp = app_sdl_hal_init(UI_SCR_W, UI_SCR_H);
    if (disp == NULL) {
        return 1;
    }

    /* 3. 设置窗口标题 */
    lv_sdl_window_set_title(disp, "学生成绩信息管理系统");

    lv_font_t* full_font =
        lv_tiny_ttf_create_file("A:fonts/NotoSansSC-Regular.ttf", 16);

    if (full_font != NULL) {
        ui_set_font(full_font);
    }

    /* 4. 创建完整 UI */
    ui_create(lv_screen_active());

    /* 5. 首次运行：bin 目录没有标志文件时自动弹出帮助，并创建标志文件 */
    if (!core_flag_file_exists()) {
        if (ui_show_help()) {
            core_flag_file_create();
        }
    }

    /* 6. 主循环 */
    while (1) {
        uint32_t sleep_time_ms = lv_timer_handler();
        if (sleep_time_ms == LV_NO_TIMER_READY) {
            sleep_time_ms = LV_DEF_REFR_PERIOD;
        }
        SDL_Delay(sleep_time_ms);
    }

    return 0;
}
