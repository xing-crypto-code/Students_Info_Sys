/**
 * @file ui.c
 * @brief 学生成绩信息管理系统 —— LVGL v9.5 UI 框架层实现。
 *
 * 本文件只负责：
 *   1. 创建 1080x720 的界面框架；
 *   2. 创建顶部工具栏、下方 9 列网格；
 *   3. 处理搜索 / 筛选 / 帮助 / 导入 / 导出 / 删除等按钮的 UI 流程；
 *   4. 处理网格点击高亮、单元格编辑框等交互。
 *
 * 业务数据层函数（data_xxx / ui_help_xxx）只调用不实现，
 * 具体约定见 ui.h 中的外部函数需求说明。
 */

#include "ui.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <inttypes.h>

/* ======================== 私有常量 ======================== */

#define CELL_PAD_X          8          /* 自动列宽时文字左右额外留白 */
#define POPUP_W             500
#define POPUP_H             200
#define PATH_TA_W           400
#define PATH_TA_H           60
#define TOAST_W             520
#define TOAST_H             100
#define TOAST_MS            2000
#define SEARCH_HILIGHT_MS   2000

/* ======================== 私有工具宏 ======================== */

/* 把单元格的行/列编码成 int，再存入 user_data；避免为每个格动态分配内存 */
#define CELL_CODE(row, col) (((int)(row)) * 16 + (int)(col))
#define CELL_ROW(code)      ((code) / 16)
#define CELL_COL(code)      ((code) % 16)

/* ======================== 私有类型/对象句柄 ======================== */

static const char * ui_headers[UI_COL_NUM] = {
    "序号", "年级", "班级", "学号", "姓名", "性别", "分数", "绩点", "排名"
};

static lv_obj_t * top_obj;                  /* 顶部 1080x80 工具栏容器 */
static lv_obj_t * obj_list;                 /* 下方 1080x560 网格容器 */
static lv_obj_t * search_ta;                /* 搜索输入框 */
static lv_obj_t * btn_filter;               /* 筛选按钮 */
static lv_obj_t * btn_help;                 /* 帮助按钮 */
static lv_obj_t * btn_import;               /* 导入按钮 */
static lv_obj_t * btn_export;               /* 导出按钮 */
static lv_obj_t * btn_delete;               /* 删除按钮（开/关状态） */

static lv_group_t * kb_group;               /* 文本输入分组 */

/* 网格：grid_cells[0] 是表头行，grid_cells[1..200] 是数据行 */
static lv_obj_t * grid_cells[UI_MAX_DATA_ROWS + 1][UI_COL_NUM];
static lv_coord_t col_widths[UI_COL_NUM];

/* 单元格选中/编辑状态 */
static int last_sel_row = -1;
static int last_sel_col = -1;
static lv_obj_t * last_sel_cell = NULL;
static lv_obj_t * edit_ta = NULL;           /* 当前单元格编辑框 */
static int edit_grid_row = -1;
static int edit_grid_col = -1;
static int edit_student_index = -1;

/* 删除开关状态 */
static bool delete_enabled = false;
static lv_obj_t * delete_popup = NULL;
static int delete_student_index = -1;

/* 帮助图片状态 */
static lv_obj_t * help_imagebtn = NULL;
static int help_index = 0;
static int help_count = 0;

/* 导入/导出路径输入状态 */
static lv_obj_t * path_ta = NULL;
static int path_action = 0;                 /* 0=无, 1=导入, 2=导出 */

/* 筛选弹层状态 */
static lv_obj_t * filter_list1 = NULL;
static lv_obj_t * filter_list2 = NULL;
static int filter_field = 0;                /* 当前二级列表对应 ui_field_t */
static char filter_opt_buf[64][MAX_FIELD_LEN];

/* 搜索高亮 / 导出提示定时器 */
static lv_timer_t * search_timer = NULL;
static lv_timer_t * toast_timer = NULL;
static lv_obj_t * toast_obj = NULL;

/* 表头排序按钮：年级、班级、性别、分数 */
static lv_obj_t * sort_buttons[4];
static int sort_states[4];   /* 0=未点击，1=当前正序，-1=当前倒序 */

/* 四个排序按钮对应的 StudentCSV 字段 */
static const int sort_fields[4] = {
    UI_FIELD_GRADE, UI_FIELD_CLASS, UI_FIELD_GENDER, UI_FIELD_SCORE
};

/* ======================== 前置声明 ======================== */

static void ui_refresh_grid_internal(void);
static void ui_apply_geometry(void);
static void ui_fill_cell_text(int grid_row, int grid_col, char * buf, size_t buf_size);
static void ui_auto_columns(int display_count);
static void ui_set_cell_bg(lv_obj_t * cell, uint32_t color);
static void ui_reset_all_cells_white(void);
static void ui_close_edit(void);
static void ui_save_and_close_edit(void);
static void ui_open_cell_editor(int grid_row, int grid_col);
static void ui_close_filter_lists(void);
static void ui_show_toast(const char * msg);
static void ui_clear_search_hilight(lv_timer_t * timer);
static bool ui_target_belongs_to_edit(lv_obj_t * target);
static void on_screen_clicked(lv_event_t * e);
static void on_cell_clicked(lv_event_t * e);
static void on_delete_button_clicked(lv_event_t * e);
static void show_delete_popup(int student_index);
static void ui_attach_icon(lv_obj_t * parent, const char * icon_name);
static void ui_create_sort_buttons(lv_obj_t * parent);
static void ui_position_sort_buttons(void);
static void on_sort_clicked(lv_event_t * e);

/* ======================== 小工具函数 ======================== */

static const lv_font_t * ui_font(void)
{
    return &lv_font_source_han_sans_sc_16_cjk;
}

static void ui_obj_clear_default(lv_obj_t * obj)
{
    lv_obj_set_style_border_width(obj, 0, 0);
    lv_obj_set_style_radius(obj, 0, 0);
    lv_obj_set_style_pad_all(obj, 0, 0);
    lv_obj_set_style_bg_color(obj, lv_color_hex(0xFFFFFF), 0);
    lv_obj_set_style_bg_opa(obj, LV_OPA_COVER, 0);
    lv_obj_set_style_text_font(obj, ui_font(), 0);
    lv_obj_set_scrollbar_mode(obj, LV_SCROLLBAR_MODE_AUTO);
}

