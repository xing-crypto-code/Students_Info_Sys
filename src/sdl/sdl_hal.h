/**
 * @file sdl_hal.h
 * @brief SDL2 版本的 LVGL 硬件抽象层（HAL）。
 *
 * 该文件只负责创建 SDL2 窗口、显示设备、鼠标/键盘输入设备，
 * 并把它们注册到 LVGL 中，供 ui.c / main.c 使用。
 */

#ifndef SDL_HAL_H
#define SDL_HAL_H

#include "lvgl/lvgl.h"

#ifdef __cplusplus
extern "C" {
#endif

     /**
      * @brief 初始化 SDL2 版本的 LVGL 显示与输入设备。
      *
      * 内部完成：
      *   1. 创建默认 lv_group；
      *   2. 创建指定尺寸的 SDL2 窗口和 lv_display；
      *   3. 创建鼠标、滚轮、键盘输入设备并绑定到默认 group。
      *
      * @param w 窗口宽度（像素）
      * @param h 窗口高度（像素）
      * @return 创建成功的 lv_display_t 指针；失败返回 NULL。
      */
     lv_display_t* app_sdl_hal_init(int32_t w, int32_t h);

#ifdef __cplusplus
}
#endif

#endif /* SDL_HAL_H */
