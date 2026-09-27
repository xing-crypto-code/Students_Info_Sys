/**
 * @file sdl_hal.c
 * @brief SDL2 版本的 LVGL 硬件抽象层实现。
 *
 * 与 win32_lvgl_port.c 的区别：
 *   - win32_lvgl_port.c 直接使用 Win32/GDI 创建窗口和帧缓冲；
 *   - 本文件使用 LVGL 自带的 SDL2 驱动创建窗口、鼠标、键盘、滚轮输入设备。
 *
 * 使用本文件时不需要 win32_lvgl_port.c/h。
 */

#include "sdl_hal.h"
#include <SDL.h>
#include <string.h>

#define WHEEL_SCROLL_STEP 150
static int s_wheel_watch_added = 0;

/* ======================== 输入法候选窗位置同步 ======================== */

static lv_obj_t* s_ime_last_focused = NULL;
static lv_timer_t* s_ime_rect_timer = NULL;

/* 周期检查当前焦点是否变成文本框；是则把 IME 候选窗定位到文本框位置 */
static void s_ime_rect_timer_cb(lv_timer_t* timer)
{
    (void)timer;

    lv_group_t* group = lv_group_get_default();
    if (group == NULL) return;

    lv_obj_t* focused = lv_group_get_focused(group);
    if (focused == s_ime_last_focused) return;
    s_ime_last_focused = focused;

    if (focused == NULL) return;
    if (!lv_obj_check_type(focused, &lv_textarea_class)) return;

    lv_area_t a;
    lv_obj_get_coords(focused, &a);

    SDL_Rect rect;
    rect.x = a.x1;
    rect.y = a.y1;
    rect.w = a.x2 - a.x1 + 1;
    rect.h = a.y2 - a.y1 + 1;
    SDL_SetTextInputRect(&rect);
}


/* ======================== Ctrl+V 剪贴板粘贴 ======================== */

#define PASTE_BUF_SIZE 4096
#define PASTE_TIMER_MS 20

static char s_paste_buf[PASTE_BUF_SIZE];
static int  s_paste_pending = 0;
static lv_timer_t* s_paste_timer = NULL;
static int s_block_next_text_input = 0;

/* 由 LVGL 定时器在主循环里执行，把剪贴板文本插入当前焦点文本框 */
static void s_paste_timer_cb(lv_timer_t* timer)
{
    (void)timer;

    if (!s_paste_pending) return;
    s_paste_pending = 0;

    lv_group_t* group = lv_group_get_default();
    if (group == NULL) return;

    lv_obj_t* focused = lv_group_get_focused(group);
    if (focused == NULL) return;
    if (!lv_obj_check_type(focused, &lv_textarea_class)) return;

    /* 插入到光标处；单行文本框会自动忽略换行，并受 max_length 限制 */
    lv_textarea_add_text(focused, s_paste_buf);
}

/* SDL 事件过滤器：拦截 Ctrl+V，读取系统剪贴板，并阻止它进入 LVGL 键盘驱动 */
static int sdl_event_filter(void* user_data, SDL_Event* event)
{
    (void)user_data;

    if (event->type == SDL_KEYDOWN &&
        event->key.keysym.sym == SDLK_v &&
        (event->key.keysym.mod & KMOD_CTRL)) {

        char* clip = SDL_GetClipboardText();
        if (clip != NULL) {
            strncpy(s_paste_buf, clip, sizeof(s_paste_buf) - 1);
            s_paste_buf[sizeof(s_paste_buf) - 1] = '\0';
            SDL_free(clip);
            s_paste_pending = 1;
        }

        s_block_next_text_input = 1;
        return 0;   /* 吞掉 Ctrl+V 的 KEYDOWN */
    }

    /* 某些平台在 Ctrl+V 时还会附带发一个 TEXTINPUT，这里一起吞掉 */
    if (event->type == SDL_TEXTINPUT) {
        if (s_block_next_text_input || (SDL_GetModState() & KMOD_CTRL)) {
            s_block_next_text_input = 0;
            return 0;
        }
    }

    /* V 键松开后解除屏蔽，避免影响后面的正常输入 */
    if (event->type == SDL_KEYUP && event->key.keysym.sym == SDLK_v) {
        s_block_next_text_input = 0;
    }

    return 1;   /* 其他事件原样交给 LVGL SDL 驱动 */
}


/* 自定义鼠标滚轮滚动回调：
 * 找到鼠标下面最近的可滚动对象，并让它滚动，
 * 不再让滚轮去切换按钮焦点。 */