/**
 * 从单行文本框中取出干净文本（去掉首尾空白和换行）。
 */
static void ui_get_ta_text(lv_obj_t * ta, char * out, size_t out_size)
{
    const char * txt = lv_textarea_get_text(ta);
    size_t i, j, len;

    if(txt == NULL) txt = "";
    len = strlen(txt);

    /* 去掉开头空白 */
    i = 0;
    while(i < len && (txt[i] == ' ' || txt[i] == '\t' || txt[i] == '\r' || txt[i] == '\n')) i++;

    /* 复制到 out */
    j = 0;
    while(i < len && j + 1 < out_size) {
        if(txt[i] == '\r' || txt[i] == '\n') { i++; continue; }
        out[j++] = txt[i++];
    }

    /* 去掉末尾空白 */
    while(j > 0 && (out[j - 1] == ' ' || out[j - 1] == '\t')) j--;
    out[j] = '\0';
}

/**
 * 判断目标对象是否为编辑框自身或编辑框的子对象。
 */
static bool ui_target_belongs_to_edit(lv_obj_t * target)
{
    while(target) {
        if(target == edit_ta) return true;
        target = lv_obj_get_parent(target);
    }
    return false;
}

/**
 * 创建一个顶部功能按钮：固定 60x60。
 * @param icon_name 图标文件名；传 NULL 则使用 text 显示文字。
 * @param text      文字备用标签；传 NULL 且无图标时按钮为空。
 */
static lv_obj_t * ui_make_small_button(lv_obj_t * parent, const char * text,
                                       const char * icon_name,
                                       int x, lv_event_cb_t cb, void * user_data)
{
    lv_obj_t * btn = lv_button_create(parent);
    lv_obj_set_pos(btn, x, UI_TOP_BTN_Y);
    lv_obj_set_size(btn, UI_SMALL_BTN_W, UI_TOP_BTN_H);
    lv_obj_set_style_text_font(btn, ui_font(), 0);
    lv_obj_add_flag(btn, LV_OBJ_FLAG_EVENT_BUBBLE);

    if(icon_name != NULL && icon_name[0] != '\0') {
        ui_attach_icon(btn, icon_name);
    }
    else if(text != NULL) {
        lv_obj_t * label = lv_label_create(btn);
        lv_label_set_text(label, text);
        lv_obj_set_style_text_font(label, ui_font(), 0);
        lv_obj_center(label);
    }

    lv_obj_add_event_cb(btn, cb, LV_EVENT_CLICKED, user_data);
    return btn;
}

/**
 * 在父对象中心添加一个图标图片。
 * @param icon_name ui.h 中定义的图标文件名，例如 "filter.png"
 */
static void ui_attach_icon(lv_obj_t * parent, const char * icon_name)
{
    if(icon_name == NULL || icon_name[0] == '\0') return;

    lv_obj_t * img = lv_image_create(parent);
    char path[512];
    snprintf(path, sizeof(path), "%s%s", UI_ICON_FOLDER, icon_name);
    lv_image_set_src(img, path);
    lv_obj_center(img);
}

/* ======================== 单元格工具 ======================== */

static void ui_set_cell_bg(lv_obj_t * cell, uint32_t color)
{
    lv_obj_set_style_bg_color(cell, lv_color_hex(color), 0);
    lv_obj_set_style_bg_opa(cell, LV_OPA_COVER, 0);
}

static void ui_set_cell_style(lv_obj_t * cell)
{
    lv_obj_set_style_border_width(cell, 1, 0);
    lv_obj_set_style_border_color(cell, lv_color_hex(0xCCCCCC), 0);
    lv_obj_set_style_radius(cell, 0, 0);
    lv_obj_set_style_pad_all(cell, 0, 0);
    lv_obj_set_style_bg_color(cell, lv_color_hex(0xFFFFFF), 0);
    lv_obj_set_style_bg_opa(cell, LV_OPA_COVER, 0);
    lv_obj_set_style_text_color(cell, lv_color_hex(0x222222), 0);
    lv_obj_set_style_text_font(cell, ui_font(), 0);
    lv_obj_set_style_text_align(cell, LV_TEXT_ALIGN_CENTER, 0);

    /* 用上内边距让单行文字在 50px 行高内接近垂直居中 */
    int pad_top = (UI_ROW_H - (int)lv_font_get_line_height(ui_font())) / 2;
    if(pad_top < 0) pad_top = 0;
    lv_obj_set_style_pad_top(cell, pad_top, 0);

    lv_label_set_long_mode(cell, LV_LABEL_LONG_MODE_DOTS);
    lv_obj_add_flag(cell, LV_OBJ_FLAG_CLICKABLE | LV_OBJ_FLAG_EVENT_BUBBLE);
}

/**
 * 创建 201 x 9 个 label 单元格（表头 1 行 + 数据 200 行）。
 */
static void ui_create_cells(lv_obj_t * parent)
{
    for(int r = 0; r <= UI_MAX_DATA_ROWS; r++) {
        for(int c = 0; c < UI_COL_NUM; c++) {
            lv_obj_t * cell = lv_label_create(parent);
            grid_cells[r][c] = cell;
            ui_set_cell_style(cell);
            lv_obj_add_event_cb(cell, on_cell_clicked, LV_EVENT_CLICKED,
                                (void *)(intptr_t)CELL_CODE(r, c));
        }
    }
}

/**
 * 重新摆放所有单元格的位置/尺寸。
 * 没有数据时每列保持 120px，因此总宽 1080px；列宽自适应后仍从左开始排列。
 */
static void ui_apply_geometry(void)
{
    lv_coord_t y = 0;

    for(int r = 0; r <= UI_MAX_DATA_ROWS; r++) {
        lv_coord_t x = 0;
        for(int c = 0; c < UI_COL_NUM; c++) {
            lv_obj_set_pos(grid_cells[r][c], x, y);
            lv_obj_set_size(grid_cells[r][c], col_widths[c], UI_ROW_H);
            x += col_widths[c];
        }
        y += UI_ROW_H;
    }

    /* 表头排序按钮跟随列宽移动 */
    ui_position_sort_buttons();
}

/**
 * 重新摆放四个表头排序按钮的位置。
 * 排序按钮位于对应表头格右侧，高度 40px，上下居中。
 */
static void ui_position_sort_buttons(void)
{
    for(int i = 0; i < 4; i++) {
        if(sort_buttons[i] == NULL) continue;

        int field = sort_fields[i];
        int col = field + 1;

        lv_coord_t x = 0;
        for(int c = 0; c < col; c++) x += col_widths[c];
        x += col_widths[col] - 20 - 2;   /* 贴右留 2px */

        lv_obj_set_pos(sort_buttons[i], x, 5);   /* (50 - 40) / 2 = 5 */
    }
}

/**
 * 点击表头排序按钮：同一按钮第一次正序，第二次倒序，之后循环切换。
 */
static void on_sort_clicked(lv_event_t * e)
{
    int field = (int)(intptr_t)lv_event_get_user_data(e);
    int idx = -1;
    for(int i = 0; i < 4; i++) {
        if(sort_fields[i] == field) {
            idx = i;
            break;
        }
    }
    if(idx < 0) return;

    bool ascending;
    if(sort_states[idx] == 1) {
        ascending = false;
        sort_states[idx] = -1;
    }
    else {
        ascending = true;
        sort_states[idx] = 1;
    }

    data_sort_students(field, ascending);
    ui_refresh_grid_internal();
}

/**
 * 创建表头排序按钮：年级、班级、性别、分数。
 */
static void ui_create_sort_buttons(lv_obj_t * parent)
{
    for(int i = 0; i < 4; i++) {
        sort_buttons[i] = lv_button_create(parent);
        lv_obj_set_size(sort_buttons[i], 20, 40);
        lv_obj_set_style_border_width(sort_buttons[i], 0, 0);
        lv_obj_set_style_radius(sort_buttons[i], 0, 0);
        lv_obj_set_style_bg_opa(sort_buttons[i], LV_OPA_TRANSP, 0);
        lv_obj_add_flag(sort_buttons[i], LV_OBJ_FLAG_CLICKABLE | LV_OBJ_FLAG_EVENT_BUBBLE);

        /* 使用同一个 sort.png 图标 */
        char path[512];
        snprintf(path, sizeof(path), "%s%s", UI_ICON_FOLDER, UI_ICON_SORT);
        lv_obj_t * img = lv_image_create(sort_buttons[i]);
        lv_image_set_src(img, path);
        lv_obj_center(img);

        lv_obj_add_event_cb(sort_buttons[i], on_sort_clicked, LV_EVENT_CLICKED,
                            (void *)(intptr_t)sort_fields[i]);
    }
}

/**
 * 读取一个单元格应显示的文本。
 * @param grid_row 0=表头行；1..200=数据行。
 * @param grid_col 0=序号列；1..8=数据列。
 */
static void ui_fill_cell_text(int grid_row, int grid_col, char * buf, size_t buf_size)
{
    if(buf_size == 0) return;
    buf[0] = '\0';

    /* 表头行 */
    if(grid_row == 0) {
        if(grid_col >= 0 && grid_col < UI_COL_NUM) {
            snprintf(buf, buf_size, "%s", ui_headers[grid_col]);
        }
        return;
    }

    /* 第 1 列：显示行号，从 1 开始 */
    if(grid_col == 0) {
        snprintf(buf, buf_size, "%d", grid_row);
        return;
    }

    /* 第 2~9 列：从数据层取学生 */
    int display_row = grid_row - 1;
    int idx = data_get_display_index(display_row);
    if(idx < 0) {
        buf[0] = '\0';
        return;
    }

    StudentCSV * s = data_get_all();
    if(s == NULL) {
        buf[0] = '\0';
        return;
    }
    s = &s[idx];

    switch(grid_col - 1) {
    case UI_FIELD_GRADE:
        snprintf(buf, buf_size, "%s", s->grade);
        break;
    case UI_FIELD_CLASS:
        snprintf(buf, buf_size, "%s", s->class_name);
        break;
    case UI_FIELD_ID:
        snprintf(buf, buf_size, "%" PRIu64, s->id);
        break;
    case UI_FIELD_NAME:
        snprintf(buf, buf_size, "%s", s->name);
        break;
    case UI_FIELD_GENDER:
        snprintf(buf, buf_size, "%s", s->gender ? "男" : "女");
        break;
    case UI_FIELD_SCORE:
        snprintf(buf, buf_size, "%.1f", s->score);
        break;
    case UI_FIELD_GPA:
        snprintf(buf, buf_size, "%.1f", s->gpa);
        break;
    case UI_FIELD_RANK:
        snprintf(buf, buf_size, "%" PRIu16, s->rank);
        break;
    default:
        buf[0] = '\0';
        break;
    }
}

/**
 * 根据单元格最长文本自动调整列宽；最小列宽 120px。
 */
static void ui_auto_columns(int display_count)
{
    char buf[128];

    for(int c = 0; c < UI_COL_NUM; c++) {
        int max_w = UI_DEFAULT_COL_W;

        /* 表头 + 当前可见数据行 */
        for(int r = 0; r <= display_count; r++) {
            ui_fill_cell_text(r, c, buf, sizeof(buf));
            if(buf[0] == '\0') continue;

            lv_point_t size;
            lv_text_get_size(&size, buf, ui_font(), 0, 0,
                             LV_COORD_MAX, LV_TEXT_FLAG_NONE);
            int w = (int)size.x + CELL_PAD_X * 2;
            if(w > max_w) max_w = w;
        }
        col_widths[c] = (lv_coord_t)max_w;
    }
}

/**
 * 把所有可见/隐藏单元格背景统一恢复成白色（表头也可保持浅灰？这里统一白色）。
 */
static void ui_reset_all_cells_white(void)
{
    for(int r = 0; r <= UI_MAX_DATA_ROWS; r++) {
        for(int c = 0; c < UI_COL_NUM; c++) {
            ui_set_cell_bg(grid_cells[r][c], 0xFFFFFF);
        }
    }
}