static int sdl_wheel_scroll_watch(void* user_data, SDL_Event* event)
{
    (void)user_data;

    if (event->type != SDL_MOUSEWHEEL) return 1;

    int mx = 0;
    int my = 0;

#if SDL_VERSION_ATLEAST(2, 26, 0)
    /* SDL 2.26.0 开始，滚轮事件直接带有鼠标坐标 */
    mx = event->wheel.mouseX;
    my = event->wheel.mouseY;
#else
    SDL_GetMouseState(&mx, &my);
#endif

    lv_point_t p;
    p.x = (lv_coord_t)mx;
    p.y = (lv_coord_t)my;

    /* 查找鼠标下面真正可滚动的对象 */
    lv_obj_t* obj = lv_indev_search_obj(lv_screen_active(), &p);

    while (obj != NULL) {
        /* 先判断有没有垂直滚动方向和 SCROLLABLE 标志 */
        if (lv_obj_has_flag(obj, LV_OBJ_FLAG_SCROLLABLE) &&
            (lv_obj_get_scroll_dir(obj) & LV_DIR_VER)) {

            /* 保证布局是最新的，才能正确计算可滚动范围 */
            lv_obj_update_layout(obj);

            int32_t top = lv_obj_get_scroll_top(obj);
            int32_t bottom = lv_obj_get_scroll_bottom(obj);

            /* 只有真的能上下滚动时才处理，否则继续找父对象 */
            if (top > 0 || bottom > 0) {
                /* 优先使用精确滚轮值，普通鼠标也能兼容 */
                float wheel_delta = event->wheel.preciseY;
                if (wheel_delta == 0.0f) {
                    wheel_delta = (float)event->wheel.y;
                }

                int32_t dy = (int32_t)(wheel_delta * WHEEL_SCROLL_STEP);
                lv_obj_scroll_by_bounded(obj, 0, dy, LV_ANIM_OFF);
                break;
            }
        }

        /* 当前对象不可滚动或没有可滚动内容，继续向上找 */
        obj = lv_obj_get_parent(obj);
    }

    return 1;
}


/**
 * @brief 初始化 SDL2 版本的 LVGL 显示与输入设备。
 *
 * @param w 窗口宽度（本项目为 1080）
 * @param h 窗口高度（本项目为 720）
 * @return lv_display_t 指针；失败返回 NULL。
 */
lv_display_t* app_sdl_hal_init(int32_t w, int32_t h)
{
    /* 创建一个默认键盘分组，所有文本框都加入该分组以接收键盘输入 */
    lv_group_t* group = lv_group_create();
    if (group == NULL) return NULL;
    lv_group_set_default(group);

    /* 创建 SDL2 窗口，并把它注册成 LVGL 显示设备 */
    lv_display_t* disp = lv_sdl_window_create(w, h);
    if (disp == NULL) return NULL;
    lv_display_set_default(disp);
    /* 每 100ms 同步一次输入法候选窗位置 */
    if (s_ime_rect_timer == NULL) {
        s_ime_rect_timer = lv_timer_create(s_ime_rect_timer_cb, 100, NULL);
    }

    /* Ctrl+V：从系统剪贴板粘贴到当前焦点文本框 */
    SDL_SetEventFilter(sdl_event_filter, NULL);
    if (s_paste_timer == NULL) {
        s_paste_timer = lv_timer_create(s_paste_timer_cb, PASTE_TIMER_MS, NULL);
    }

    /* 创建鼠标输入设备，并将其绑定到默认分组 */
    lv_indev_t* mouse = lv_sdl_mouse_create();
    if (mouse != NULL) {
        lv_indev_set_display(mouse, disp);
        lv_indev_set_group(mouse, group);
    }

    /* 创建鼠标滚轮输入设备，但不绑定到键盘分组，
       避免滚轮切换按钮焦点。滚动由自定义 watch 处理。 */
    lv_indev_t* mousewheel = lv_sdl_mousewheel_create();
    if (mousewheel != NULL) {
        lv_indev_set_display(mousewheel, disp);
        lv_indev_set_group(mousewheel, NULL);
    }

    /* 注册自定义滚轮事件监听 */
    if (!s_wheel_watch_added) {
        SDL_AddEventWatch(sdl_wheel_scroll_watch, NULL);
        s_wheel_watch_added = 1;
    }


    /* 创建键盘输入设备，供搜索框、导入/导出输入框、单元格编辑框输入文字 */
    lv_indev_t* keyboard = lv_sdl_keyboard_create();
    if (keyboard != NULL) {
        lv_indev_set_display(keyboard, disp);
        lv_indev_set_group(keyboard, group);
    }

    return disp;
}