/**
 * 核心刷新函数：
 *   1. 从数据层获取当前应显示的数据行数；
 *   2. 重算列宽、摆放单元格；
 *   3. 填充文本，隐藏超出范围的行；
 *   4. 清除旧的选中/搜索高亮。
 */
static void ui_refresh_grid_internal(void)
{
    int display_count = data_get_display_count();
    if(display_count < 0) display_count = 0;
    if(display_count > UI_MAX_DATA_ROWS) display_count = UI_MAX_DATA_ROWS;

    /* 先重算列宽并摆放位置 */
    for(int c = 0; c < UI_COL_NUM; c++) col_widths[c] = UI_DEFAULT_COL_W;
    ui_auto_columns(display_count);
    ui_apply_geometry();

    char buf[128];

    for(int r = 0; r <= UI_MAX_DATA_ROWS; r++) {
        bool is_data_row_hidden = (r > display_count);

        for(int c = 0; c < UI_COL_NUM; c++) {
            ui_fill_cell_text(r, c, buf, sizeof(buf));
            lv_label_set_text(grid_cells[r][c], buf);

            if(is_data_row_hidden) {
                lv_obj_add_flag(grid_cells[r][c], LV_OBJ_FLAG_HIDDEN);
            }
            else {
                lv_obj_clear_flag(grid_cells[r][c], LV_OBJ_FLAG_HIDDEN);
            }
        }
    }

    /* 刷新后不保留临时高亮/选中状态 */
    last_sel_row = last_sel_col = -1;
    last_sel_cell = NULL;
    ui_reset_all_cells_white();
    lv_obj_invalidate(obj_list);
}

/* ======================== 顶部按钮回调 ======================== */

/**
 * 搜索框回车：调用外部 data_find_matches() 取得匹配行，整行黄色高亮 2 秒。
 */
static void on_search_ready(lv_event_t * e)
{
    (void)e;
    char keyword[256];

    ui_save_and_close_edit();
    ui_get_ta_text(search_ta, keyword, sizeof(keyword));

    /* 清掉上一次高亮定时器 */
    if(search_timer) {
        lv_timer_delete(search_timer);
        search_timer = NULL;
    }

    int matches[UI_MAX_DATA_ROWS];
    int match_count = data_find_matches(keyword, matches, UI_MAX_DATA_ROWS);
    if(match_count > UI_MAX_DATA_ROWS) match_count = UI_MAX_DATA_ROWS;

    ui_refresh_grid_internal();

    for(int i = 0; i < match_count; i++) {
        int display_row = matches[i];
        if(display_row < 0 || display_row >= UI_MAX_DATA_ROWS) continue;
        int grid_row = display_row + 1;
        for(int c = 0; c < UI_COL_NUM; c++) {
            ui_set_cell_bg(grid_cells[grid_row][c], 0xFFFF00);
        }
    }

    if(match_count > 0) {
        search_timer = lv_timer_create(ui_clear_search_hilight,
                                       SEARCH_HILIGHT_MS, NULL);
        if(search_timer) lv_timer_set_repeat_count(search_timer, 1);
    }
}

/**
 * 搜索高亮时间到：恢复白色网格。
 */
static void ui_clear_search_hilight(lv_timer_t * timer)
{
    (void)timer;
    search_timer = NULL;
    if(obj_list) ui_refresh_grid_internal();
}

/* ---------- 筛选 ---------- */

static void ui_close_filter_lists(void)
{
    if(filter_list2) {
        lv_obj_delete(filter_list2);
        filter_list2 = NULL;
    }
    if(filter_list1) {
        lv_obj_delete(filter_list1);
        filter_list1 = NULL;
    }
}

static void on_filter_option_clicked(lv_event_t * e)
{
    int code = (int)(intptr_t)lv_event_get_user_data(e);
    int field = code / 64;
    int index = code % 64;

    ui_save_and_close_edit();
    if(field >= 0 && field < UI_FIELD_COUNT &&
       index >= 0 && index < 64 && filter_opt_buf[index][0] != '\0') {
        data_apply_filter(field, filter_opt_buf[index]);
    }
    ui_close_filter_lists();
    ui_refresh_grid_internal();
}

/**
 * 第一级筛选列表：年级/班级/性别/分数/全部。
 * “全部”直接清除筛选；其它打开右侧二级列表。
 */
static void on_filter_kind_clicked(lv_event_t * e)
{
    int kind = (int)(intptr_t)lv_event_get_user_data(e);

    ui_save_and_close_edit();

    if(kind == UI_FILTER_ALL) {
        data_clear_filter();
        ui_close_filter_lists();
        ui_refresh_grid_internal();
        return;
    }

    /* 换算成 StudentCSV 字段号 */
    switch(kind) {
    case UI_FILTER_GRADE:  filter_field = UI_FIELD_GRADE;  break;
    case UI_FILTER_CLASS:  filter_field = UI_FIELD_CLASS;  break;
    case UI_FILTER_GENDER: filter_field = UI_FIELD_GENDER; break;
    case UI_FILTER_SCORE:  filter_field = UI_FIELD_SCORE;  break;
    default: return;
    }

    int n = data_get_filter_options(filter_field, filter_opt_buf, 64);
    if(n > 64) n = 64;

    /* 关闭旧的二级列表 */
    if(filter_list2) {
        lv_obj_delete(filter_list2);
        filter_list2 = NULL;
    }

    if(n <= 0) return;

    /* 二级列表放在一级列表右侧 */
    lv_obj_t * parent = lv_screen_active();
    filter_list2 = lv_list_create(parent);
    lv_obj_set_pos(filter_list2, 900, 100);
    lv_obj_set_size(filter_list2, 170, 300);
    lv_obj_set_style_bg_color(filter_list2, lv_color_hex(0xFFFFFF), 0);
    lv_obj_set_style_border_width(filter_list2, 1, 0);
    lv_obj_set_style_border_color(filter_list2, lv_color_hex(0x888888), 0);
    lv_obj_set_style_text_font(filter_list2, ui_font(), 0);
    lv_obj_add_flag(filter_list2, LV_OBJ_FLAG_EVENT_BUBBLE);
    lv_obj_move_foreground(filter_list2);

    for(int i = 0; i < n; i++) {
        lv_obj_t * btn = lv_list_add_button(filter_list2, NULL, filter_opt_buf[i]);
        if(btn) {
            lv_obj_add_event_cb(btn, on_filter_option_clicked, LV_EVENT_CLICKED,
                                (void *)(intptr_t)(filter_field * 64 + i));
            lv_obj_set_style_text_font(btn, ui_font(), 0);
            lv_obj_add_flag(btn, LV_OBJ_FLAG_EVENT_BUBBLE);
        }
    }
}

static void on_filter_clicked(lv_event_t * e)
{
    (void)e;
    ui_save_and_close_edit();

    /* 若已打开则先关闭，实现“再点一次收起” */
    if(filter_list1) {
        ui_close_filter_lists();
        return;
    }

    lv_obj_t * parent = lv_screen_active();
    filter_list1 = lv_list_create(parent);
    lv_obj_set_pos(filter_list1, 700, 100);
    lv_obj_set_size(filter_list1, 170, 300);
    lv_obj_set_style_bg_color(filter_list1, lv_color_hex(0xFFFFFF), 0);
    lv_obj_set_style_border_width(filter_list1, 1, 0);
    lv_obj_set_style_border_color(filter_list1, lv_color_hex(0x888888), 0);
    lv_obj_set_style_text_font(filter_list1, ui_font(), 0);
    lv_obj_add_flag(filter_list1, LV_OBJ_FLAG_EVENT_BUBBLE);
    lv_obj_move_foreground(filter_list1);

    const char * texts[] = { "年级", "班级", "性别", "分数", "全部" };
    for(int i = 0; i < 5; i++) {
        lv_obj_t * btn = lv_list_add_button(filter_list1, NULL, texts[i]);
        if(btn) {
            lv_obj_add_event_cb(btn, on_filter_kind_clicked, LV_EVENT_CLICKED,
                                (void *)(intptr_t)i);
            lv_obj_set_style_text_font(btn, ui_font(), 0);
            lv_obj_add_flag(btn, LV_OBJ_FLAG_EVENT_BUBBLE);
        }
    }
}

/* ---------- 帮助 ---------- */

static void ui_set_help_image(int index)
{
    if(help_imagebtn == NULL) return;
    const void * src = ui_help_get_image_src(index);
    if(src == NULL) return;

    lv_imagebutton_set_src(help_imagebtn, LV_IMAGEBUTTON_STATE_RELEASED, NULL, src, NULL);
    lv_imagebutton_set_src(help_imagebtn, LV_IMAGEBUTTON_STATE_PRESSED, NULL, src, NULL);
}

static void on_help_image_clicked(lv_event_t * e)
{
    (void)e;
    if(help_imagebtn == NULL) return;

    help_index++;
    if(help_index >= help_count) {
        /* 已是最后一张，点击后删除帮助图片按钮 */
        lv_obj_delete(help_imagebtn);
        help_imagebtn = NULL;
        help_index = 0;
        help_count = 0;
    }
    else {
        ui_set_help_image(help_index);
    }
}

static void on_help_clicked(lv_event_t * e)
{
    (void)e;
    ui_save_and_close_edit();

    if(help_imagebtn != NULL) return;   /* 已存在则忽略 */

    help_count = ui_help_get_image_count();
    if(help_count <= 0) return;

    help_index = 0;
    help_imagebtn = lv_imagebutton_create(lv_screen_active());
    lv_obj_set_pos(help_imagebtn, 0, 0);
    lv_obj_set_size(help_imagebtn, 1080, 80);
    lv_obj_add_flag(help_imagebtn, LV_OBJ_FLAG_EVENT_BUBBLE);
    lv_obj_move_foreground(help_imagebtn);
    lv_obj_add_event_cb(help_imagebtn, on_help_image_clicked, LV_EVENT_CLICKED, NULL);
    ui_set_help_image(0);
}

/* ---------- 导入 / 导出 ---------- */

static void on_path_ready(lv_event_t * e)
{
    (void)e;
    if(path_ta == NULL) return;

    char path[1024];
    int action = path_action;

    ui_get_ta_text(path_ta, path, sizeof(path));

    lv_obj_delete(path_ta);
    path_ta = NULL;
    path_action = 0;

    if(action == 1) {
        int r = data_import_csv(path);
        ui_refresh_grid_internal();
        if(r >= 0) {
            char msg[256];
            snprintf(msg, sizeof(msg), "导入成功：%d 条数据", r);
            ui_show_toast(msg);
        }
        else {
            ui_show_toast("导入失败，请检查文件路径");
        }
    }
    else if(action == 2) {
        int r = data_export_csv(path);
        if(r >= 0) {
            char msg[1200];
            snprintf(msg, sizeof(msg), "文件已导出至'%s'", path);
            ui_show_toast(msg);
        }
        else {
            ui_show_toast("导出失败，请检查保存路径");
        }
    }
}

/**
 * 创建与按钮中心对齐的路径输入框。
 * @param center_x 按钮中心 X 坐标
 * @param action   1=导入, 2=导出
 */
static void ui_open_path_ta(int center_x, int action)
{
    ui_save_and_close_edit();

    /* 同一时间只保留一个路径输入框 */
    if(path_ta) {
        lv_obj_delete(path_ta);
        path_ta = NULL;
    }

    path_action = action;
    path_ta = lv_textarea_create(lv_screen_active());
    lv_obj_set_pos(path_ta, center_x - PATH_TA_W / 2, (UI_TOP_H - PATH_TA_H) / 2);
    lv_obj_set_size(path_ta, PATH_TA_W, PATH_TA_H);
    lv_textarea_set_one_line(path_ta, true);
    lv_textarea_set_max_length(path_ta, 1023);
    lv_textarea_set_placeholder_text(path_ta, action == 1 ? "输入 CSV 文件路径，回车导入" : "输入导出文件路径，回车导出");
    lv_textarea_set_cursor_click_pos(path_ta, true);
    lv_obj_set_style_text_font(path_ta, ui_font(), 0);
    lv_obj_set_style_text_font(path_ta, ui_font(), LV_PART_TEXTAREA_PLACEHOLDER);
    lv_obj_add_flag(path_ta, LV_OBJ_FLAG_EVENT_BUBBLE);
    lv_obj_move_foreground(path_ta);
    lv_obj_add_event_cb(path_ta, on_path_ready, LV_EVENT_READY, NULL);

    if(kb_group) {
        lv_group_add_obj(kb_group, path_ta);
        lv_group_focus_obj(path_ta);
        lv_group_set_editing(kb_group, true);
    }
}

static void on_import_clicked(lv_event_t * e)
{
    (void)e;
    /* 导入按钮中心 X = 880 + 30 = 910 */
    ui_open_path_ta(910, 1);
}

static void on_export_clicked(lv_event_t * e)
{
    (void)e;
    /* 导出按钮中心 X = 950 + 30 = 980 */
    ui_open_path_ta(980, 2);
}

/* ---------- 删除 ---------- */

static void on_delete_button_clicked(lv_event_t * e)
{
    (void)e;
    ui_save_and_close_edit();
    delete_enabled = !delete_enabled;

    if(delete_enabled) {
        /* 开启状态：红色底表示当前点击网格会进入删除确认 */
        lv_obj_set_style_bg_color(btn_delete, lv_color_hex(0xFF5555), 0);
    }
    else {
        lv_obj_set_style_bg_color(btn_delete, lv_color_hex(0xFFFFFF), 0);
    }
}

static void on_delete_cancel(lv_event_t * e)
{
    (void)e;
    if(delete_popup) {
        lv_obj_delete(delete_popup);
        delete_popup = NULL;
    }
}

static void on_delete_confirm(lv_event_t * e)
{
    (void)e;
    if(delete_student_index >= 0) {
        data_delete_student(delete_student_index);
        ui_refresh_grid_internal();
    }
    if(delete_popup) {
        lv_obj_delete(delete_popup);
        delete_popup = NULL;
    }
}

/**
 * 显示居中的删除确认弹窗。
 */
static void show_delete_popup(int student_index)
{
    if(delete_popup) {
        lv_obj_delete(delete_popup);
        delete_popup = NULL;
    }

    StudentCSV * all = data_get_all();
    const char * who = "";
    static char who_buf[256];
    if(all && student_index >= 0) {
        snprintf(who_buf, sizeof(who_buf), "%s(%s)", all[student_index].name,
                 all[student_index].grade);
        who = who_buf;
    }

    delete_student_index = student_index;
    delete_popup = lv_obj_create(lv_screen_active());
    lv_obj_set_size(delete_popup, POPUP_W, POPUP_H);
    lv_obj_center(delete_popup);
    lv_obj_set_style_bg_color(delete_popup, lv_color_hex(0xFFFFFF), 0);
    lv_obj_set_style_bg_opa(delete_popup, LV_OPA_COVER, 0);
    lv_obj_set_style_border_width(delete_popup, 2, 0);
    lv_obj_set_style_border_color(delete_popup, lv_color_hex(0x666666), 0);
    lv_obj_set_style_radius(delete_popup, 8, 0);
    lv_obj_add_flag(delete_popup, LV_OBJ_FLAG_EVENT_BUBBLE);
    lv_obj_move_foreground(delete_popup);

    /* 上方 100px：提示文字 */
    lv_obj_t * tip = lv_label_create(delete_popup);
    char tip_buf[512];
    snprintf(tip_buf, sizeof(tip_buf), "你确定要删除'%s'的所有信息吗？", who);
    lv_label_set_text(tip, tip_buf);
    lv_obj_set_pos(tip, 10, 20);
    lv_obj_set_size(tip, POPUP_W - 20, 60);
    lv_obj_set_style_text_align(tip, LV_TEXT_ALIGN_CENTER, 0);
    lv_obj_set_style_text_font(tip, ui_font(), 0);

    /* 下方左右对称：取消(绿) 与 确定(红) */
    lv_obj_t * cancel = lv_button_create(delete_popup);
    lv_obj_set_pos(cancel, 150, 130);
    lv_obj_set_size(cancel, 80, 50);
    lv_obj_set_style_bg_color(cancel, lv_color_hex(0x00C853), 0);
    lv_obj_set_style_bg_opa(cancel, LV_OPA_COVER, 0);
    lv_obj_set_style_text_color(cancel, lv_color_hex(0xFFFFFF), 0);
    lv_obj_set_style_text_font(cancel, ui_font(), 0);
    lv_obj_t * cancel_l = lv_label_create(cancel);
    lv_label_set_text(cancel_l, "取消");
    lv_obj_set_style_text_font(cancel_l, ui_font(), 0);
    lv_obj_center(cancel_l);
    lv_obj_add_event_cb(cancel, on_delete_cancel, LV_EVENT_CLICKED, NULL);
    lv_obj_add_flag(cancel, LV_OBJ_FLAG_EVENT_BUBBLE);

    lv_obj_t * confirm = lv_button_create(delete_popup);
    lv_obj_set_pos(confirm, 270, 130);
    lv_obj_set_size(confirm, 80, 50);
    lv_obj_set_style_bg_color(confirm, lv_color_hex(0xE53935), 0);
    lv_obj_set_style_bg_opa(confirm, LV_OPA_COVER, 0);
    lv_obj_set_style_text_color(confirm, lv_color_hex(0xFFFFFF), 0);
    lv_obj_set_style_text_font(confirm, ui_font(), 0);
    lv_obj_t * confirm_l = lv_label_create(confirm);
    lv_label_set_text(confirm_l, "确定");
    lv_obj_set_style_text_font(confirm_l, ui_font(), 0);
    lv_obj_center(confirm_l);
    lv_obj_add_event_cb(confirm, on_delete_confirm, LV_EVENT_CLICKED, NULL);
    lv_obj_add_flag(confirm, LV_OBJ_FLAG_EVENT_BUBBLE);
}

/* ---------- Toast 弹窗 ---------- */

static void on_toast_clicked(lv_event_t * e)
{
    (void)e;
    if(toast_timer) {
        lv_timer_delete(toast_timer);
        toast_timer = NULL;
    }
    if(toast_obj) {
        lv_obj_delete(toast_obj);
        toast_obj = NULL;
    }
}

static void on_toast_timer(lv_timer_t * timer)
{
    (void)timer;
    toast_timer = NULL;
    if(toast_obj) {
        lv_obj_delete(toast_obj);
        toast_obj = NULL;
    }
}

static void ui_show_toast(const char * msg)
{
    if(toast_obj) {
        lv_obj_delete(toast_obj);
        toast_obj = NULL;
    }
    if(toast_timer) {
        lv_timer_delete(toast_timer);
        toast_timer = NULL;
    }

    toast_obj = lv_label_create(lv_screen_active());
    lv_label_set_text(toast_obj, msg);
    lv_obj_set_size(toast_obj, TOAST_W, TOAST_H);
    lv_obj_center(toast_obj);
    lv_obj_set_style_bg_color(toast_obj, lv_color_hex(0xFFFFFF), 0);
    lv_obj_set_style_bg_opa(toast_obj, LV_OPA_COVER, 0);
    lv_obj_set_style_border_width(toast_obj, 2, 0);
    lv_obj_set_style_border_color(toast_obj, lv_color_hex(0x888888), 0);
    lv_obj_set_style_radius(toast_obj, 8, 0);
    lv_obj_set_style_text_color(toast_obj, lv_color_hex(0x222222), 0);
    lv_obj_set_style_text_font(toast_obj, ui_font(), 0);
    lv_obj_set_style_text_align(toast_obj, LV_TEXT_ALIGN_CENTER, 0);
    lv_obj_add_flag(toast_obj, LV_OBJ_FLAG_CLICKABLE | LV_OBJ_FLAG_EVENT_BUBBLE);
    lv_obj_move_foreground(toast_obj);
    lv_obj_add_event_cb(toast_obj, on_toast_clicked, LV_EVENT_CLICKED, NULL);

    toast_timer = lv_timer_create(on_toast_timer, TOAST_MS, NULL);
    if(toast_timer) lv_timer_set_repeat_count(toast_timer, 1);
}

/* ======================== 单元格点击 / 编辑 ======================== */

static void ui_close_edit(void)
{
    if(edit_ta) {
        lv_obj_delete(edit_ta);
        edit_ta = NULL;
    }
    edit_grid_row = edit_grid_col = -1;
    edit_student_index = -1;
}

static void ui_save_and_close_edit(void)
{
    if(edit_ta == NULL) return;

    char text[256];
    int idx = edit_student_index;
    int field = edit_grid_col - 1;   /* 第1列序号不可编辑，能进入编辑则 col>=1 */

    ui_get_ta_text(edit_ta, text, sizeof(text));
    ui_close_edit();

    if(idx >= 0 && field >= 0 && field < UI_FIELD_COUNT) {
        data_update_student_field(idx, field, text);
    }

    if(obj_list) ui_refresh_grid_internal();
}

static void on_edit_ready(lv_event_t * e)
{
    (void)e;
    ui_save_and_close_edit();
}

static void ui_open_cell_editor(int grid_row, int grid_col)
{
    /* 只能编辑数据行、数据列（col>=1） */
    if(grid_row <= 0 || grid_col <= 0) return;

    int idx = data_get_display_index(grid_row - 1);
    if(idx < 0) return;

    char current[256];
    ui_fill_cell_text(grid_row, grid_col, current, sizeof(current));

    edit_grid_row = grid_row;
    edit_grid_col = grid_col;
    edit_student_index = idx;

    edit_ta = lv_textarea_create(obj_list);
    lv_obj_t * base_cell = grid_cells[grid_row][grid_col];
    lv_obj_set_pos(edit_ta, lv_obj_get_x(base_cell), lv_obj_get_y(base_cell));
    lv_obj_set_size(edit_ta, col_widths[grid_col], UI_ROW_H);
    lv_textarea_set_one_line(edit_ta, true);
    lv_textarea_set_max_length(edit_ta, 255);
    lv_textarea_set_text(edit_ta, current);
    lv_textarea_set_cursor_click_pos(edit_ta, true);
    lv_obj_set_style_text_font(edit_ta, ui_font(), 0);
    lv_obj_set_style_text_align(edit_ta, LV_TEXT_ALIGN_CENTER, 0);
    lv_obj_add_flag(edit_ta, LV_OBJ_FLAG_EVENT_BUBBLE);
    lv_obj_move_foreground(edit_ta);
    lv_obj_add_event_cb(edit_ta, on_edit_ready, LV_EVENT_READY, NULL);

    if(kb_group) {
        lv_group_add_obj(kb_group, edit_ta);
        lv_group_focus_obj(edit_ta);
        lv_group_set_editing(kb_group, true);
    }
}

/**
 * 单击网格单元格：
 *  - 删除模式开启时，弹删除确认框；
 *  - 否则第一次点击高亮行/列；再次点击同一可编辑单元格则打开编辑框。
 */
static void on_cell_clicked(lv_event_t * e)
{
    int code = (int)(intptr_t)lv_event_get_user_data(e);
    int row = CELL_ROW(code);
    int col = CELL_COL(code);
    lv_obj_t * cell = lv_event_get_target(e);

    /* 如果有编辑框且点的是其它位置，先保存并关闭编辑框 */
    if(edit_ta && cell != edit_ta) {
        ui_save_and_close_edit();
        /* 保存后会 refresh，旧 cell 可能已被重建，但 cell 是同一个 label，不影响 */
    }

    /* 删除模式：只对数据行生效 */
    if(delete_enabled && row > 0) {
        int idx = data_get_display_index(row - 1);
        if(idx >= 0) show_delete_popup(idx);
        return;
    }

    /* 再次点击同一个单元格：若可编辑则打开编辑框 */
    if(last_sel_row == row && last_sel_col == col && last_sel_cell == cell &&
       row > 0 && col > 0) {
        ui_open_cell_editor(row, col);
        return;
    }

    /* 第一次点击：整行整列高亮 */
    last_sel_row = row;
    last_sel_col = col;
    last_sel_cell = cell;

    for(int r = 0; r <= UI_MAX_DATA_ROWS; r++) {
        for(int c = 0; c < UI_COL_NUM; c++) {
            if(r == row || c == col) {
                ui_set_cell_bg(grid_cells[r][c], 0x00C853);   /* 绿色 */
            }
            else {
                ui_set_cell_bg(grid_cells[r][c], 0xFFFFFF);   /* 白色 */
            }
        }
    }
    ui_set_cell_bg(cell, 0xFFFF00);                            /* 被点击格黄色 */
}

/* ======================== 屏幕全局点击 ======================== */

static void on_screen_clicked(lv_event_t * e)
{
    lv_obj_t * target = lv_event_get_target(e);

    /* 点编辑框本身/子控件不关闭；点编辑框以外区域保存并关闭 */
    if(edit_ta && !ui_target_belongs_to_edit(target)) {
        ui_save_and_close_edit();
    }
}

/* ======================== UI 创建 ======================== */

static void ui_create_top_bar(lv_obj_t * scr)
{
    /* 顶部工具栏容器 */
    top_obj = lv_obj_create(scr);
    lv_obj_set_pos(top_obj, 0, 0);
    lv_obj_set_size(top_obj, 1080, 80);
    ui_obj_clear_default(top_obj);
    lv_obj_clear_flag(top_obj, LV_OBJ_FLAG_SCROLLABLE);
    lv_obj_add_flag(top_obj, LV_OBJ_FLAG_CLICKABLE | LV_OBJ_FLAG_EVENT_BUBBLE);

    /* 搜索输入框（尺寸 300x60） */
    search_ta = lv_textarea_create(top_obj);
    lv_obj_set_pos(search_ta, 0, UI_TOP_BTN_Y);
    lv_obj_set_size(search_ta, UI_SEARCH_W, UI_TOP_BTN_H);
    lv_textarea_set_one_line(search_ta, true);
    lv_textarea_set_max_length(search_ta, 255);
    lv_textarea_set_placeholder_text(search_ta, "搜索：输入关键字后回车");
    lv_textarea_set_cursor_click_pos(search_ta, true);
    lv_obj_set_style_text_font(search_ta, ui_font(), 0);
    lv_obj_set_style_text_font(search_ta, ui_font(), LV_PART_TEXTAREA_PLACEHOLDER);
    lv_obj_set_style_pad_left(search_ta, 48, 0);   /* 给左侧搜索图标留出空间 */
    lv_obj_add_flag(search_ta, LV_OBJ_FLAG_EVENT_BUBBLE);
    lv_obj_add_event_cb(search_ta, on_search_ready, LV_EVENT_READY, NULL);

    /* 搜索框左侧图标 */
    {
        char path[512];
        snprintf(path, sizeof(path), "%s%s", UI_ICON_FOLDER, UI_ICON_SEARCH);
        lv_obj_t * search_icon = lv_image_create(search_ta);
        lv_image_set_src(search_icon, path);
        lv_obj_set_pos(search_icon, 10, 16);
    }

    /* 左侧：筛选 */
    btn_filter = ui_make_small_button(top_obj, NULL, UI_ICON_FILTER,
                                      UI_SEARCH_W + UI_BTN_GAP,
                                      on_filter_clicked, NULL);

    /* 右侧：帮助、导入、导出、删除，每个间隔 10px，整体贴右边 */
    int right_x = 1080 - 4 * UI_SMALL_BTN_W - 3 * UI_BTN_GAP;
    btn_help = ui_make_small_button(top_obj, NULL, UI_ICON_HELP,
                                    right_x, on_help_clicked, NULL);
    right_x += UI_SMALL_BTN_W + UI_BTN_GAP;
    btn_import = ui_make_small_button(top_obj, NULL, UI_ICON_IMPORT,
                                      right_x, on_import_clicked, NULL);
    right_x += UI_SMALL_BTN_W + UI_BTN_GAP;
    btn_export = ui_make_small_button(top_obj, NULL, UI_ICON_EXPORT,
                                      right_x, on_export_clicked, NULL);
    right_x += UI_SMALL_BTN_W + UI_BTN_GAP;
    btn_delete = ui_make_small_button(top_obj, NULL, UI_ICON_DELETE,
                                      right_x, on_delete_button_clicked, NULL);
}

static void ui_create_grid_area(lv_obj_t * scr)
{
    obj_list = lv_obj_create(scr);
    lv_obj_set_pos(obj_list, UI_LIST_X, UI_LIST_Y);
    lv_obj_set_size(obj_list, UI_LIST_W, UI_LIST_H);
    ui_obj_clear_default(obj_list);
    lv_obj_set_scroll_dir(obj_list, LV_DIR_ALL);
    lv_obj_add_flag(obj_list, LV_OBJ_FLAG_CLICKABLE | LV_OBJ_FLAG_EVENT_BUBBLE);

    for(int c = 0; c < UI_COL_NUM; c++) col_widths[c] = UI_DEFAULT_COL_W;
    ui_create_cells(obj_list);
    ui_create_sort_buttons(obj_list);
    ui_apply_geometry();
}

/**
 * 对外刷新接口：删除/导入/筛选/编辑后调用。
 */
void ui_refresh_grid(void)
{
    ui_refresh_grid_internal();
}

/**
 * 创建完整 UI。
 */
void ui_create(lv_obj_t * scr)
{
    /* 屏幕底色设为浅灰，便于看清顶部白色条与下方白色网格 */
    lv_obj_set_style_bg_color(scr, lv_color_hex(0xE0E0E0), 0);
    lv_obj_set_style_bg_opa(scr, LV_OPA_COVER, 0);
    lv_obj_set_style_text_font(scr, ui_font(), 0);

    /* 全局点击：点击编辑框以外区域时保存编辑 */
    lv_obj_add_event_cb(scr, on_screen_clicked, LV_EVENT_CLICKED, NULL);

    /* 键盘分组 */
    kb_group = lv_group_create();

    ui_create_top_bar(scr);
    ui_create_grid_area(scr);

    /* 文本框加入键盘分组 */
    lv_group_add_obj(kb_group, search_ta);

    /* 初始显示网格（无数据时数据层应返回 200 空行） */
    ui_refresh_grid_internal();
}

void ui_bind_keyboard(lv_indev_t * kb)
{
    if(kb && kb_group) lv_indev_set_group(kb, kb_group);
}
