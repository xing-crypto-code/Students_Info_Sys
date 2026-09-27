/*
 * @file ui.c
 * @brief 学生成绩信息管理系统 —— LVGL v9.5 UI 框架层实现。
 *
 * 代码分区：
 *   一、私有数据与对象句柄；
 *   二、前置声明；
 *   三、字体与默认样式；
 *   四、通用小工具；
 *   五、单元格工具；
 *   六、网格布局；
 *   七、单元格文本；
 *   八、顶部按钮回调（筛选、新增、帮助、导入导出、弹窗）；
 *   九、新增学生与闪烁提示；
 *   十、单元格点击与编辑；
 *   十一、屏幕全局点击；
 *   十二、UI 创建。
 *
 * 本文件只负责界面显示和用户交互，业务数据全部通过 core.h 的接口访问。
 */

/* 包含 UI 层头文件 ui.h；ui.h 内部再包含 core.h，提供数据层函数原型。 */
#include "ui.h"

/* 引入 LVGL 内置的 lodepng 解码器，用于把超大尺寸的帮助 PNG 解码到系统堆，
 * 再缩放到屏幕大小后交给 lv_imagebutton 显示（参见下方“帮助”一节）。 */
#include "src/libs/lodepng/lodepng.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
/* 提供 PRIu64/PRIu16 等格式化宏，用于打印学号和排名。 */
#include <inttypes.h>

/* 本文件使用的界面配置枚举定义在 ui.h 中，函数内部使用字面量常量。 */

/* ==================== 一、私有数据与对象句柄 ==================== */

/* 定义表头标题的静态常量字符串数组，长度与网格列数一致。 */
static const char* ui_headers[12] = {
    /* 表头：序号、年级、班级、学号、姓名、性别、平时、期中、期末、总分、绩点、排名 */
    "序号", "年级", "班级", "学号", "姓名", "性别",
    "平时", "期中", "期末", "总分", "绩点", "排名"
};

/* 保存顶部 1080x80 工具栏容器指针，供后续创建按钮和刷新布局时复用。 */
static lv_obj_t* top_obj;
/* 保存下方 1080x560 网格表格容器指针，网格单元格都挂在此容器上。 */
static lv_obj_t* obj_list;
static lv_obj_t* header_obj = NULL;        /* 固定在列表顶部的表头容器 */
/* 保存 搜索 输入框对象指针，读取搜索关键字时直接使用。 */
static lv_obj_t* search_ta;
/* 保存 筛选 按钮对象指针，用于设置图标和点击回调。 */
static lv_obj_t* btn_filter;
/* 保存 新增 按钮对象指针，点击后创建新学生信息。 */
static lv_obj_t* btn_add;
/* 保存 帮助 按钮对象指针，用于触发帮助图片轮播。 */
static lv_obj_t* btn_help;
/* 保存 导入 按钮对象指针，点击后弹出路径输入框。 */
static lv_obj_t* btn_import;
/* 保存 导出 按钮对象指针，点击后弹出路径输入框。 */
static lv_obj_t* btn_export;
/* 保存 删除 按钮对象指针，该按钮支持开关状态切换。 */
static lv_obj_t* btn_delete;

static char search_bg_path[512];   /* 搜索框背景图路径缓存 */

/* 保存 LVGL 键盘分组对象指针，用于将搜索框等控件加入键盘导航。 */
static lv_group_t* kb_group;

/* 网格：grid_cells[0] 是表头行，grid_cells[1..200] 是数据行 */
/* 声明表格单元格二维数组：第 0 行是表头，第 1 至 200 行显示学生数据，共 12 列。 */
static lv_obj_t* grid_cells[200 + 1][12];
/* 保存按内容自动计算的 12 列列宽，供表头和数据格对齐。 */
static lv_coord_t col_widths[12];

/* 单元格选中/编辑状态 */
/* 记录上次选中的数据行号，-1 表示当前无选中单元格。 */
static int last_sel_row = -1;
/* 记录上次选中的列号，-1 表示当前无选中单元格。 */
static int last_sel_col = -1;
/* 记录上次选中的单元格对象，便于切换选中时清除旧高亮。 */
static lv_obj_t* last_sel_cell = NULL;
/* 保存当前正在编辑的文本框对象，NULL 表示未进入编辑状态。 */
static lv_obj_t* edit_ta = NULL;
/* 记录编辑框所在网格行号（表头为 0 行），-1 表示未编辑。 */
static int edit_grid_row = -1;
/* 记录编辑框所在网格列号，-1 表示未编辑。 */
static int edit_grid_col = -1;
/* 记录编辑内容对应的学生数组下标，保存时定位到具体学生。 */
static int edit_student_index = -1;

/* 删除开关状态 */
/* 删除开关标志，true 时点击普通数据行会弹出删除确认。 */
static bool delete_enabled = false;
/* 保存删除确认弹窗对象，用于显示/关闭删除提示。 */
static lv_obj_t* delete_popup = NULL;
static lv_obj_t* weight_popup = NULL;
static lv_obj_t* invalid_id_popup = NULL;
/* 导出方式选择弹窗（学生成绩表 CSV / 统计结果 TXT）。 */
static lv_obj_t* export_choice_popup = NULL;
/* 保存待删除学生在数据数组中的下标，供确认回调删除。 */
static int delete_student_index = -1;

/* 帮助图片状态 */

static lv_obj_t* help_imagebtn = NULL;
/* 全屏半透明遮罩：承载帮助图片按钮，并负责“点击任意位置关闭”。 */
static lv_obj_t* help_backdrop = NULL;
/* 记录当前帮助图片索引，从 0 开始轮播。 */
static int help_index = 0;
/* 记录帮助图片总张数，用于索引越界时回绕。 */
static int help_count = 0;
/* 帮助图片解码并缩放后的像素缓冲（内存字节序为 B,G,R,A）；NULL 表示尚未加载。 */
static uint8_t* help_image_pixels = NULL;
/* 帮助图片描述符，指向 help_image_pixels；只要图片还在显示就必须保持有效。 */
static lv_image_dsc_t help_image_dsc;
/* “正在加载帮助图片”提示标签；加载完成后隐藏。 */
static lv_obj_t* help_loading_label = NULL;
/* 延迟加载帮助图片的单次定时器：先让提示文字显示一帧，再执行耗时的解码缩放。 */
static lv_timer_t* help_load_timer = NULL;

/* 导入/导出路径输入状态 */
/* 保存导入/导出路径输入框对象，确认时读取用户输入的路径。 */
static lv_obj_t* path_ta = NULL;
/* 记录当前路径输入用途：0 无操作、1 导入、2 导出，决定确认后调用 data_import_csv 还是 data_export_csv。 */
static int path_action = 0;                 /* 0=无, 1=导入, 2=导出CSV, 3=新增, 4~6=单项权重, 7=三项权重, 8=导出统计TXT */

/* 筛选弹层状态 */
/* 保存筛选一级类别列表对象（年级/班级/性别/分数/全部）。 */
static lv_obj_t* filter_list1 = NULL;
/* 保存筛选二级选项列表对象，展示某个类别下的具体选项。 */
static lv_obj_t* filter_list2 = NULL;
/* 记录二级列表当前对应的 ui_field_t 字段，用于生成选项。 */
static int filter_field = 0;
/* 声明最多 64 个筛选项、每项 MAX_FIELD_LEN 字符的缓冲区，接收数据层返回的选项。 */
static char filter_opt_buf[64][64];

/* 搜索高亮 / 导出提示定时器 */
/* 保存搜索高亮清除定时器对象，延迟后恢复单元格白底。 */
static lv_timer_t* search_timer = NULL;
/* 保存 Toast 自动隐藏定时器对象，到时删除提示框。 */
static lv_timer_t* toast_timer = NULL;
/* 保存 Toast 提示框对象，便于创建后定时移除。 */
static lv_obj_t* toast_obj = NULL;

/* 表头排序按钮：年级、班级、学号、性别、平时、期中、期末、总分、绩点、排名 */
static lv_obj_t* sort_buttons[10];
static int sort_states[10];   /* 0=未点击，1=当前正序，-1=当前倒序 */
static const int sort_fields[10] = {
    UI_FIELD_GRADE,
    UI_FIELD_CLASS,
    UI_FIELD_ID,
    UI_FIELD_GENDER,
    UI_FIELD_REGULAR,
    UI_FIELD_MIDTERM,
    UI_FIELD_FINAL,
    UI_FIELD_SCORE,
    UI_FIELD_GPA,
    UI_FIELD_RANK
};

/* ==================== 二、前置声明 ==================== */

/* 前置声明内部刷新网格函数，使后面定义可互相调用。 */
static void ui_refresh_grid_internal(void);
/* 前置声明布局应用函数，创建后统一计算对象位置尺寸。 */
static void ui_apply_geometry(void);

static void ui_scroll_to_student_index(int index);

static void ui_mark_failing_rows_red(void);

static void ui_start_new_student_blink(int student_index);

static void on_add_clicked(lv_event_t* e);
static void ui_do_search(void);
static void on_search_button_clicked(lv_event_t* e);
static void ui_open_path_ta(int action);
/* 前置声明单元格文本填充函数，buf 和 buf_size 指定输出缓冲区。 */
static void ui_fill_cell_text(int grid_row, int grid_col, char* buf, size_t buf_size);
/* 前置声明按当前显示行数自动分配列宽的函数。 */
static void ui_auto_columns(int display_count);
static void ui_set_cell_bg(lv_obj_t* cell, uint32_t color);
static void ui_reset_all_cells_white(void);
static bool ui_target_belongs_to_edit(lv_obj_t* target);
static void ui_get_ta_text(lv_obj_t* ta, char* out, size_t out_size);
static lv_obj_t* ui_make_small_button(lv_obj_t* parent, const char* text,
    const char* icon_name, int x, lv_event_cb_t cb, void* user_data);
static void ui_attach_icon(lv_obj_t* parent, const char* icon_name);
static void ui_set_cell_style(lv_obj_t* cell);
static void ui_create_cells(lv_obj_t* parent);
static void ui_create_sort_buttons(lv_obj_t* parent);
static void ui_position_sort_buttons(void);
static void on_sort_clicked(lv_event_t* e);

static const lv_font_t* ui_font(void);
static void ui_obj_clear_default(lv_obj_t* obj);
static void ui_close_edit(void);
static void ui_save_and_close_edit(void);
static void ui_open_cell_editor(int grid_row, int grid_col);
static void ui_close_filter_lists(void);
static void ui_show_toast(const char* msg);
static void ui_clear_search_hilight(lv_timer_t* timer);
static void on_screen_clicked(lv_event_t* e);
static void on_cell_clicked(lv_event_t* e);
static void on_delete_button_clicked(lv_event_t* e);
static void show_delete_popup(int student_index);
static void show_weight_warning_popup(float sum);
static void show_invalid_id_popup(void);
static void ui_repaint_cells_base(void);
static void ui_apply_weight_change(void);
static void ui_close_popup(lv_obj_t** popup);

/* ==================== 三、字体与默认样式 ==================== */

static const lv_font_t* g_ui_font = &lv_font_source_han_sans_sc_16_cjk;

/*
 * 获取当前 UI 统一字体；尚未设置时返回 LVGL 默认字体。
 *
 * 主要逻辑：
 *   1. 返回 UI 全局字体；尚未设置时返回 LVGL 默认字体。
 */
static const lv_font_t* ui_font(void)
{
    return g_ui_font;
}

/*
 * 设置 UI 统一字体，并应用到屏幕默认文本样式。
 *
 * 主要逻辑：
 *   1. 保存字体指针，并把屏幕默认字体设置为该字体。
 */
void ui_set_font(const lv_font_t* font)
{
    if (font != NULL) g_ui_font = font;
}

/*
 * 清除控件的默认背景、边框、圆角和内边距，得到简洁的透明容器。
 *
 * 主要逻辑：
 *   1. 设置滚动属性
 *   2. 调用 lv_obj_set_style_border_width()
 *   3. 调用 lv_obj_set_style_radius()
 *   4. 调用 lv_obj_set_style_pad_all()
 *   5. 调用 lv_obj_set_style_bg_color()
 */
static void ui_obj_clear_default(lv_obj_t* obj)
{
    lv_obj_set_style_border_width(obj, 0, 0);
    lv_obj_set_style_radius(obj, 0, 0);
    lv_obj_set_style_pad_all(obj, 0, 0);
    lv_obj_set_style_bg_color(obj, lv_color_hex(0xFFFFFF), 0);
    lv_obj_set_style_bg_opa(obj, LV_OPA_COVER, 0);
    lv_obj_set_style_text_font(obj, ui_font(), 0);
    lv_obj_set_scrollbar_mode(obj, LV_SCROLLBAR_MODE_AUTO);  // 设置滚动属性
}
/* ==================== 四、通用小工具 ==================== */

/*
 * 读取文本框内容并去除首尾空白，复制到调用者缓冲区。
 *
 * 主要逻辑：
 *   1. 调用 lv_textarea_get_text()
 *   2. 调用 strlen()
 *   3. 循环处理：i < len && (txt[i] == ' ' || txt[i] == '\t' || txt[i] == '\r' || tx...
 *   4. 循环处理：i < len && j + 1 < out_size
 *   5. 循环处理：j > 0 && (out[j - 1] == ' ' || out[j - 1] == '\t'
 */
static void ui_get_ta_text(lv_obj_t* ta, char* out, size_t out_size)
{
    const char* txt = lv_textarea_get_text(ta);
    size_t i, j, len;

    if (txt == NULL) txt = "";
    len = strlen(txt);

    i = 0;
    while (i < len && (txt[i] == ' ' || txt[i] == '\t' || txt[i] == '\r' || txt[i] == '\n')) i++;

    j = 0;
    while (i < len && j + 1 < out_size) {
        if (txt[i] == '\r' || txt[i] == '\n') { i++; continue; }
        out[j++] = txt[i++];
    }

    while (j > 0 && (out[j - 1] == ' ' || out[j - 1] == '\t')) j--;
    out[j] = '\0';
}

/*
 * 判断一次点击的目标对象是否属于当前单元格编辑框或其子控件。
 *
 * 主要逻辑：
 *   1. 循环处理：target
 *   2. 调用 lv_obj_get_parent()
 */
static bool ui_target_belongs_to_edit(lv_obj_t* target)
{
    while (target) {
        if (target == edit_ta) return true;
        target = lv_obj_get_parent(target);
    }
    return false;
}

/*
 * 在父对象上创建一个统一样式的小按钮并绑定点击回调。
 *
 * 主要逻辑：
 *   1. 顶部按钮不参与键盘分组，避免焦点切换时把表格滚动到右侧
 *   2. 调用 lv_button_create()
 *   3. 调用 lv_obj_set_pos()
 *   4. 调用 lv_obj_set_size()
 *   5. 调用 lv_obj_set_style_bg_color()
 */
static lv_obj_t* ui_make_small_button(lv_obj_t* parent, const char* text,
    const char* icon_name, int x, lv_event_cb_t cb, void* user_data)
{
    lv_obj_t* btn = lv_button_create(parent);
    lv_obj_set_pos(btn, x, 10);
    lv_obj_set_size(btn, 60, 60);
    lv_obj_set_style_bg_color(btn, lv_color_hex(0xFFFFFF), 0);
    lv_obj_set_style_bg_opa(btn, LV_OPA_COVER, 0);
    lv_obj_set_style_text_color(btn, lv_color_hex(0x000000), 0);
    lv_obj_set_style_text_font(btn, ui_font(), 0);
    lv_obj_add_flag(btn, LV_OBJ_FLAG_EVENT_BUBBLE);

    if (icon_name != NULL && icon_name[0] != '\0') {
        ui_attach_icon(btn, icon_name);
    }
    else if (text != NULL) {
        lv_obj_t* label = lv_label_create(btn);
        lv_label_set_text(label, text);
        lv_obj_set_style_text_font(label, ui_font(), 0);
        lv_obj_center(label);
    }

    lv_obj_add_event_cb(btn, cb, LV_EVENT_CLICKED, user_data);
    // 顶部按钮不参与键盘分组，避免焦点切换时把表格滚动到右侧
    lv_group_remove_obj(btn);
    return btn;
}

/*
 * 在指定控件上创建并居中显示 PNG 图标。
 *
 * 主要逻辑：
 *   1. 调用 lv_image_create()
 *   2. 调用 snprintf()
 *   3. 调用 lv_image_set_src()
 *   4. 调用 lv_obj_center()
 */
static void ui_attach_icon(lv_obj_t* parent, const char* icon_name)
{
    if (icon_name == NULL || icon_name[0] == '\0') return;

    lv_obj_t* img = lv_image_create(parent);
    char path[512];
    snprintf(path, sizeof(path), "%s%s", "A:btn_png/", icon_name);
    lv_image_set_src(img, path);
    lv_obj_center(img);
}

/* ==================== 五、单元格工具 ==================== */

/*
 * 设置某个网格单元格的背景颜色，用于高亮、标红和恢复白底。
 *
 * 主要逻辑：
 *   1. 调用 lv_obj_set_style_bg_color()
 *   2. 调用 lv_color_hex()
 *   3. 调用 lv_obj_set_style_bg_opa()
 */
static void ui_set_cell_bg(lv_obj_t* cell, uint32_t color)
{
    lv_obj_set_style_bg_color(cell, lv_color_hex(color), 0);
    lv_obj_set_style_bg_opa(cell, LV_OPA_COVER, 0);
}

/*
 * 设置单元格的边框、背景、字体、内边距和点击样式。
 *
 * 主要逻辑：
 *   1. 调用 lv_obj_set_style_border_width()
 *   2. 调用 lv_obj_set_style_border_color()
 *   3. 调用 lv_color_hex()
 *   4. 调用 lv_obj_set_style_radius()
 *   5. 调用 lv_obj_set_style_pad_all()
 */
static void ui_set_cell_style(lv_obj_t* cell)
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

    int pad_top = (50 - (int)lv_font_get_line_height(ui_font())) / 2;
    if (pad_top < 0) pad_top = 0;
    lv_obj_set_style_pad_top(cell, pad_top, 0);

    lv_label_set_long_mode(cell, LV_LABEL_LONG_MODE_DOTS);
    lv_obj_add_flag(cell, LV_OBJ_FLAG_CLICKABLE | LV_OBJ_FLAG_EVENT_BUBBLE);
}

/*
 * 创建 201 行 × 12 列的网格单元格，并给数据单元格绑定点击事件。
 *
 * 主要逻辑：
 *   1. 表头行（第 0 行）放在固定容器 header_obj 中，不随数据列表滚动。
 *   2. 数据行（第 1 ~ 200行）仍然放在可滚动的 obj_list 中。
 */
static void ui_create_cells(lv_obj_t* parent)
{
    // 表头行（第 0 行）放在固定容器 header_obj 中，不随数据列表滚动。
    for (int c = 0; c < 12; c++) {
        lv_obj_t* cell = lv_label_create(header_obj);
        grid_cells[0][c] = cell;
        ui_set_cell_style(cell);
        lv_obj_add_event_cb(cell, on_cell_clicked, LV_EVENT_CLICKED,
            (void*)(intptr_t)((0) * 16 + (c)));
    }

    // 数据行（第 1 ~ 200行）仍然放在可滚动的 obj_list 中。
    for (int r = 1; r <= 200; r++) {
        for (int c = 0; c < 12; c++) {
            lv_obj_t* cell = lv_label_create(parent);
            grid_cells[r][c] = cell;
            ui_set_cell_style(cell);
            lv_obj_add_event_cb(cell, on_cell_clicked, LV_EVENT_CLICKED,
                (void*)(intptr_t)((r) * 16 + (c)));
        }
    }
}

/* ==================== 六、网格布局 ==================== */

/*
 * 按当前 col_widths 给表头和数据单元格设置坐标与尺寸。
 *
 * 主要逻辑：
 *   1. 表头行固定在 header_obj 顶部 y=0；数据行在 obj_list 中从 y=0 开始。
 *   2. 循环处理：int r = 0; r <= 200; r++
 *   3. 循环处理：int c = 0; c < 12; c++
 *   4. 调用 lv_obj_set_pos()
 *   5. 调用 lv_obj_set_size()
 */
static void ui_apply_geometry(void)
{
    for (int r = 0; r <= 200; r++) {
        lv_coord_t x = 0;
        // 表头行固定在 header_obj 顶部 y=0；数据行在 obj_list 中从 y=0 开始。
        lv_coord_t y = (r == 0) ? 0 : (lv_coord_t)(r - 1) * 50;

        for (int c = 0; c < 12; c++) {
            lv_obj_set_pos(grid_cells[r][c], x, y);
            lv_obj_set_size(grid_cells[r][c], col_widths[c], 50);
            x += col_widths[c];
        }
    }

    ui_position_sort_buttons();
}

/*
 * 把四个表头排序按钮对齐到对应表头单元格的右侧。
 *
 * 主要逻辑：
 *   1. 循环处理：int i = 0; i < 10; i++
 *   2. 循环处理：int c = 0; c < col; c++
 *   3. 调用 lv_obj_set_pos()
 */
static void ui_position_sort_buttons(void)
{
    for (int i = 0; i < 10; i++) {
        if (sort_buttons[i] == NULL) continue;

        int field = sort_fields[i];
        int col = field + 1;

        lv_coord_t x = 0;
        for (int c = 0; c < col; c++) x += col_widths[c];
        x += col_widths[col] - 20 - 2;

        lv_obj_set_pos(sort_buttons[i], x, 5);
    }
}

/*
 * 表头排序按钮点击回调：切换正序/倒序，调用 data_sort_students() 并刷新表格。
 *
 * 主要逻辑：
 *   1. 调用 lv_event_get_user_data()
 *   2. 循环处理：int i = 0; i < 10; i++
 *   3. 调用 data_sort_students()
 *   4. 调用 ui_refresh_grid_internal()
 */
static void on_sort_clicked(lv_event_t* e)
{
    int field = (int)(intptr_t)lv_event_get_user_data(e);
    int idx = -1;

    for (int i = 0; i < 10; i++) {
        if (sort_fields[i] == field) {
            idx = i;
            break;
        }
    }
    if (idx < 0) return;

    bool ascending;
    if (sort_states[idx] == 1) {
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

/*
 * 创建年级、班级、性别、总分四个表头排序按钮。
 *
 * 主要逻辑：
 *   1. 排序按钮不参与键盘分组，避免被自动聚焦后滚动表格
 *   2. 循环处理：int i = 0; i < 10; i++
 *   3. 调用 lv_button_create()
 *   4. 调用 lv_obj_set_size()
 *   5. 调用 lv_obj_set_style_border_width()
 */
static void ui_create_sort_buttons(lv_obj_t* parent)
{
    for (int i = 0; i < 10; i++) {
        sort_buttons[i] = lv_button_create(parent);
        lv_obj_set_size(sort_buttons[i], 20, 40);
        lv_obj_set_style_border_width(sort_buttons[i], 0, 0);
        lv_obj_set_style_radius(sort_buttons[i], 0, 0);
        lv_obj_set_style_bg_opa(sort_buttons[i], LV_OPA_TRANSP, 0);
        lv_obj_add_flag(sort_buttons[i], LV_OBJ_FLAG_CLICKABLE | LV_OBJ_FLAG_EVENT_BUBBLE);

        char path[512];
        snprintf(path, sizeof(path), "%s%s", "A:btn_png/", "sort.png");
        lv_obj_t* img = lv_image_create(sort_buttons[i]);
        lv_image_set_src(img, path);
        lv_obj_center(img);

        lv_obj_add_event_cb(sort_buttons[i], on_sort_clicked, LV_EVENT_CLICKED,
            (void*)(intptr_t)sort_fields[i]);

        // 排序按钮不参与键盘分组，避免被自动聚焦后滚动表格
        lv_group_remove_obj(sort_buttons[i]);
    }
}

/* ==================== 七、单元格文本 ==================== */

/*
 * 根据网格行列生成应显示的文本：表头、学生字段或空白。
 *
 * 主要逻辑：
 *   1. 调用 snprintf()
 *   2. 调用 data_get_display_index()
 *   3. 调用 data_get_all()
 *   4. 按 grid_col - 1 分支处理
 *   5. 调用 data_calc_total()
 */
static void ui_fill_cell_text(int grid_row, int grid_col, char* buf, size_t buf_size)
{
    if (buf_size == 0) return;
    buf[0] = '\0';

    if (grid_row == 0) {
        if (grid_col >= 0 && grid_col < 12) {
            snprintf(buf, buf_size, "%s", ui_headers[grid_col]);
        }
        return;
    }

    if (grid_col == 0) {
        snprintf(buf, buf_size, "%d", grid_row);
        return;
    }

    int display_row = grid_row - 1;
    int idx = data_get_display_index(display_row);
    if (idx < 0) {
        buf[0] = '\0';
        return;
    }

    StudentCSV* s = data_get_all();
    if (s == NULL) {
        buf[0] = '\0';
        return;
    }
    s = &s[idx];

    switch (grid_col - 1) {
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
    case UI_FIELD_REGULAR:
        snprintf(buf, buf_size, "%.1f", s->regular_score);
        break;
    case UI_FIELD_MIDTERM:
        snprintf(buf, buf_size, "%.1f", s->midterm_score);
        break;
    case UI_FIELD_FINAL:
        snprintf(buf, buf_size, "%.1f", s->final_score);
        break;
    case UI_FIELD_SCORE: {
        float total = data_calc_total(s->regular_score, s->midterm_score, s->final_score);
        snprintf(buf, buf_size, "%.1f", total);
        break;
    }
    case UI_FIELD_GPA: {
        float total = data_calc_total(s->regular_score, s->midterm_score, s->final_score);
        snprintf(buf, buf_size, "%.2f", data_calc_gpa(total));
        break;
    }
    case UI_FIELD_RANK:
        snprintf(buf, buf_size, "%" PRIu16, s->rank);
        break;
    default:
        buf[0] = '\0';
        break;
    }
}

/*
 * 把剩余宽度平均加到各自适应列上，前 rem 列各多 1 像素。
 *
 * 主要逻辑：
 *   1. 循环处理：int i = 0; i < count; i++
 */
static void ui_add_width_evenly(const int* adaptive_index, int count, int each, int rem)
{
    for (int i = 0; i < count; i++) {
        int c = adaptive_index[i];
        col_widths[c] += each;
        if (i < rem) col_widths[c] += 1;
    }
}

/*
 * 按当前显示行数和内容计算 12 列宽度，保证总宽度为 1080。
 *
 * 主要逻辑：
 *   1. 循环处理：int c = 0; c < 12; c++
 *   2. 循环处理：int r = 0; r <= display_count; r++
 *   3. 调用 ui_fill_cell_text()
 *   4. 调用 lv_text_get_size()
 *   5. 调用 ui_font()
 */
static void ui_auto_columns(int display_count)
{
    char buf[128];
    int fixed_total = 0;
    int adaptive_index[12];
    int adaptive_count = 0;
    int desired[12];

    for (int c = 0; c < 12; c++) {
        desired[c] = 60;

        if (c == 0) {
            col_widths[c] = 60;
            fixed_total += 60;
            continue;
        }
        if (c == UI_FIELD_ID + 1) {
            col_widths[c] = 130;
            fixed_total += 130;
            continue;
        }
        if (c == UI_FIELD_GENDER + 1) {
            col_widths[c] = 80;
            fixed_total += 80;
            continue;
        }

        adaptive_index[adaptive_count++] = c;

        int max_w = 60;
        for (int r = 0; r <= display_count; r++) {
            ui_fill_cell_text(r, c, buf, sizeof(buf));
            if (buf[0] == '\0') continue;

            lv_point_t size;
            lv_text_get_size(&size, buf, ui_font(), 0, 0,
                LV_COORD_MAX, LV_TEXT_FLAG_NONE);
            int w = (int)size.x + 8 * 2;
            if (w > max_w) max_w = w;
        }
        desired[c] = max_w;
    }

    int target = 1080 - fixed_total;
    if (target < adaptive_count * 60) {
        target = adaptive_count * 60;
    }

    for (int i = 0; i < adaptive_count; i++) {
        int c = adaptive_index[i];
        col_widths[c] = 60;
    }

    int extra_pool = target - adaptive_count * 60;
    long long extra_need_sum = 0;

    for (int i = 0; i < adaptive_count; i++) {
        int c = adaptive_index[i];
        desired[c] -= 60;
        if (desired[c] < 0) desired[c] = 0;
        extra_need_sum += desired[c];
    }

    if (extra_pool > 0 && extra_need_sum > 0 && extra_need_sum <= extra_pool) {
        int used = 0;
        for (int i = 0; i < adaptive_count; i++) {
            int c = adaptive_index[i];
            col_widths[c] += desired[c];
            used += desired[c];
        }
        int leftover = extra_pool - used;
        int each = leftover / adaptive_count;
        int rem = leftover % adaptive_count;
        ui_add_width_evenly(adaptive_index, adaptive_count, each, rem);
    }
    else if (extra_pool > 0 && extra_need_sum > extra_pool) {
        int used = 0;
        for (int i = 0; i < adaptive_count; i++) {
            int c = adaptive_index[i];
            int alloc = (int)((long long)extra_pool * desired[c] / extra_need_sum);
            col_widths[c] += alloc;
            used += alloc;
        }
        int rem = extra_pool - used;
        for (int i = 0; i < adaptive_count && rem > 0; i++) {
            int c = adaptive_index[i];
            if (desired[c] > 0) {
                col_widths[c] += 1;
                rem--;
            }
        }
        for (int i = 0; i < adaptive_count && rem > 0; i++) {
            col_widths[adaptive_index[i]] += 1;
            rem--;
        }
    }
    else if (extra_pool > 0) {
        int each = extra_pool / adaptive_count;
        int rem = extra_pool % adaptive_count;
        ui_add_width_evenly(adaptive_index, adaptive_count, each, rem);
    }

    int sum = 0;
    for (int c = 0; c < 12; c++) sum += col_widths[c];
    if (sum != 1080 && adaptive_count > 0) {
        col_widths[adaptive_index[0]] += (1080 - sum);
    }
}

/*
 * 定义辅助函数：把所有单元格背景统一恢复成白色，用于清除高亮或选中状态。
 *
 * 主要逻辑：
 *   1. 外层行循环覆盖从表头到最大数据行的全部网格位置。
 *   2. 内层列循环遍历每一列，逐个单元格恢复颜色。
 *   3. 直接设置指定单元格背景色为白色 0xFFFFFF。
 */
static void ui_reset_all_cells_white(void)
{
    // 外层行循环覆盖从表头到最大数据行的全部网格位置。
    for (int r = 0; r <= 200; r++) {
        // 内层列循环遍历每一列，逐个单元格恢复颜色。
        for (int c = 0; c < 12; c++) {
            // 直接设置指定单元格背景色为白色 0xFFFFFF。
            ui_set_cell_bg(grid_cells[r][c], 0xFFFFFF);
        }
    }
}

/*
 * 定义网格核心刷新函数：负责从数据层取可见行数、重算列宽、重新填充并清理高亮。
 *
 * 主要逻辑：
 *   1. 调用数据层获取当前应显示的数据行数，数值会受筛选和搜索影响。
 *   2. 若数据层返回负数则按 0 处理，避免出现无效的负行数。
 *   3. 若可见行数超过界面支持的最大数据行数则截断，防止数组越界。
 *   4. 先重算列宽并摆放位置
 *   5. 先把全部列宽重置为默认值，再重新进行自动测量，避免旧列宽残留。
 *   6. 根据当前可见行数调用自动列宽函数，重算各列宽度。
 */
static void ui_refresh_grid_internal(void)
{
    // 调用数据层获取当前应显示的数据行数，数值会受筛选和搜索影响。
    int display_count = data_get_display_count();
    // 若数据层返回负数则按 0 处理，避免出现无效的负行数。
    if (display_count < 0) display_count = 0;
    // 若可见行数超过界面支持的最大数据行数则截断，防止数组越界。
    if (display_count > 200) display_count = 200;

    // 先重算列宽并摆放位置
    // 先把全部列宽重置为默认值，再重新进行自动测量，避免旧列宽残留。
    for (int c = 0; c < 12; c++) col_widths[c] = 120;
    // 根据当前可见行数调用自动列宽函数，重算各列宽度。
    ui_auto_columns(display_count);
    // 把新列宽和行列位置应用到网格对象上，完成几何布局。
    ui_apply_geometry();

    // 声明局部缓冲区，在逐格填充文本时复用。
    char buf[128];

    // 外层行循环覆盖表头与所有可能的数据行位置，统一处理显示或隐藏。
    for (int r = 0; r <= 200; r++) {
        // 判断当前行是否大于实际可见行数，是则属于需要隐藏的空白行。
        bool is_data_row_hidden = (r > display_count);

        // 内层列循环逐格设置文本和隐藏标志。
        for (int c = 0; c < 12; c++) {
            // 生成该行该列应显示的文本放入 buf，可能来自表头或数据层。
            ui_fill_cell_text(r, c, buf, sizeof(buf));
            // 把文本设置到对应 LVGL 标签上，真正更新界面显示内容。
            lv_label_set_text(grid_cells[r][c], buf);

            // 若当前行在可见范围之外，进入隐藏分支。
            if (is_data_row_hidden) {
                // 给单元格添加 LVGL 隐藏标志，使这行不再显示。
                lv_obj_add_flag(grid_cells[r][c], LV_OBJ_FLAG_HIDDEN);
            }
            // 否则当前行在可见范围内，进入显示分支。
            else {
                // 清除单元格的隐藏标志，让该行恢复显示。
                lv_obj_clear_flag(grid_cells[r][c], LV_OBJ_FLAG_HIDDEN);
            }
        }
    }

    // 刷新后不保留临时高亮/选中状态
    // 将上次选中的行号和列号同时置为 -1，表示当前没有选中项。
    last_sel_row = last_sel_col = -1;
    // 把上次选中的单元格对象指针清空，防止继续引用旧对象。
    last_sel_cell = NULL;
    // 把所有单元格背景重置为白色，清除上一次遗留的选中或搜索高亮。
    ui_repaint_cells_base();

    // 调用 LVGL 使列表对象失效并重绘，让刷新结果立刻出现在屏幕上。
    lv_obj_invalidate(obj_list);

    // 表格总宽正好是 1080，不存在横向滚动，强制回到最左侧
    lv_obj_scroll_to_x(obj_list, 0, LV_ANIM_OFF);
}

/* ==================== 八、顶部按钮回调 ==================== */

/*
 * 定义搜索框回车回调：读取关键词、查找匹配行并整行黄色高亮。
 *
 * 主要逻辑：
 *   1. 让第一条匹配行尽量停留在表格窗口的垂直中间。
 *   2. 表头已移出滚动区，第 first_row 行数据在 obj_list 中的 y = first_row * UI_ROW_H。
 */
static void ui_do_search(void)
{
    char keyword[256];

    ui_save_and_close_edit();
    ui_get_ta_text(search_ta, keyword, sizeof(keyword));

    if (search_timer) {
        lv_timer_delete(search_timer);
        search_timer = NULL;
    }

    if (keyword[0] == '\0') {
        ui_refresh_grid_internal();
        ui_show_toast("请输入搜索关键字");
        return;
    }

    int matches[200];
    int match_count = data_find_matches(keyword, matches, 200);
    if (match_count > 200) match_count = 200;

    ui_refresh_grid_internal();

    for (int i = 0; i < match_count; i++) {
        int display_row = matches[i];
        if (display_row < 0 || display_row >= 200) continue;

        int grid_row = display_row + 1;
        for (int c = 0; c < 12; c++) {
            ui_set_cell_bg(grid_cells[grid_row][c], 0xFFFF00);
        }
    }

    if (match_count > 0) {
        int first_row = matches[0];
        if (first_row >= 0 && first_row < 200) {
            // 让第一条匹配行尽量停留在表格窗口的垂直中间。
            // 表头已移出滚动区，第 first_row 行数据在 obj_list 中的 y = first_row * UI_ROW_H。
            lv_coord_t view_h = lv_obj_get_height(obj_list);
            lv_coord_t target_y = (lv_coord_t)(first_row * 50 +
                50 / 2 - view_h / 2);
            if (target_y < 0) target_y = 0;
            lv_obj_scroll_to_y(obj_list, target_y, LV_ANIM_ON);
        }

        search_timer = lv_timer_create(ui_clear_search_hilight, 5000, NULL);
        if (search_timer) lv_timer_set_repeat_count(search_timer, 1);
    }
    else {
        ui_show_toast("未找到匹配的学生");
    }
}

/*
 * 搜索框回车回调：调用 ui_do_search() 执行搜索。
 *
 * 主要逻辑：
 *   1. 调用 ui_do_search()
 */
static void on_search_ready(lv_event_t* e)
{
    (void)e;
    ui_do_search();
}

/*
 * 搜索按钮点击回调：调用 ui_do_search() 执行搜索。
 *
 * 主要逻辑：
 *   1. 调用 ui_do_search()
 */
static void on_search_button_clicked(lv_event_t* e)
{
    (void)e;
    ui_do_search();
}

/*
 * 定义搜索高亮定时器回调：清空定时器指针并刷新网格恢复白色。
 *
 * 主要逻辑：
 *   1. 显式忽略定时器参数，避免未使用参数告警。
 *   2. 先把全局定时器指针置空，表示高亮定时器已经结束。
 *   3. 若列表对象仍存在则重新刷新网格，把所有黄色高亮恢复为白色。
 */
static void ui_clear_search_hilight(lv_timer_t* timer)
{
    // 显式忽略定时器参数，避免未使用参数告警。
    (void)timer;
    // 先把全局定时器指针置空，表示高亮定时器已经结束。
    search_timer = NULL;
    // 若列表对象仍存在则重新刷新网格，把所有黄色高亮恢复为白色。
    if (obj_list) ui_refresh_grid_internal();
}

/* ---------- 8.1 筛选 ---------- */

/*
 * 定义关闭一级和二级筛选列表的函数，用于筛选操作结束后的清理。
 *
 * 主要逻辑：
 *   1. 若二级筛选列表对象存在，进入删除分支。
 *   2. 删除二级列表对象，释放其占用的 LVGL 资源。
 *   3. 把二级列表指针置空，避免后续重复删除或访问悬空指针。
 *   4. 若一级筛选列表对象存在，进入删除分支。
 *   5. 删除一级列表对象，关闭已展开的筛选入口。
 *   6. 把一级列表指针置空，保持状态一致。
 */
static void ui_close_filter_lists(void)
{
    // 若二级筛选列表对象存在，进入删除分支。
    if (filter_list2) {
        // 删除二级列表对象，释放其占用的 LVGL 资源。
        lv_obj_delete(filter_list2);
        // 把二级列表指针置空，避免后续重复删除或访问悬空指针。
        filter_list2 = NULL;
    }
    // 若一级筛选列表对象存在，进入删除分支。
    if (filter_list1) {
        // 删除一级列表对象，关闭已展开的筛选入口。
        lv_obj_delete(filter_list1);
        // 把一级列表指针置空，保持状态一致。
        filter_list1 = NULL;
    }
}

/*
 * 定义二级筛选项点击回调：从编码中解析字段与选项并应用筛选。
 *
 * 主要逻辑：
 *   1. 从 LVGL 事件用户数据中取出打包的整数编码，该编码同时包含字段号和选项号。
 *   2. 编码除以 64 得到字段号，用于确定要筛选哪个字段。
 *   3. 编码对 64 取余得到选项下标，用于确定选中第几个筛选项。
 *   4. 应用筛选前先保存并关闭编辑中的单元格，避免数据冲突。
 *   5. 校验字段号是否在合法字段范围内，并准备校验选项下标。
 *   6. 继续校验选项下标小于 64 且对应选项文本非空，全部通过才执行筛选。
 */
static void on_filter_option_clicked(lv_event_t* e)
{
    // 从 LVGL 事件用户数据中取出打包的整数编码，该编码同时包含字段号和选项号。
    int code = (int)(intptr_t)lv_event_get_user_data(e);
    // 编码除以 64 得到字段号，用于确定要筛选哪个字段。
    int field = code / 64;
    // 编码对 64 取余得到选项下标，用于确定选中第几个筛选项。
    int index = code % 64;

    // 应用筛选前先保存并关闭编辑中的单元格，避免数据冲突。
    ui_save_and_close_edit();
    // 校验字段号是否在合法字段范围内，并准备校验选项下标。
    if (field >= 0 && field < UI_FIELD_COUNT &&
        // 继续校验选项下标小于 64 且对应选项文本非空，全部通过才执行筛选。
        index >= 0 && index < 64 && filter_opt_buf[index][0] != '\0') {
        // 调用数据层应用指定字段的具体筛选值，更新底层数据集合。
        data_apply_filter(field, filter_opt_buf[index]);
    }
    // 筛选应用完成后关闭两个筛选列表，让界面恢复简洁。
    ui_close_filter_lists();
    // 重新刷新网格，使新的筛选结果立即展示出来。
    ui_refresh_grid_internal();

    // 筛选后跳回表格最上方
    if (obj_list) lv_obj_scroll_to_y(obj_list, 0, LV_ANIM_ON);
}

/*
 * 定义一级筛选类别点击回调，处理年级、班级、性别、分数和全部入口。
 *
 * 主要逻辑：
 *   1. 从事件用户数据中取出被点击的筛选类别编号。
 *   2. 切换筛选前先保存并关闭正在编辑的单元格，防止筛选刷新丢失输入。
 *   3. 若点击的是“全部”类别，进入清除筛选的分支。
 *   4. 调用数据层清除当前筛选条件，恢复显示全部学生。
 *   5. 清除筛选后关闭所有筛选下拉列表。
 *   6. 刷新网格以显示未筛选的完整数据。
 */
static void on_filter_kind_clicked(lv_event_t* e)
{
    // 从事件用户数据中取出被点击的筛选类别编号。
    int kind = (int)(intptr_t)lv_event_get_user_data(e);

    // 切换筛选前先保存并关闭正在编辑的单元格，防止筛选刷新丢失输入。
    ui_save_and_close_edit();

    // 若点击的是“全部”类别，进入清除筛选的分支。
    if (kind == UI_FILTER_ALL) {
        // 调用数据层清除当前筛选条件，恢复显示全部学生。
        data_clear_filter();
        // 清除筛选后关闭所有筛选下拉列表。
        ui_close_filter_lists();
        // 刷新网格以显示未筛选的完整数据。
        ui_refresh_grid_internal();

        // 清除筛选后也跳回表格最上方
        if (obj_list) lv_obj_scroll_to_y(obj_list, 0, LV_ANIM_ON);

        // 处理完“全部”后直接返回，不再继续创建二级列表。
        return;
    }

    // 换算成 StudentCSV 字段号
    // 进入按筛选类别分发到具体字段的 switch 分支。
    switch (kind) {
        // 年级类别对应学生数据的年级字段，记录后跳出分支。
    case UI_FILTER_GRADE:  filter_field = UI_FIELD_GRADE;  break;
        // 班级类别对应班级字段，记录到全局筛选字段后跳出。
    case UI_FILTER_CLASS:  filter_field = UI_FIELD_CLASS;  break;
        // 性别类别对应性别字段，用于打开性别选项列表。
    case UI_FILTER_GENDER: filter_field = UI_FIELD_GENDER; break;
        // 分数类别对应分数字段，用于打开分数筛选范围列表。
    case UI_FILTER_SCORE:  filter_field = UI_FIELD_SCORE;  break;
        // 遇到无法识别的类别直接返回，不改变当前筛选字段。
    default: return;
    }

    // 向数据层请求该字段可用的筛选项，最多 64 项，存入全局选项缓冲区。
    int n = data_get_filter_options(filter_field, filter_opt_buf, 64);
    // 若返回数量超过 64 则截断，防止后续访问越界。
    if (n > 64) n = 64;

    // 关闭旧的二级列表
    // 若已存在旧的二级列表，先删除再重建，避免多个列表叠加。
    if (filter_list2) {
        // 删除旧的二级列表对象，释放界面资源。
        lv_obj_delete(filter_list2);
        // 把二级列表指针置空，等待重新创建。
        filter_list2 = NULL;
    }

    // 若该字段没有可选筛选项则直接返回，不弹出空列表。
    if (n <= 0) return;

    // 二级列表放在一级列表右侧
    // 获取当前活动屏幕作为父对象，用于把二级列表创建在正确层级。
    lv_obj_t* parent = lv_screen_active();
    // 创建 LVGL 列表对象作为二级选项列表，并保存到全局指针供后续关闭。
    filter_list2 = lv_list_create(parent);
    // 把二级列表移动到右侧固定坐标 (900,100)，与一级列表并排显示。
    lv_obj_set_pos(filter_list2, 480, 10);
    // 设置二级列表的尺寸为 170x300，为多个选项留出滚动空间。
    lv_obj_set_size(filter_list2, 200, 250);
    // 将二级列表背景设为白色，与整体界面风格保持一致。
    lv_obj_set_style_bg_color(filter_list2, lv_color_hex(0xFFFFFF), 0);
    // 设置二级列表边框宽度为 1 像素，让列表边界清晰。
    lv_obj_set_style_border_width(filter_list2, 1, 0);
    // 设置边框颜色为灰色，使列表与浅色背景之间有适度区分。
    lv_obj_set_style_border_color(filter_list2, lv_color_hex(0x888888), 0);
    // 把二级列表的文本字体设置为界面统一字体，保证中文正常显示。
    lv_obj_set_style_text_font(filter_list2, ui_font(), 0);
    // 开启事件冒泡标志，使列表内的点击事件可以继续向上层传播。
    lv_obj_add_flag(filter_list2, LV_OBJ_FLAG_EVENT_BUBBLE);
    // 把二级列表移到前台，避免被其它界面对象遮挡。
    lv_obj_move_foreground(filter_list2);

    // 遍历该字段的全部筛选项，为每个选项创建一个按钮。
    for (int i = 0; i < n; i++) {
        // 在二级列表中新增带文字的按钮，文本为该筛选项名称。
        lv_obj_t* btn = lv_list_add_button(filter_list2, NULL, filter_opt_buf[i]);
        // 仅当按钮创建成功时才注册事件与设置样式，防止空指针操作。
        if (btn) {
            // 为按钮注册点击回调，传入字段号与选项序号编码作为用户数据。
            lv_obj_add_event_cb(btn, on_filter_option_clicked, LV_EVENT_CLICKED,
                // 补充编码细节：使用 filter_field*64+i，使回调能还原字段号和选项号。
                (void*)(intptr_t)(filter_field * 64 + i));
            // 把按钮文本字体设置为界面统一字体，保证文字渲染一致。
            lv_obj_set_style_text_font(btn, ui_font(), 0);
            // 给按钮开启事件冒泡标志，使点击事件能按预期向上传递。
            lv_obj_add_flag(btn, LV_OBJ_FLAG_EVENT_BUBBLE);
        }
    }
}

/*
 * 筛选按钮的 LVGL 点击回调函数签名；用户点击界面上的“筛选”控件时进入此函数。
 *
 * 主要逻辑：
 *   1. 显式忽略 LVGL 传入的事件参数 e，避免编译器报告未使用参数，同时本回调不需要读取事件细节。
 *   2. 先保存并关闭当前可能正在进行的表格编辑，防止弹出筛选列表时残留编辑状态或导致数据不一致。
 *   3. 若已打开则先关闭，实现“再点一次收起”
 *   4. 若全局筛选列表控件 filter_list1 已存在，说明列表已经展开，再次点击按钮应执行收起逻辑。
 *   5. 调用关闭函数销毁所有已展开的筛选列表并清理相关全局指针，实现“再点一次收起”的交互。
 *   6. 收起后立即结束回调，不再继续创建新的筛选列表。
 */
static void on_filter_clicked(lv_event_t* e)
{
    // 显式忽略 LVGL 传入的事件参数 e，避免编译器报告未使用参数，同时本回调不需要读取事件细节。
    (void)e;
    // 先保存并关闭当前可能正在进行的表格编辑，防止弹出筛选列表时残留编辑状态或导致数据不一致。
    ui_save_and_close_edit();

    // 若已打开则先关闭，实现“再点一次收起”
    // 若全局筛选列表控件 filter_list1 已存在，说明列表已经展开，再次点击按钮应执行收起逻辑。
    if (filter_list1) {
        // 调用关闭函数销毁所有已展开的筛选列表并清理相关全局指针，实现“再点一次收起”的交互。
        ui_close_filter_lists();
        // 收起后立即结束回调，不再继续创建新的筛选列表。
        return;
    }

    // 获取当前活动屏幕作为父容器，保证筛选下拉列表创建在用户当前可见的屏幕上。
    lv_obj_t* parent = lv_screen_active();
    // 在父屏幕上新建一个 LVGL 列表并保存到全局 filter_list1，供之后判断展开状态和统一关闭使用。
    filter_list1 = lv_list_create(parent);
    // 把筛选列表定位到屏幕坐标 (700,100)，使其出现在表格右上方的固定下拉位置。
    lv_obj_set_pos(filter_list1, 380, 10);
    // 将列表尺寸设为宽 170、高 300，让 5 个筛选选项完整显示且尽量不遮挡主要数据区域。
    lv_obj_set_size(filter_list1, 100, 250);
    // 列表背景设为白色，使下拉菜单在彩色界面中保持清晰易读。
    lv_obj_set_style_bg_color(filter_list1, lv_color_hex(0xFFFFFF), 0);
    // 为列表设置 1 像素边框，让筛选菜单与页面内容之间有明确边界。
    lv_obj_set_style_border_width(filter_list1, 1, 0);
    // 边框颜色使用灰色，形成柔和但可见的分隔，避免纯白背景融为一体。
    lv_obj_set_style_border_color(filter_list1, lv_color_hex(0x888888), 0);
    // 列表文字统一使用 UI 自定义字体，保证中文筛选项显示正常且风格一致。
    lv_obj_set_style_text_font(filter_list1, ui_font(), 0);
    // 给列表加上事件冒泡标志，使列表内部的点击行为可以向上传播，配合程序统一的关闭/背景点击机制。
    lv_obj_add_flag(filter_list1, LV_OBJ_FLAG_EVENT_BUBBLE);
    // 把筛选列表移到前台，避免表格或其他控件遮挡刚弹出的下拉菜单。
    lv_obj_move_foreground(filter_list1);

    // 定义 5 个筛选选项文本：年级、班级、性别、分数、全部，覆盖主要筛选维度并提供恢复全部数据的入口。
    const char* texts[] = { "年级", "班级", "性别", "总分", "全部" };
    // 循环遍历 5 个筛选项，逐个创建按钮并复用同一个点击回调，用循环序号 i 区分选项类型。
    for (int i = 0; i < 5; i++) {
        // 在筛选列表中创建一个带文本的按钮项，返回按钮对象供后续绑定事件和设置样式。
        lv_obj_t* btn = lv_list_add_button(filter_list1, NULL, texts[i]);
        // 检查按钮是否成功创建，避免按钮为空时继续调用 LVGL API 导致空指针崩溃。
        if (btn) {
            // 为筛选按钮注册点击回调 on_filter_kind_clicked，点击某个筛选项时触发对应的筛选逻辑。
            lv_obj_add_event_cb(btn, on_filter_kind_clicked, LV_EVENT_CLICKED,
                // 把循环序号 i 转换为指针作为事件用户数据传入，回调收到后可据此判断用户点击的是哪一项筛选。
                (void*)(intptr_t)i);
            // 按钮文字也设置成 UI 自定义字体，确保中文选项在按钮上正常显示。
            lv_obj_set_style_text_font(btn, ui_font(), 0);
            // 让按钮点击事件冒泡到父列表，便于统一处理点击外部收起筛选列表等交互。
            lv_obj_add_flag(btn, LV_OBJ_FLAG_EVENT_BUBBLE);
        }
    }
}

/* ==================== 九、新增学生与闪烁提示 ==================== */

static lv_timer_t* add_blink_timer = NULL;
static uint64_t add_blink_id = 0;
static int add_blink_phase = 0;

/*
 * 根据闪烁阶段，把新增行中未填写/格式错误的单元格设为红或白。
 *
 * 主要逻辑：
 *   1. 用唯一学号定位学生，防止分数重排后数组下标变化
 *   2. 如果已经填了不及格分数，整行红色优先，不再闪烁
 *   3. 总分、绩点、排名由系统自动计算，不参与闪烁
 */
static void ui_apply_new_student_blink(bool red_on)
{
    if (add_blink_id == 0) return;

    int total = data_get_count();
    StudentCSV* all = data_get_all();
    if (all == NULL || total <= 0) return;

    // 用唯一学号定位学生，防止分数重排后数组下标变化
    int student_index = -1;
    for (int i = 0; i < total; i++) {
        if (all[i].id == add_blink_id) {
            student_index = i;
            break;
        }
    }
    if (student_index < 0) return;

    // 如果已经填了不及格分数，整行红色优先，不再闪烁
    if ((all[student_index].new_filled_mask & (1u << STU_FIELD_SCORE)) != 0 &&
        all[student_index].score < 60.0f) {
        return;
    }

    int display_row = -1;
    for (int r = 0; r < data_get_display_count() && r < 200; r++) {
        if (data_get_display_index(r) == student_index) {
            display_row = r;
            break;
        }
    }
    if (display_row < 0) return;

    int grid_row = display_row + 1;

    for (int col = 1; col < 12; col++) {
        // 总分、绩点、排名由系统自动计算，不参与闪烁
        if (col == UI_FIELD_SCORE + 1) continue;
        if (col == UI_FIELD_GPA + 1) continue;
        if (col == UI_FIELD_RANK + 1) continue;

        int field = col - 1;

        if (data_validate_student_field(student_index, field)) {
            ui_set_cell_bg(grid_cells[grid_row][col], 0xFFFFFF);
        }
        else {
            ui_set_cell_bg(grid_cells[grid_row][col],
                red_on ? 0xFF0000 : 0xFFFFFF);
        }
    }
}

/*
 * 闪烁定时器：红 1 秒 -> 白 0.5 秒 -> 红 1 秒 -> 白结束。
 *
 * 主要逻辑：
 *   1. 闪烁结束后重新应用不及格整行红色，保证总分为 0 或不及格的新增行仍然标红
 *   2. 调用 ui_apply_new_student_blink()
 *   3. 调用 lv_timer_set_period()
 *   4. 调用 lv_timer_delete()
 *   5. 调用 ui_mark_failing_rows_red()
 */
static void add_blink_timer_cb(lv_timer_t* timer)
{
    add_blink_phase++;

    if (add_blink_phase == 1) {
        ui_apply_new_student_blink(false);
        lv_timer_set_period(timer, 500);
    }
    else if (add_blink_phase == 2) {
        ui_apply_new_student_blink(true);
        lv_timer_set_period(timer, 1000);
    }
    else {
        ui_apply_new_student_blink(false);
        lv_timer_delete(timer);
        add_blink_timer = NULL;
        add_blink_id = 0;
        add_blink_phase = 0;

        // 闪烁结束后重新应用不及格整行红色，保证总分为 0 或不及格的新增行仍然标红
        ui_mark_failing_rows_red();
        lv_obj_invalidate(obj_list);
    }
}

/*
 * 启动新增行闪烁：记录新生学号并创建闪烁定时器。
 *
 * 主要逻辑：
 *   1. 调用 data_get_all()
 *   2. 调用 data_get_count()
 *   3. 调用 lv_timer_delete()
 *   4. 调用 ui_apply_new_student_blink()
 *   5. 调用 lv_timer_create()
 */
static void ui_start_new_student_blink(int student_index)
{
    StudentCSV* all = data_get_all();
    int total = data_get_count();
    if (all == NULL || student_index < 0 || student_index >= total) return;

    if (add_blink_timer) {
        lv_timer_delete(add_blink_timer);
        add_blink_timer = NULL;
    }

    add_blink_id = all[student_index].id;
    add_blink_phase = 0;

    ui_apply_new_student_blink(true);
    add_blink_timer = lv_timer_create(add_blink_timer_cb, 1000, NULL);
}

/*
 * 滚动表格，使指定学生的行进入可视区域。
 *
 * 主要逻辑：
 *   1. 循环处理：int r = 0; r < data_get_display_count(
 *   2. 调用 data_get_display_index()
 *   3. 调用 lv_obj_scroll_to_view()
 */
static void ui_scroll_to_student_index(int index)
{
    for (int r = 0; r < data_get_display_count() && r < 200; r++) {
        if (data_get_display_index(r) == index) {
            lv_obj_scroll_to_view(grid_cells[r + 1][1], LV_ANIM_ON);
            return;
        }
    }
}

/*
 * 分数低于 60.0 的学生，整行单元格背景标红。
 *
 * 主要逻辑：
 *   1. 调用 data_get_all()
 *   2. 调用 data_get_count()
 *   3. 循环处理：int r = 0; r < data_get_display_count(
 *   4. 调用 data_get_display_index()
 *   5. 循环处理：int col = 0; col < 12; col++
 */
static void ui_mark_failing_rows_red(void)
{
    StudentCSV* all = data_get_all();
    int total = data_get_count();
    if (all == NULL || total <= 0) return;

    for (int r = 0; r < data_get_display_count() && r < 200; r++) {
        int idx = data_get_display_index(r);
        if (idx < 0 || idx >= total) continue;

        if (all[idx].score < 60.0f) {
            for (int col = 0; col < 12; col++) {
                ui_set_cell_bg(grid_cells[r + 1][col], 0xFF0000);
            }
        }
    }
}

/*
 * 把全部单元格恢复白底，并重新标记不及格行为红色。
 *
 * 主要逻辑：
 *   1. 调用 ui_reset_all_cells_white()
 *   2. 调用 ui_mark_failing_rows_red()
 */
static void ui_repaint_cells_base(void)
{
    ui_reset_all_cells_white();
    ui_mark_failing_rows_red();
}

/*
 * 新增按钮点击回调：打开学号输入框，等待用户输入 12 位学号。
 *
 * 主要逻辑：
 *   1. 复用导入/导出的路径输入框，action=3 表示输入学号新增
 *   2. 调用 ui_save_and_close_edit()
 *   3. 调用 ui_close_filter_lists()
 *   4. 调用 ui_open_path_ta()
 */
static void on_add_clicked(lv_event_t* e)
{
    (void)e;

    ui_save_and_close_edit();
    ui_close_filter_lists();

    // 复用导入/导出的路径输入框，action=3 表示输入学号新增
    ui_open_path_ta(3);
}
/* ---------- 9.1 帮助浮层 ---------- */

/*
 * 关闭帮助浏览：删除帮助按钮、释放解码后的像素缓冲并复位帮助状态。
 *
 * 主要逻辑：
 *   1. 若还有延迟加载定时器未触发，先取消，避免关闭后回调继续执行。
 *   2. 加载提示标签是帮助浮层的子控件，随浮层一起删除，这里先清空指针。
 *   3. 删除全屏遮罩，同时会删除它的子控件（帮助图片按钮和提示标签）。
 *   4. 释放缩放后的像素缓冲，避免内存泄漏。
 *   5. 复位轮播状态，下次打开帮助时从第 1 张开始。
 */
static void ui_help_close(void)
{
    // 若还有延迟加载定时器未触发，先取消，避免关闭后回调继续执行。
    if (help_load_timer != NULL) {
        lv_timer_delete(help_load_timer);
        help_load_timer = NULL;
    }
    // 加载提示标签是帮助浮层的子控件，随浮层一起删除，这里先清空指针。
    help_loading_label = NULL;

    // 删除全屏遮罩，同时会删除它的子控件（帮助图片按钮和提示标签）。
    if (help_backdrop != NULL) {
        lv_obj_delete(help_backdrop);
        help_backdrop = NULL;
        help_imagebtn = NULL;
    }
    else if (help_imagebtn != NULL) {
        lv_obj_delete(help_imagebtn);
        help_imagebtn = NULL;
    }

    // 释放缩放后的像素缓冲，避免内存泄漏。
    if (help_image_pixels != NULL) {
        free(help_image_pixels);
        help_image_pixels = NULL;
    }

    // 复位轮播状态，下次打开帮助时从第 1 张开始。
    help_index = 0;
    help_count = 0;
}

/*
 * 通过 LVGL 文件系统读取第 index 张帮助图片文件到内存。
 *
 * 主要逻辑：
 *   1. 取得帮助图片路径（形如 A:help_png/help1.png），由 core 层统一生成。
 *   2. 使用 LVGL 的 FS 驱动打开文件，这样路径规则与图标加载保持一致。
 *   3. 先获取文件大小。个别 FS 实现没有 size 接口时，退回到 seek 到文件末尾再 tell。
 *   4. 用系统堆分配 PNG 文件缓冲；PNG 本体通常只有几 MB，真正的开销在解码后的像素。
 *   5. 回到文件开头，完整读取 PNG 数据。
 */
static int ui_help_read_file(int index, uint8_t** out_buf, uint32_t* out_size)
{
    // 取得帮助图片路径（形如 A:help_png/help1.png），由 core 层统一生成。
    const char* path = (const char*)ui_help_get_image_src(index);
    if (path == NULL) return -1;

    // 使用 LVGL 的 FS 驱动打开文件，这样路径规则与图标加载保持一致。
    lv_fs_file_t file;
    lv_fs_res_t res = lv_fs_open(&file, path, LV_FS_MODE_RD);
    if (res != LV_FS_RES_OK) {
        fprintf(stderr, "[help] open failed: %s (res=%d)\n", path, (int)res);
        return -2;
    }

    // 先获取文件大小。个别 FS 实现没有 size 接口时，退回到 seek 到文件末尾再 tell。
    uint32_t size = 0;
    res = lv_fs_get_size(&file, &size);
    if (res != LV_FS_RES_OK || size == 0) {
        res = lv_fs_seek(&file, 0, LV_FS_SEEK_END);
        if (res == LV_FS_RES_OK) {
            uint32_t pos = 0;
            res = lv_fs_tell(&file, &pos);
            if (res == LV_FS_RES_OK) size = pos;
        }
    }
    if (res != LV_FS_RES_OK || size == 0) {
        lv_fs_close(&file);
        fprintf(stderr, "[help] get size failed: %s (res=%d)\n", path, (int)res);
        return -3;
    }

    // 用系统堆分配 PNG 文件缓冲；PNG 本体通常只有几 MB，真正的开销在解码后的像素。
    uint8_t* buf = (uint8_t*)malloc(size);
    if (buf == NULL) {
        lv_fs_close(&file);
        fprintf(stderr, "[help] out of memory for %u bytes: %s\n", (unsigned)size, path);
        return -4;
    }

    // 回到文件开头，完整读取 PNG 数据。
    res = lv_fs_seek(&file, 0, LV_FS_SEEK_SET);
    uint32_t br = 0;
    if (res == LV_FS_RES_OK) res = lv_fs_read(&file, buf, size, &br);
    lv_fs_close(&file);

    if (res != LV_FS_RES_OK || br != size) {
        free(buf);
        fprintf(stderr, "[help] read failed: %s (res=%d, br=%u/%u)\n",
            path, (int)res, (unsigned)br, (unsigned)size);
        return -5;
    }

    *out_buf = buf;
    *out_size = size;
    return 0;
}

/*
 * 区域平均法缩放 RGBA 原图：目标图像素取源图对应小块的 RGBA 平均值。
 *
 * 主要逻辑：
 *   1. 计算目标行对应的源图行区间 [sy0, sy1)，用 +dh-1 保证向上取整且不越界。
 *   2. 计算目标列对应的源图列区间 [sx0, sx1)。
 *   3. LodePNG 输出顺序为 R,G,B,A；LVGL 的 ARGB8888 在内存中是 B,G,R,A，需交换红蓝。
 */
static uint8_t* ui_help_downscale_rgba(const uint8_t* src, int sw, int sh, int dw, int dh)
{
    uint8_t* dst = (uint8_t*)malloc((size_t)dw * (size_t)dh * 4);
    if (dst == NULL) return NULL;

    for (int y = 0; y < dh; y++) {
        // 计算目标行对应的源图行区间 [sy0, sy1)，用 +dh-1 保证向上取整且不越界。
        int sy0 = (int)((int64_t)y * sh / dh);
        int sy1 = (int)(((int64_t)(y + 1) * sh + dh - 1) / dh);
        if (sy1 <= sy0) sy1 = sy0 + 1;
        if (sy1 > sh) sy1 = sh;

        for (int x = 0; x < dw; x++) {
            // 计算目标列对应的源图列区间 [sx0, sx1)。
            int sx0 = (int)((int64_t)x * sw / dw);
            int sx1 = (int)(((int64_t)(x + 1) * sw + dw - 1) / dw);
            if (sx1 <= sx0) sx1 = sx0 + 1;
            if (sx1 > sw) sx1 = sw;

            uint32_t r = 0, g = 0, b = 0, a = 0, n = 0;
            for (int sy = sy0; sy < sy1; sy++) {
                const uint8_t* row = src + ((size_t)sy * (size_t)sw + sx0) * 4;
                for (int sx = sx0; sx < sx1; sx++, row += 4) {
                    r += row[0];
                    g += row[1];
                    b += row[2];
                    a += row[3];
                    n++;
                }
            }

            // LodePNG 输出顺序为 R,G,B,A；LVGL 的 ARGB8888 在内存中是 B,G,R,A，需交换红蓝。
            uint8_t* d = dst + ((size_t)y * (size_t)dw + x) * 4;
            d[0] = (uint8_t)(b / n);
            d[1] = (uint8_t)(g / n);
            d[2] = (uint8_t)(r / n);
            d[3] = (uint8_t)(a / n);
        }
    }

    return dst;
}

/*
 * 读取并解码第 index 张帮助图片，必要时等比缩小到屏幕大小以内。
 *
 * 主要逻辑：
 *   1. 第一步：把 PNG 文件内容读入内存。
 *   2. 校验文件头，确保是真正的 PNG。
 *   3. 之前 help1.png 实际是改了扩展名的 JPEG，LodePNG 会报“incorrect PNG signature”，
 *   4. 这里提前给出更明确的错误信息，便于替换成真正的 PNG。
 *   5. 第二步：用 lodepng 解码。
 *   6. 注意：LVGL 内置的 lodepng 经过改造，lodepng_decode32 的输出实际上是
 */
static int ui_help_load_scaled(int index, uint8_t** out_pixels, int* out_w, int* out_h)
{
    // 第一步：把 PNG 文件内容读入内存。
    uint8_t* png = NULL;
    uint32_t png_size = 0;
    int err = ui_help_read_file(index, &png, &png_size);
    if (err != 0) return err;

    // 校验文件头，确保是真正的 PNG。
    // 之前 help1.png 实际是改了扩展名的 JPEG，LodePNG 会报“incorrect PNG signature”，
    // 这里提前给出更明确的错误信息，便于替换成真正的 PNG。
    static const uint8_t png_magic[8] = { 0x89, 'P', 'N', 'G', 0x0D, 0x0A, 0x1A, 0x0A };
    if (png_size < 8 || memcmp(png, png_magic, 8) != 0) {
        fprintf(stderr, "[help] not a PNG file: %s\n",
            (const char*)ui_help_get_image_src(index));
        free(png);
        return -12;
    }

    // 第二步：用 lodepng 解码。
    // 注意：LVGL 内置的 lodepng 经过改造，lodepng_decode32 的输出实际上是
    // 一个 lv_draw_buf_t*，像素数据在它的 data 字段中，而不是直接的像素指针，
    // 因此后续不能用 free() 释放，必须用 lv_draw_buf_destroy()。
    unsigned w = 0, h = 0;
    unsigned char* decoded = NULL;
    unsigned lerr = lodepng_decode32(&decoded, &w, &h, png, png_size);
    free(png);
    if (lerr != 0 || decoded == NULL) {
        fprintf(stderr, "[help] png decode failed (err=%u)\n", lerr);
        if (decoded != NULL) lv_draw_buf_destroy((lv_draw_buf_t*)decoded);
        return -10;
    }

    // draw_buf->data 中仍然是 LodePNG 的 R,G,B,A 顺序，需要转换成 LVGL ARGB8888 的 B,G,R,A。
    lv_draw_buf_t* draw_buf = (lv_draw_buf_t*)decoded;
    const uint8_t* rgba = (const uint8_t*)draw_buf->data;

    // 第三步：若原图超出屏幕，则等比缩小；否则保持原始尺寸。
    int dw = (int)w;
    int dh = (int)h;
    if (dw > 1080 || dh > 720) {
        float scale_x = (float)1080 / (float)dw;
        float scale_y = (float)720 / (float)dh;
        float scale = (scale_x < scale_y) ? scale_x : scale_y;
        dw = (int)((float)dw * scale);
        dh = (int)((float)dh * scale);
        if (dw < 1) dw = 1;
        if (dh < 1) dh = 1;
    }

    uint8_t* pixels = NULL;
    if (dw == (int)w && dh == (int)h) {
        // 原图尺寸已经在屏幕范围内：复制一份并交换红蓝字节，得到普通 malloc 的 B,G,R,A 缓冲。
        size_t count = (size_t)w * (size_t)h;
        pixels = (uint8_t*)malloc(count * 4);
        if (pixels != NULL) {
            for (size_t i = 0; i < count; i++) {
                pixels[i * 4 + 0] = rgba[i * 4 + 2];
                pixels[i * 4 + 1] = rgba[i * 4 + 1];
                pixels[i * 4 + 2] = rgba[i * 4 + 0];
                pixels[i * 4 + 3] = rgba[i * 4 + 3];
            }
        }
    }
    else {
        // 大图：区域平均缩小，函数内部同时完成 R,G,B,A -> B,G,R,A 的转换。
        pixels = ui_help_downscale_rgba(rgba, (int)w, (int)h, dw, dh);
    }

    // 释放 lodepng 的 draw buffer（包括其中的像素数据）。
    lv_draw_buf_destroy(draw_buf);

    if (pixels == NULL) return -11;

    *out_pixels = pixels;
    *out_w = dw;
    *out_h = dh;
    return 0;
}

/*
 * 加载并显示第 index 张帮助图片；失败时在提示标签上显示错误信息。
 *
 * 主要逻辑：
 *   1. 帮助按钮已被关闭时直接返回，避免访问空指针。
 *   2. 替换旧图前先释放上一张图的像素缓冲。
 *   3. 构造 LVGL 图片描述符，data 指向常驻缓冲；按钮显示期间该缓冲必须一直有效。
 *   4. “松开”和“按下”两种状态使用同一张图片，按压时不会出现空白。
 *   5. lv_imagebutton 会把中间图片平铺到控件区域，因此把控件尺寸设为图片实际尺寸并居中，
 *   6. 这样图片刚好铺满按钮，不会被重复平铺，也保证整张帮助图可见。
 */
static void ui_set_help_image(int index)
{
    // 帮助按钮已被关闭时直接返回，避免访问空指针。
    if (help_imagebtn == NULL) return;

    uint8_t* pixels = NULL;
    int w = 0, h = 0;
    int err = ui_help_load_scaled(index, &pixels, &w, &h);
    if (err != 0) {
        if (help_loading_label != NULL) {
            lv_label_set_text(help_loading_label, "帮助图片读取失败\n请确认 bin/help_png/help1.png 是 PNG 格式");
            lv_obj_set_style_text_color(help_loading_label, lv_color_hex(0xFF0000), 0);
        }
        return;
    }

    // 替换旧图前先释放上一张图的像素缓冲。
    if (help_image_pixels != NULL) {
        free(help_image_pixels);
        help_image_pixels = NULL;
    }
    help_image_pixels = pixels;

    // 构造 LVGL 图片描述符，data 指向常驻缓冲；按钮显示期间该缓冲必须一直有效。
    lv_memzero(&help_image_dsc, sizeof(help_image_dsc));
    help_image_dsc.header.magic = LV_IMAGE_HEADER_MAGIC;
    help_image_dsc.header.cf = LV_COLOR_FORMAT_ARGB8888;
    help_image_dsc.header.w = (uint32_t)w;
    help_image_dsc.header.h = (uint32_t)h;
    help_image_dsc.header.stride = (uint32_t)w * 4;
    help_image_dsc.data_size = (uint32_t)w * (uint32_t)h * 4;
    help_image_dsc.data = pixels;

    // “松开”和“按下”两种状态使用同一张图片，按压时不会出现空白。
    lv_imagebutton_set_src(help_imagebtn, LV_IMAGEBUTTON_STATE_RELEASED, NULL, &help_image_dsc, NULL);
    lv_imagebutton_set_src(help_imagebtn, LV_IMAGEBUTTON_STATE_PRESSED, NULL, &help_image_dsc, NULL);

    // lv_imagebutton 会把中间图片平铺到控件区域，因此把控件尺寸设为图片实际尺寸并居中，
    // 这样图片刚好铺满按钮，不会被重复平铺，也保证整张帮助图可见。
    lv_obj_set_size(help_imagebtn, w, h);
    lv_obj_center(help_imagebtn);

    // 图片已显示，隐藏“正在加载”提示。
    if (help_loading_label != NULL) {
        lv_obj_add_flag(help_loading_label, LV_OBJ_FLAG_HIDDEN);
    }
}

/*
 * 延迟加载定时器回调：提示文字绘制一帧后再执行解码，避免点击后界面像卡死一样没有反馈。
 *
 * 主要逻辑：
 *   1. 该定时器是一次性的，触发后会被 LVGL 自动删除，这里只需清空全局指针。
 *   2. 用户在加载完成前点击关闭时，帮助浮层已被删除，不再继续加载。
 */
static void on_help_load_timer(lv_timer_t* timer)
{
    (void)timer;
    // 该定时器是一次性的，触发后会被 LVGL 自动删除，这里只需清空全局指针。
    help_load_timer = NULL;
    // 用户在加载完成前点击关闭时，帮助浮层已被删除，不再继续加载。
    if (help_backdrop == NULL || help_imagebtn == NULL) return;
    ui_set_help_image(help_index);
}

/*
 * 点击帮助图片按钮的回调：有多张图时切换到下一张，最后一张点击后关闭帮助。
 *
 * 主要逻辑：
 *   1. 调用 ui_help_close()
 *   2. 调用 ui_set_help_image()
 */
static void on_help_image_clicked(lv_event_t* e)
{
    (void)e;
    if (help_backdrop == NULL) return;

    help_index++;
    if (help_index >= help_count) {
        ui_help_close();
    }
    else {
        ui_set_help_image(help_index);
    }
}

/*
 * 点击顶部“帮助”按钮的回调：打开全屏帮助浮层并异步加载 help 图片。
 *
 * 主要逻辑：
 *   1. 打开帮助前先保存并关闭正在编辑的单元格，避免编辑浮层与帮助图片相互遮挡或状态冲突。
 *   2. 帮助已经打开时重复点击直接忽略，防止堆叠多个帮助浮层。
 *   3. 取得帮助图片数量；当前 core 层配置为 1 张。
 *   4. 创建覆盖整个屏幕的半透明遮罩：点击遮罩任意位置关闭帮助。
 *   5. 遮罩自身不滚动，也不参与键盘/编码器焦点分组，避免影响表格横向滚动位置。
 *   6. 把帮助浮层移到最前，确保它显示在所有界面控件之上。
 */
static void on_help_clicked(lv_event_t* e)
{
    (void)e;
    // 打开帮助前先保存并关闭正在编辑的单元格，避免编辑浮层与帮助图片相互遮挡或状态冲突。
    ui_save_and_close_edit();

    // 帮助已经打开时重复点击直接忽略，防止堆叠多个帮助浮层。
    if (help_backdrop != NULL) return;

    // 取得帮助图片数量；当前 core 层配置为 1 张。
    help_count = ui_help_get_image_count();
    if (help_count <= 0) {
        ui_show_toast("未找到帮助图片");
        return;
    }

    help_index = 0;

    // 创建覆盖整个屏幕的半透明遮罩：点击遮罩任意位置关闭帮助。
    help_backdrop = lv_obj_create(lv_screen_active());
    lv_obj_set_pos(help_backdrop, 0, 0);
    lv_obj_set_size(help_backdrop, 1080, 720);
    // 遮罩自身不滚动，也不参与键盘/编码器焦点分组，避免影响表格横向滚动位置。
    lv_obj_remove_flag(help_backdrop, LV_OBJ_FLAG_SCROLLABLE);
    lv_obj_remove_flag(help_backdrop, LV_OBJ_FLAG_CLICK_FOCUSABLE);
    lv_group_remove_obj(help_backdrop);

    lv_obj_set_style_bg_color(help_backdrop, lv_color_black(), 0);
    lv_obj_set_style_bg_opa(help_backdrop, LV_OPA_90, 0);
    lv_obj_set_style_border_width(help_backdrop, 0, 0);
    lv_obj_set_style_radius(help_backdrop, 0, 0);
    lv_obj_set_style_pad_all(help_backdrop, 0, 0);

    // 把帮助浮层移到最前，确保它显示在所有界面控件之上。
    lv_obj_move_foreground(help_backdrop);
    // 点击遮罩空白处关闭帮助；点击图片时事件冒泡到这里，同样关闭。
    lv_obj_add_event_cb(help_backdrop, on_help_image_clicked, LV_EVENT_CLICKED, NULL);

    // 图片按钮作为遮罩的子控件，先占满屏幕；图片解码完成后再调整为图片实际尺寸并居中。
    // lv_imagebutton 会把中间图片平铺到自身区域，所以按钮尺寸必须与图片尺寸一致。
    help_imagebtn = lv_imagebutton_create(help_backdrop);
    lv_obj_set_pos(help_imagebtn, 0, 0);
    lv_obj_set_size(help_imagebtn, 1080, 720);
    lv_obj_remove_flag(help_imagebtn, LV_OBJ_FLAG_SCROLLABLE);
    lv_obj_remove_flag(help_imagebtn, LV_OBJ_FLAG_CLICK_FOCUSABLE);
    lv_group_remove_obj(help_imagebtn);
    // 点击图片时事件冒泡给遮罩，由遮罩统一处理关闭/切换。
    lv_obj_add_flag(help_imagebtn, LV_OBJ_FLAG_EVENT_BUBBLE);
    // 图片按钮本身透明，只显示图片内容，让遮罩的黑色底作为背景。
    lv_obj_set_style_bg_opa(help_imagebtn, LV_OPA_TRANSP, 0);
    lv_obj_set_style_bg_opa(help_imagebtn, LV_OPA_TRANSP, LV_STATE_PRESSED);
    lv_obj_set_style_border_width(help_imagebtn, 0, 0);
    lv_obj_set_style_radius(help_imagebtn, 0, 0);
    lv_obj_set_style_pad_all(help_imagebtn, 0, 0);
    lv_obj_set_style_shadow_width(help_imagebtn, 0, 0);

    // 先创建“正在加载”提示，再延迟 50ms 解码大图。
    help_loading_label = lv_label_create(help_backdrop);
    lv_label_set_text(help_loading_label, "正在加载帮助图片…");
    lv_obj_set_style_text_color(help_loading_label, lv_color_white(), 0);
    lv_obj_set_style_text_font(help_loading_label, ui_font(), 0);
    lv_obj_center(help_loading_label);

    help_load_timer = lv_timer_create(on_help_load_timer, 50, NULL);
    if (help_load_timer != NULL) {
        lv_timer_set_repeat_count(help_load_timer, 1);
    }
}

/*
 * 对外接口：模拟点击顶部“帮助”按钮；成功打开帮助返回 1，否则返回 0。
 *
 * 主要逻辑：
 *   1. 帮助浮层已经打开时直接按成功处理。
 *   2. 复用帮助按钮的点击回调，事件参数传 NULL 即可。
 *   3. 返回帮助浮层是否创建成功。
 */
int ui_show_help(void)
{
    // 帮助浮层已经打开时直接按成功处理。
    if (help_imagebtn != NULL) return 1;
    // 复用帮助按钮的点击回调，事件参数传 NULL 即可。
    on_help_clicked(NULL);
    // 返回帮助浮层是否创建成功。
    return (help_imagebtn != NULL) ? 1 : 0;
}

/* ---------- 9.2 导入 / 导出 ---------- */

/*
 * 路径输入框回车事件回调签名；用户在导入/导出路径框中按回车后触发。
 *
 * 主要逻辑：
 *   1. 忽略 LVGL 事件参数，路径处理只需要文本框内容和全局 action 标识。
 *   2. 若路径输入框已不存在则直接返回，防止读取已释放控件的文本。
 *   3. 在栈上分配 1024 字节路径缓冲区，足以容纳常见 CSV 文件路径并留出结尾空字符空间。
 *   4. 先把全局操作类型保存到局部 action，因为下面会删除输入框并清空全局值，后续分支仍需要此类型。
 *   5. 从当前路径输入框中读取用户输入的路径到 path 缓冲区，函数内部会做长度限制，避免越界。
 *   6. ===== 新增：规范化用户输入的路径 =====
 */
static void on_path_ready(lv_event_t* e)
{
    // 忽略 LVGL 事件参数，路径处理只需要文本框内容和全局 action 标识。
    (void)e;
    // 若路径输入框已不存在则直接返回，防止读取已释放控件的文本。
    if (path_ta == NULL) return;

    // 在栈上分配 1024 字节路径缓冲区，足以容纳常见 CSV 文件路径并留出结尾空字符空间。
    char path[1024];
    // 先把全局操作类型保存到局部 action，因为下面会删除输入框并清空全局值，后续分支仍需要此类型。
    int action = path_action;

    // 从当前路径输入框中读取用户输入的路径到 path 缓冲区，函数内部会做长度限制，避免越界。
    ui_get_ta_text(path_ta, path, sizeof(path));
    // ===== 新增：规范化用户输入的路径 =====
    {
        size_t plen = strlen(path);

        // 1) 去掉末尾可能带进来的空白
        while (plen > 0 &&
            (path[plen - 1] == '\r' || path[plen - 1] == '\n' ||
                path[plen - 1] == ' ' || path[plen - 1] == '\t')) {
            path[--plen] = '\0';
        }

        // 2) 去掉资源管理器“复制文件地址”带进来的成对双引号
        if (plen >= 2 && path[0] == '"' && path[plen - 1] == '"') {
            memmove(path, path + 1, plen - 2);
            path[plen - 2] = '\0';
        }

        // 3) 把 C:Users\... 补成 C:\Users\...
        plen = strlen(path);
        if (plen >= 3 && plen + 1 < sizeof(path) &&
            path[1] == ':' && path[2] != '\\' && path[2] != '/') {
            memmove(path + 3, path + 2, plen - 2 + 1);  // 连 '\0' 一起后移
            path[2] = '\\';
        }
    }

    // 回车后删除路径输入框，完成一次路径输入并释放对应 UI 控件。
    lv_obj_delete(path_ta);
    // 把全局 path_ta 置空，避免引用已删除对象，并标识当前没有活动路径输入框。
    path_ta = NULL;
    // 清空全局 path_action，防止下一次打开输入框前误用旧的导入/导出操作类型。
    path_action = 0;

    // 若保存下来的 action 为 1，表示用户执行的是 CSV 导入操作。
    if (action == 1) {
        // 用户输入的是文件夹地址（空=桌面）：自动查找最新的 students_*.csv 导入。
        char resolved[1200];

        if (!data_resolve_import_path(path, resolved, sizeof(resolved))) {
            ui_show_toast("未找到 students_*.csv，请检查文件夹");
        }
        else {
            // 调用数据层函数导入解析出的 CSV 文件；r 为成功导入的条数，负数表示失败。
            int r = data_import_csv(resolved);
            // 导入后立即刷新网格内部数据视图。
            ui_refresh_grid_internal();

            if (r >= 0) {
                char msg[256];
                snprintf(msg, sizeof(msg), "导入成功：%d 条数据", r);
                ui_show_toast(msg);
            }
            else {
                ui_show_toast("导入失败，请检查文件是否可读");
            }
        }
    }
    // 若 action 为 2，表示用户执行的是 CSV 导出操作。
    else if (action == 2) {
        // 根据输入地址自动生成导出路径：空=桌面，文件名 students_时间.csv。
        char out_path[1200];

        if (!data_resolve_export_path(path, 0, out_path, sizeof(out_path))) {
            ui_show_toast("无法确定保存位置");
        }
        else {
            int r = data_export_csv(out_path);

            if (r >= 0) {
                char msg[1200];
                snprintf(msg, sizeof(msg), "文件已导出至'%s'", out_path);
                ui_show_toast(msg);
            }
            else {
                ui_show_toast("导出失败，请检查文件夹是否存在");
            }
        }
    }
    // action == 3：输入学号新增学生
    else if (action == 3) {
        int r = data_add_student_by_id(path);

        if (r >= 0) {
            ui_close_filter_lists();
            data_clear_filter();

            ui_refresh_grid_internal();
            ui_scroll_to_student_index(r);
            ui_start_new_student_blink(r);

            ui_show_toast("新增成功，请填写闪烁单元格");
        }
        else if (r == -2) {
            ui_open_path_ta(3);
            ui_show_toast("学号已存在，请重新输入");
        }
        else if (r == -3) {
            ui_show_toast("学生数量已达上限");
        }
        else if (r == -4) {
            show_invalid_id_popup();
        }
        else {
            ui_open_path_ta(3);
            ui_show_toast("学号格式错误，请输入数字");
        }
    }
    // action == 4/5/6：输入平时/期中/期末权重（%）
    else if (action >= 4 && action <= 6) {
        int which = action - 4;  // 0=平时, 1=期中, 2=期末

        if (!data_set_weight_from_text(which, path)) {
            ui_open_path_ta(action);
            ui_show_toast("权重格式错误，请输入0~100的数字");
        }
        else {
            ui_apply_weight_change();
        }
    }
    // action == 7：一次输入平时/期中/期末三项权重
    else if (action == 7) {
        if (!data_set_weights_from_text(path)) {
            ui_open_path_ta(7);
            ui_show_toast("请输入3个0~100的权重，例如：30 20 50");
        }
        else {
            ui_apply_weight_change();
        }
    }
    // action == 8：导出按班级统计结果 TXT
    else if (action == 8) {
        // 根据输入地址自动生成导出路径：空=桌面，文件名 students_时间.txt。
        char out_path[1200];

        if (!data_resolve_export_path(path, 1, out_path, sizeof(out_path))) {
            ui_show_toast("无法确定保存位置");
        }
        else {
            int r = data_export_statistics(out_path);

            if (r >= 0) {
                char msg[1200];
                snprintf(msg, sizeof(msg), "统计结果已导出至'%s'", out_path);
                ui_show_toast(msg);
            }
            else {
                ui_show_toast("导出失败，请检查文件夹是否存在");
            }
        }
    }
}

/*
 * 创建路径输入框的函数签名；center_x 为触发按钮中心 X，action 表示本次操作是导入(1)还是导出(2)。
 *
 * 主要逻辑：
 *   1. 打开路径输入框前先保存并关闭当前编辑，避免输入框和表格编辑同时占用界面焦点。
 *   2. 同一时间只保留一个路径输入框
 *   3. 若已存在路径输入框，先删除旧框，保证同一时间只有一个路径输入框。
 *   4. 销毁旧的路径输入框控件，清理上一次输入界面。
 *   5. 将旧输入框全局指针置空，之后创建新输入框时不会引用已删除对象。
 *   6. 把本次操作类型存入全局 path_action，供回车回调在 on_path_ready 中判断导入或导出
 */
static void ui_open_path_ta(int action)
{
    // 打开路径输入框前先保存并关闭当前编辑，避免输入框和表格编辑同时占用界面焦点。
    ui_save_and_close_edit();

    // 同一时间只保留一个路径输入框
    // 若已存在路径输入框，先删除旧框，保证同一时间只有一个路径输入框。
    if (path_ta) {
        // 销毁旧的路径输入框控件，清理上一次输入界面。
        lv_obj_delete(path_ta);
        // 将旧输入框全局指针置空，之后创建新输入框时不会引用已删除对象。
        path_ta = NULL;
    }

    // 把本次操作类型存入全局 path_action，供回车回调在 on_path_ready 中判断导入或导出
    path_action = action;
    // 在当前活动屏幕上创建单行文本框并存入全局 path_ta，作为路径输入控件
    path_ta = lv_textarea_create(lv_screen_active());
    // 让输入框展示在(400,10)处
    lv_obj_set_pos(path_ta, 450, 30);
    // 设置 2 像素边框。
    lv_obj_set_style_border_width(path_ta, 2, 0);
    // 设置边框颜色
    lv_obj_set_style_border_color(path_ta, lv_color_hex(0x333333), 0);
    // 清除全部内边距，保证内边缘为 0。
    lv_obj_set_style_pad_all(path_ta, 0, 0);
    // 设置输入框宽高，保证导入和导出弹出框尺寸一致。
    lv_obj_set_size(path_ta, 350, 60);
    // 把文本框设为单行模式，因为路径不应换行，回车键专门用于提交路径。
    lv_textarea_set_one_line(path_ta, true);
    // 限制最大输入长度为 1023 字符，与后面 1024 字节缓冲区配合防止路径溢出。
    lv_textarea_set_max_length(path_ta, 1023);
    // 按 action 显示不同占位提示
    if (action == 1) {
        lv_textarea_set_placeholder_text(path_ta,
            "输入文件夹路径，自动导入最新 students_*.csv");
    }
    else if (action == 2) {
        lv_textarea_set_placeholder_text(path_ta,
            "输入文件夹路径，自动命名导出 CSV");
    }
    else if (action == 3) {
        lv_textarea_set_placeholder_text(path_ta, "输入学号，回车新增");
    }
    else if (action == 4) {
        lv_textarea_set_placeholder_text(path_ta, "输入平时权重（%），回车确认");
    }
    else if (action == 5) {
        lv_textarea_set_placeholder_text(path_ta, "输入期中权重（%），回车确认");
    }
    else if (action == 6) {
        lv_textarea_set_placeholder_text(path_ta, "输入期末权重（%），回车确认");
    }
    else if (action == 7) {
        lv_textarea_set_placeholder_text(path_ta,
            "输入平时 期中 期末权重（%），用空格/逗号/斜杠分隔，如 30 20 50");
    }
    else if (action == 8) {
        lv_textarea_set_placeholder_text(path_ta,
            "输入文件夹路径，自动命名导出 TXT");
    }
    else {
        lv_textarea_set_placeholder_text(path_ta, "输入内容，回车确认");
    }
    // 允许点击文本框任意位置移动光标，方便修改路径中间内容。
    lv_textarea_set_cursor_click_pos(path_ta, true);
    // 文本框正文使用 UI 字体，保证中文路径和提示文字正常渲染。
    lv_obj_set_style_text_font(path_ta, ui_font(), 0);
    // 占位符文本也设置为 UI 字体，避免默认字体显示中文占位提示时出现乱码。
    lv_obj_set_style_text_font(path_ta, ui_font(), LV_PART_TEXTAREA_PLACEHOLDER);
    // 给输入框加事件冒泡标志，使其与程序中点击外部收起/保存等统一交互兼容。
    lv_obj_add_flag(path_ta, LV_OBJ_FLAG_EVENT_BUBBLE);
    // 把路径输入框移到最前，避免被顶部按钮或其他控件遮挡。
    lv_obj_move_foreground(path_ta);
    // 给文本框绑定 LV_EVENT_READY（单行文本框回车）事件到 on_path_ready，实现回车后立即导入或导出。
    lv_obj_add_event_cb(path_ta, on_path_ready, LV_EVENT_READY, NULL);

    // 若系统键盘分组 kb_group 存在，说明程序启用了实体/虚拟键盘输入支持，需要把输入框纳入分组。
    if (kb_group) {
        // 把路径输入框加入键盘焦点分组，使键盘可在不同输入控件间切换焦点。
        lv_group_add_obj(kb_group, path_ta);
        // 让路径输入框立即获得焦点，用户打开后可直接开始输入路径。
        lv_group_focus_obj(path_ta);
        // 把键盘分组设为编辑模式，让键盘按键直接输入到文本框而不是用于导航。
        lv_group_set_editing(kb_group, true);
    }
}

/* ---------- 9.3 导出方式选择弹窗 ---------- */

/*
 * 在弹窗内创建一个带位置和颜色的统一样式按钮。
 *
 * 主要逻辑：
 *   1. 调用 lv_button_create()
 *   2. 调用 lv_obj_set_pos()
 *   3. 调用 lv_obj_set_size()
 *   4. 调用 lv_obj_set_style_bg_color()
 *   5. 调用 lv_color_hex()
 */
static lv_obj_t* ui_make_popup_button(lv_obj_t* parent, const char* text,
    int x, int y, int w, int h, uint32_t color, lv_event_cb_t cb)
{
    lv_obj_t* btn = lv_button_create(parent);
    lv_obj_set_pos(btn, x, y);
    lv_obj_set_size(btn, w, h);
    lv_obj_set_style_bg_color(btn, lv_color_hex(color), 0);
    lv_obj_set_style_bg_opa(btn, LV_OPA_COVER, 0);
    lv_obj_set_style_text_color(btn, lv_color_hex(0xFFFFFF), 0);
    lv_obj_set_style_text_font(btn, ui_font(), 0);
    lv_obj_set_style_pad_all(btn, 0, 0);

    lv_obj_t* label = lv_label_create(btn);
    lv_label_set_text(label, text);
    lv_obj_set_style_text_font(label, ui_font(), 0);
    lv_obj_center(label);

    lv_obj_add_event_cb(btn, cb, LV_EVENT_CLICKED, NULL);
    lv_obj_add_flag(btn, LV_OBJ_FLAG_EVENT_BUBBLE);
    lv_group_remove_obj(btn);
    return btn;
}

/*
 * 关闭导出方式选择弹窗。
 *
 * 主要逻辑：
 *   1. 调用 ui_close_popup()
 */
static void ui_close_export_choice(void)
{
    ui_close_popup(&export_choice_popup);
}

/*
 * 选择“导出学生成绩表（CSV）”。
 *
 * 主要逻辑：
 *   1. 复用原来的 CSV 导出路径输入流程
 *   2. 调用 ui_close_export_choice()
 *   3. 调用 ui_open_path_ta()
 */
static void on_export_choice_csv(lv_event_t* e)
{
    (void)e;
    ui_close_export_choice();
    // 复用原来的 CSV 导出路径输入流程
    ui_open_path_ta(2);
}

/*
 * 选择“导出统计结果（TXT）”。
 *
 * 主要逻辑：
 *   1. action=8 表示按班级导出统计结果 TXT
 *   2. 调用 ui_close_export_choice()
 *   3. 调用 ui_open_path_ta()
 */
static void on_export_choice_txt(lv_event_t* e)
{
    (void)e;
    ui_close_export_choice();
    // action=8 表示按班级导出统计结果 TXT
    ui_open_path_ta(8);
}

/*
 * 取消导出。
 *
 * 主要逻辑：
 *   1. 调用 ui_close_export_choice()
 */
static void on_export_choice_cancel(lv_event_t* e)
{
    (void)e;
    ui_close_export_choice();
}

/*
 * 创建一个统一样式的居中弹窗。
 *
 * 主要逻辑：
 *   1. 调用 lv_obj_create()
 *   2. 调用 lv_screen_active()
 *   3. 调用 lv_obj_set_size()
 *   4. 调用 lv_obj_center()
 *   5. 调用 lv_obj_set_style_pad_all()
 */
static lv_obj_t* ui_create_popup(int w, int h)
{
    lv_obj_t* popup = lv_obj_create(lv_screen_active());
    lv_obj_set_size(popup, w, h);
    lv_obj_center(popup);
    lv_obj_set_style_pad_all(popup, 0, 0);
    lv_obj_set_style_bg_color(popup, lv_color_hex(0xFFFFFF), 0);
    lv_obj_set_style_bg_opa(popup, LV_OPA_COVER, 0);
    lv_obj_set_style_border_width(popup, 2, 0);
    lv_obj_set_style_border_color(popup, lv_color_hex(0x666666), 0);
    lv_obj_set_style_radius(popup, 8, 0);
    lv_obj_add_flag(popup, LV_OBJ_FLAG_EVENT_BUBBLE);
    lv_obj_move_foreground(popup);
    return popup;
}

/*
 * 关闭指定弹窗并把全局指针置空。
 *
 * 主要逻辑：
 *   1. 调用 lv_obj_delete()
 */
static void ui_close_popup(lv_obj_t** popup)
{
    if (popup != NULL && *popup != NULL) {
        lv_obj_delete(*popup);
        *popup = NULL;
    }
}

/*
 * 点击顶部“导出”按钮后，先让用户选择导出成绩表还是统计结果。
 *
 * 主要逻辑：
 *   1. 关闭旧弹窗
 *   2. 创建 CSV 导出按钮
 *   3. 创建 TXT 导出按钮
 *   4. 创建取消按钮
 */
static void show_export_choice_popup(void)
{
    ui_save_and_close_edit();
    ui_close_popup(&export_choice_popup);  // 关闭旧弹窗

    export_choice_popup = ui_create_popup(560, 320);

    lv_obj_t* title = lv_label_create(export_choice_popup);
    lv_label_set_text(title, "请选择导出内容");
    lv_obj_set_pos(title, 20, 18);
    lv_obj_set_size(title, 520, 40);
    lv_obj_set_style_text_align(title, LV_TEXT_ALIGN_CENTER, 0);
    lv_obj_set_style_text_font(title, ui_font(), 0);

    ui_make_popup_button(export_choice_popup, "导出学生成绩表（CSV）", 50, 75, 460, 70, 0x2196F3, on_export_choice_csv);  // 创建 CSV 导出按钮
    ui_make_popup_button(export_choice_popup, "导出统计结果（TXT）", 50, 165, 460, 70, 0x00A65A, on_export_choice_txt);  // 创建 TXT 导出按钮
    ui_make_popup_button(export_choice_popup, "取消", 220, 258, 120, 45, 0x888888, on_export_choice_cancel);  // 创建取消按钮
}

/*
 * “导入”按钮的点击回调签名；点击后打开导入路径输入框。
 *
 * 主要逻辑：
 *   1. 忽略 LVGL 事件参数，导入按钮固定使用预设的中心坐标和导入 action。
 *   2. 导入按钮中心 X = 880 + 30 = 910
 *   3. 以按钮中心 X=910、action=1 打开路径输入框，等待用户输入待导入 CSV 的路径。
 */
static void on_import_clicked(lv_event_t* e)
{
    // 忽略 LVGL 事件参数，导入按钮固定使用预设的中心坐标和导入 action。
    (void)e;
    // 导入按钮中心 X = 880 + 30 = 910
    // 以按钮中心 X=910、action=1 打开路径输入框，等待用户输入待导入 CSV 的路径。
    ui_open_path_ta(1);
}

/*
 * “导出”按钮的点击回调签名；点击后先选择导出 CSV 还是统计结果 TXT。
 *
 * 主要逻辑：
 *   1. 忽略 LVGL 事件参数，导出逻辑由选择弹窗继续处理。
 *   2. 调用 show_export_choice_popup()
 */
static void on_export_clicked(lv_event_t* e)
{
    // 忽略 LVGL 事件参数，导出逻辑由选择弹窗继续处理。
    (void)e;
    show_export_choice_popup();
}

/* ---------- 9.4 删除 ---------- */

/*
 * 删除模式切换按钮的点击回调签名；控制是否进入“点击网格即删除”的模式。
 *
 * 主要逻辑：
 *   1. 忽略 LVGL 事件参数，本回调只需翻转删除模式开关并更新按钮颜色。
 *   2. 切换删除模式前先保存并关闭当前编辑，防止删除状态与编辑状态同时存在造成误操作。
 *   3. 翻转 delete_enabled 开关，原来关闭则开启删除模式，原来开启则关闭删除模式。
 *   4. 若删除模式已开启，进入状态提示分支。
 *   5. 开启状态：红色底表示当前点击网格会进入删除确认
 *   6. 把删除按钮背景设为浅红色，让用户明确知道当前点击表格会进入删除确认流程。
 */
static void on_delete_button_clicked(lv_event_t* e)
{
    // 忽略 LVGL 事件参数，本回调只需翻转删除模式开关并更新按钮颜色。
    (void)e;
    // 切换删除模式前先保存并关闭当前编辑，防止删除状态与编辑状态同时存在造成误操作。
    ui_save_and_close_edit();
    // 翻转 delete_enabled 开关，原来关闭则开启删除模式，原来开启则关闭删除模式。
    delete_enabled = !delete_enabled;

    // 若删除模式已开启，进入状态提示分支。
    if (delete_enabled) {
        // 开启状态：红色底表示当前点击网格会进入删除确认
        // 把删除按钮背景设为浅红色，让用户明确知道当前点击表格会进入删除确认流程。
        lv_obj_set_style_bg_color(btn_delete, lv_color_hex(0xFF5555), 0);
    }
    else {
        // 关闭删除模式时把删除按钮恢复为白色背景，表示回到普通选择/编辑模式。
        lv_obj_set_style_bg_color(btn_delete, lv_color_hex(0xFFFFFF), 0);
    }
}

/*
 * 删除确认弹窗“取消”按钮的回调；取消后关闭弹窗且不删除数据。
 *
 * 主要逻辑：
 *   1. 关闭删除确认弹窗
 *   2. 调用 ui_close_popup()
 */
static void on_delete_cancel(lv_event_t* e)
{
    (void)e;
    ui_close_popup(&delete_popup);  // 关闭删除确认弹窗
}

/*
 * 删除确认弹窗“确定”按钮的回调；确认后真正执行学生数据删除。
 *
 * 主要逻辑：
 *   1. 关闭删除确认弹窗
 *   2. 调用 data_delete_student()
 *   3. 调用 ui_refresh_grid_internal()
 *   4. 调用 ui_close_popup()
 */
static void on_delete_confirm(lv_event_t* e)
{
    (void)e;
    if (delete_student_index >= 0) {
        data_delete_student(delete_student_index);
        ui_refresh_grid_internal();
    }
    ui_close_popup(&delete_popup);  // 关闭删除确认弹窗
}

/*
 * 显示居中的删除确认弹窗。
 *
 * 主要逻辑：
 *   1. 关闭旧弹窗
 *   2. 创建取消按钮
 *   3. 创建确定按钮
 */
static void show_delete_popup(int student_index)
{
    ui_close_popup(&delete_popup);  // 关闭旧弹窗

    StudentCSV* all = data_get_all();
    const char* who = "";
    static char who_buf[256];
    if (all && student_index >= 0) {
        snprintf(who_buf, sizeof(who_buf), "%s(%s)", all[student_index].name, all[student_index].grade);
        who = who_buf;
    }

    delete_student_index = student_index;
    delete_popup = ui_create_popup(500, 200);

    char tip_buf[512];
    snprintf(tip_buf, sizeof(tip_buf), "你确定要删除'%s'的所有信息吗？", who);
    lv_obj_t* tip = lv_label_create(delete_popup);
    lv_label_set_text(tip, tip_buf);
    lv_obj_set_pos(tip, 10, 20);
    lv_obj_set_size(tip, 480, 60);
    lv_obj_set_style_text_align(tip, LV_TEXT_ALIGN_CENTER, 0);
    lv_obj_set_style_text_font(tip, ui_font(), 0);

    ui_make_popup_button(delete_popup, "取消", 150, 130, 80, 50, 0x00C853, on_delete_cancel);  // 创建取消按钮
    ui_make_popup_button(delete_popup, "确定", 270, 130, 80, 50, 0xE53935, on_delete_confirm);  // 创建确定按钮
}

/* ---------- 9.5 权重警告弹窗 ---------- */

/*
 * 权重警告弹窗中“恢复默认”按钮回调：恢复 30/20/50 并刷新表格。
 *
 * 主要逻辑：
 *   1. 关闭权重提示弹窗
 *   2. 调用 data_reset_weights()
 *   3. 调用 data_recalc_all_totals()
 *   4. 调用 ui_refresh_grid_internal()
 *   5. 调用 ui_close_popup()
 */
static void on_weight_reset_clicked(lv_event_t* e)
{
    (void)e;
    data_reset_weights();
    data_recalc_all_totals();
    ui_refresh_grid_internal();
    ui_close_popup(&weight_popup);  // 关闭权重提示弹窗
    ui_show_toast("已恢复默认权重（30/20/50）");
}

/*
 * 显示权重之和不为 100 的警告弹窗，并提供恢复默认按钮。
 *
 * 主要逻辑：
 *   1. 只有一个按钮，左右居中，内边距 0，红色
 *   2. 调用 lv_obj_delete()
 *   3. 调用 lv_obj_create()
 *   4. 调用 lv_screen_active()
 *   5. 调用 lv_obj_set_size()
 */
static void show_weight_warning_popup(float sum)
{
    if (weight_popup) {
        lv_obj_delete(weight_popup);
        weight_popup = NULL;
    }

    weight_popup = lv_obj_create(lv_screen_active());
    lv_obj_set_size(weight_popup, 500, 200);
    lv_obj_center(weight_popup);
    lv_obj_set_style_pad_all(weight_popup, 0, 0);
    lv_obj_set_style_bg_color(weight_popup, lv_color_hex(0xFFFFFF), 0);
    lv_obj_set_style_bg_opa(weight_popup, LV_OPA_COVER, 0);
    lv_obj_set_style_border_width(weight_popup, 2, 0);
    lv_obj_set_style_border_color(weight_popup, lv_color_hex(0x666666), 0);
    lv_obj_set_style_radius(weight_popup, 8, 0);
    lv_obj_add_flag(weight_popup, LV_OBJ_FLAG_EVENT_BUBBLE);
    lv_obj_move_foreground(weight_popup);

    char msg[256];
    snprintf(msg, sizeof(msg),
        "权重之和必须为100%%！\n当前权重之和：%.1f%%\n点击下方按钮恢复默认权重（30/20/50）",
        sum);

    lv_obj_t* tip = lv_label_create(weight_popup);
    lv_label_set_text(tip, msg);
    lv_obj_set_pos(tip, 10, 20);
    lv_obj_set_size(tip, 500 - 20, 110);
    lv_obj_set_style_text_align(tip, LV_TEXT_ALIGN_CENTER, 0);
    lv_obj_set_style_text_font(tip, ui_font(), 0);

    // 只有一个按钮，左右居中，内边距 0，红色
    lv_obj_t* btn = lv_button_create(weight_popup);
    lv_obj_set_size(btn, 220, 50);
    lv_obj_align(btn, LV_ALIGN_BOTTOM_MID, 0, -20);
    lv_obj_set_style_pad_all(btn, 0, 0);
    lv_obj_set_style_bg_color(btn, lv_color_hex(0xFF0000), 0);
    lv_obj_set_style_bg_opa(btn, LV_OPA_COVER, 0);
    lv_obj_set_style_text_color(btn, lv_color_hex(0xFFFFFF), 0);
    lv_obj_set_style_text_font(btn, ui_font(), 0);
    lv_obj_add_event_cb(btn, on_weight_reset_clicked, LV_EVENT_CLICKED, NULL);

    lv_obj_t* label = lv_label_create(btn);
    lv_label_set_text(label, "恢复默认权重");
    lv_obj_set_style_text_font(label, ui_font(), 0);
    lv_obj_center(label);
}

/*
 * 检查权重之和是否合法，合法则重算总分并刷新表格。
 *
 * 主要逻辑：
 *   1. 关闭旧的权重提示弹窗
 *   2. 调用 data_get_weight_sum()
 *   3. 调用 show_weight_warning_popup()
 *   4. 调用 ui_close_popup()
 *   5. 调用 data_recalc_all_totals()
 */
static void ui_apply_weight_change(void)
{
    float sum = data_get_weight_sum();
    if (sum < 99.999f || sum > 100.001f) {
        show_weight_warning_popup(sum);
        return;
    }
    ui_close_popup(&weight_popup);  // 关闭旧的权重提示弹窗
    data_recalc_all_totals();
    ui_refresh_grid_internal();
    ui_show_toast("权重已更新");
}
/* ---------- 9.6 学号不合法提示弹窗 ---------- */

/*
 * 学号不合法提示弹窗的“确定”按钮回调：关闭提示并重新打开学号输入框。
 *
 * 主要逻辑：
 *   1. 关闭提示弹窗
 *   2. 重新打开学号输入框
 */
static void on_invalid_id_ok(lv_event_t* e)
{
    (void)e;
    ui_close_popup(&invalid_id_popup);  // 关闭提示弹窗
    ui_open_path_ta(3);  // 重新打开学号输入框
}

/*
 * 显示学号必须为 12 位数字的提示弹窗。
 *
 * 主要逻辑：
 *   1. 调用 lv_obj_delete()
 *   2. 调用 lv_obj_create()
 *   3. 调用 lv_screen_active()
 *   4. 调用 lv_obj_set_size()
 *   5. 调用 lv_obj_center()
 */
static void show_invalid_id_popup(void)
{
    if (invalid_id_popup) {
        lv_obj_delete(invalid_id_popup);
        invalid_id_popup = NULL;
    }

    invalid_id_popup = lv_obj_create(lv_screen_active());
    lv_obj_set_size(invalid_id_popup, 500, 200);
    lv_obj_center(invalid_id_popup);
    lv_obj_set_style_pad_all(invalid_id_popup, 0, 0);
    lv_obj_set_style_bg_color(invalid_id_popup, lv_color_hex(0xFFFFFF), 0);
    lv_obj_set_style_bg_opa(invalid_id_popup, LV_OPA_COVER, 0);
    lv_obj_set_style_border_width(invalid_id_popup, 2, 0);
    lv_obj_set_style_border_color(invalid_id_popup, lv_color_hex(0x666666), 0);
    lv_obj_set_style_radius(invalid_id_popup, 8, 0);
    lv_obj_add_flag(invalid_id_popup, LV_OBJ_FLAG_EVENT_BUBBLE);
    lv_obj_move_foreground(invalid_id_popup);

    lv_obj_t* tip = lv_label_create(invalid_id_popup);
    lv_label_set_text(tip, "学号不合法\n必须输入 12 位数字");
    lv_obj_set_pos(tip, 10, 30);
    lv_obj_set_size(tip, 500 - 20, 80);
    lv_obj_set_style_text_align(tip, LV_TEXT_ALIGN_CENTER, 0);
    lv_obj_set_style_text_font(tip, ui_font(), 0);

    lv_obj_t* btn = lv_button_create(invalid_id_popup);
    lv_obj_set_size(btn, 180, 45);
    lv_obj_align(btn, LV_ALIGN_BOTTOM_MID, 0, -20);
    lv_obj_set_style_pad_all(btn, 0, 0);
    lv_obj_set_style_bg_color(btn, lv_color_hex(0xFF0000), 0);
    lv_obj_set_style_bg_opa(btn, LV_OPA_COVER, 0);
    lv_obj_set_style_text_color(btn, lv_color_hex(0xFFFFFF), 0);
    lv_obj_set_style_text_font(btn, ui_font(), 0);
    lv_obj_add_event_cb(btn, on_invalid_id_ok, LV_EVENT_CLICKED, NULL);
    lv_group_remove_obj(btn);

    lv_obj_t* label = lv_label_create(btn);
    lv_label_set_text(label, "重新输入");
    lv_obj_set_style_text_font(label, ui_font(), 0);
    lv_obj_center(label);
}
/* ---------- 9.7 Toast 提示 ---------- */

/*
 * 定义点击 Toast 时的回调函数，参数为 LVGL 事件对象。
 *
 * 主要逻辑：
 *   1. 将事件参数强制置空使用，消除未使用参数编译警告，因为点击处理不需要事件数据。
 *   2. 若已有自动关闭定时器在运行，先进入清理分支。
 *   3. 删除旧的 toast 定时器，停止之前安排的自动关闭。
 *   4. 把定时器指针置空，避免之后访问已删除的定时器。
 *   5. 若屏幕上还显示着旧 toast 对象，则进入清理分支。
 *   6. 删除 toast 标签对象，将其从界面移除。
 */
static void on_toast_clicked(lv_event_t* e)
{
    // 将事件参数强制置空使用，消除未使用参数编译警告，因为点击处理不需要事件数据。
    (void)e;
    // 若已有自动关闭定时器在运行，先进入清理分支。
    if (toast_timer) {
        // 删除旧的 toast 定时器，停止之前安排的自动关闭。
        lv_timer_delete(toast_timer);
        // 把定时器指针置空，避免之后访问已删除的定时器。
        toast_timer = NULL;
    }
    // 若屏幕上还显示着旧 toast 对象，则进入清理分支。
    if (toast_obj) {
        // 删除 toast 标签对象，将其从界面移除。
        lv_obj_delete(toast_obj);
        // toast 对象指针置空，避免悬空引用。
        toast_obj = NULL;
    }
}

/*
 * 定义 Toast 自动关闭的定时器回调，由 lv_timer 到期触发。
 *
 * 主要逻辑：
 *   1. 忽略定时器参数以消除编译警告，本回调只需清理 toast。
 *   2. 先把定时器全局指针置空，表示当前没有待执行的 toast 定时器。
 *   3. 若 toast 对象仍在屏幕上，则进行删除。
 *   4. 删除 toast 标签对象。
 *   5. toast 指针置空，防止后续重复删除。
 */
static void on_toast_timer(lv_timer_t* timer)
{
    // 忽略定时器参数以消除编译警告，本回调只需清理 toast。
    (void)timer;
    // 先把定时器全局指针置空，表示当前没有待执行的 toast 定时器。
    toast_timer = NULL;
    // 若 toast 对象仍在屏幕上，则进行删除。
    if (toast_obj) {
        // 删除 toast 标签对象。
        lv_obj_delete(toast_obj);
        // toast 指针置空，防止后续重复删除。
        toast_obj = NULL;
    }
}

/*
 * 定义显示 Toast 提示的函数，msg 为要展示的文本。
 *
 * 主要逻辑：
 *   1. 若已有 toast 正在显示，先删除旧对象，避免多个提示重叠。
 *   2. 删除旧的 toast 标签。
 *   3. 旧 toast 指针置空。
 *   4. 若已有自动关闭定时器，先删除旧定时器。
 *   5. 删除旧的定时器，避免旧回调影响新提示。
 *   6. 定时器指针置空。
 */
static void ui_show_toast(const char* msg)
{
    // 若已有 toast 正在显示，先删除旧对象，避免多个提示重叠。
    if (toast_obj) {
        // 删除旧的 toast 标签。
        lv_obj_delete(toast_obj);
        // 旧 toast 指针置空。
        toast_obj = NULL;
    }
    // 若已有自动关闭定时器，先删除旧定时器。
    if (toast_timer) {
        // 删除旧的定时器，避免旧回调影响新提示。
        lv_timer_delete(toast_timer);
        // 定时器指针置空。
        toast_timer = NULL;
    }

    // 在活动屏幕上新建标签对象作为 toast 提示。
    toast_obj = lv_label_create(lv_screen_active());
    // 把调用方传入的提示文本设置到 toast 标签。
    lv_label_set_text(toast_obj, msg);
    // 设置 toast 的宽高为预定义尺寸，使提示框大小统一。
    lv_obj_set_size(toast_obj, 520, 100);
    // 将 toast 在屏幕中央显示，吸引用户注意。
    lv_obj_center(toast_obj);
    // toast 背景设为白色，作为浮层提示框。
    lv_obj_set_style_bg_color(toast_obj, lv_color_hex(0xFFFFFF), 0);
    // toast 背景不透明。
    lv_obj_set_style_bg_opa(toast_obj, LV_OPA_COVER, 0);
    // 设置 2 像素边框，让提示框边缘清晰。
    lv_obj_set_style_border_width(toast_obj, 2, 0);
    // 边框用浅灰色，柔和且与内容区分。
    lv_obj_set_style_border_color(toast_obj, lv_color_hex(0x888888), 0);
    // 设置 8 像素圆角。
    lv_obj_set_style_radius(toast_obj, 8, 0);
    // 正文文字设为深灰色，保证可读性。
    lv_obj_set_style_text_color(toast_obj, lv_color_hex(0x222222), 0);
    // 正文使用统一 UI 字体。
    lv_obj_set_style_text_font(toast_obj, ui_font(), 0);
    // 文本居中排列。
    lv_obj_set_style_text_align(toast_obj, LV_TEXT_ALIGN_CENTER, 0);
    // 给 toast 添加可点击与事件冒泡标志，使点击 toast 能触发关闭回调。
    lv_obj_add_flag(toast_obj, LV_OBJ_FLAG_CLICKABLE | LV_OBJ_FLAG_EVENT_BUBBLE);
    // 把 toast 移到前台，避免被网格或其他控件遮挡。
    lv_obj_move_foreground(toast_obj);
    // 绑定点击回调，用户点击 toast 时立即关闭它。
    lv_obj_add_event_cb(toast_obj, on_toast_clicked, LV_EVENT_CLICKED, NULL);

    // 创建单次定时器，延迟 TOAST_MS 后调用 on_toast_timer 自动关闭 toast。
    toast_timer = lv_timer_create(on_toast_timer, 2000, NULL);
    // 若定时器创建成功，把重复次数设为 1，保证只自动关闭一次。
    if (toast_timer) lv_timer_set_repeat_count(toast_timer, 1);
}

/* ==================== 十、单元格点击与编辑 ==================== */

/*
 * 关闭单元格编辑器：删除编辑输入框并重置编辑状态。
 *
 * 主要逻辑：
 *   1. 若当前存在编辑文本框 edit_ta，则进入清理分支。
 *   2. 从界面删除编辑文本框。
 *   3. 文本框指针置空。
 *   4. 把当前编辑位置的行列都设为 -1，表示没有正在编辑的单元格。
 *   5. 把当前编辑学生索引设为 -1，表示没有正在编辑的学生。
 */
static void ui_close_edit(void)
{
    // 若当前存在编辑文本框 edit_ta，则进入清理分支。
    if (edit_ta) {
        // 从界面删除编辑文本框。
        lv_obj_delete(edit_ta);
        // 文本框指针置空。
        edit_ta = NULL;
    }
    // 把当前编辑位置的行列都设为 -1，表示没有正在编辑的单元格。
    edit_grid_row = edit_grid_col = -1;
    // 把当前编辑学生索引设为 -1，表示没有正在编辑的学生。
    edit_student_index = -1;
}

/*
 * 保存当前编辑框内容并关闭编辑器。
 *
 * 主要逻辑：
 *   1. 若编辑框已不存在，直接返回，无需保存。
 *   2. 局部缓冲区存放从输入框读取的文本。
 *   3. 记录正在编辑的学生索引，后续更新用。
 *   4. 将网格列号转换为数据字段编号：第 0 列是序号列不可编辑，所以列号减 1。
 *   5. 第1列序号不可编辑，能进入编辑则 col>=1
 *   6. 从编辑框中读取用户输入的文本到 text 缓冲区。
 */
static void ui_save_and_close_edit(void)
{
    // 若编辑框已不存在，直接返回，无需保存。
    if (edit_ta == NULL) return;

    // 局部缓冲区存放从输入框读取的文本。
    char text[256];
    // 记录正在编辑的学生索引，后续更新用。
    int idx = edit_student_index;
    // 将网格列号转换为数据字段编号：第 0 列是序号列不可编辑，所以列号减 1。
    int field = edit_grid_col - 1;  // 第1列序号不可编辑，能进入编辑则 col>=1

    // 从编辑框中读取用户输入的文本到 text 缓冲区。
    ui_get_ta_text(edit_ta, text, sizeof(text));
    // 关闭并销毁编辑框，避免界面残留输入控件。
    ui_close_edit();

    // 仅在学生索引和字段编号均合法时执行数据更新。
    if (idx >= 0 && field >= 0 && field < UI_FIELD_COUNT) {
        // 调用数据层函数把指定学生第 field 个字段更新为输入文本。
        data_update_student_field(idx, field, text);
    }

    // 若学生列表对象仍存在，刷新网格显示，使编辑结果立即反映到界面。
    if (obj_list) ui_refresh_grid_internal();
}

/*
 * 定义文本框编辑完成（LV_EVENT_READY）的回调。
 *
 * 主要逻辑：
 *   1. 忽略事件参数。
 *   2. 编辑完成后保存内容并关闭编辑框。
 */
static void on_edit_ready(lv_event_t* e)
{
    // 忽略事件参数。
    (void)e;
    // 编辑完成后保存内容并关闭编辑框。
    ui_save_and_close_edit();
}

/*
 * 打开某个网格单元格的编辑器，grid_row/grid_col 为网格行列。
 *
 * 主要逻辑：
 *   1. 只能编辑数据行、数据列（col>=1）
 *   2. 只允许编辑数据行和数据列：首行表头与第 0 列序号列不可编辑，非法行列直接返回。
 *   3. 学号列不允许手动编辑
 *   4. 总分由权重实时计算，不允许手动编辑
 *   5. 绩点列由分数实时计算，不允许手动编辑
 *   6. 排名列由系统自动计算，不允许手动编辑
 */
static void ui_open_cell_editor(int grid_row, int grid_col)
{
    // 只能编辑数据行、数据列（col>=1）
    // 只允许编辑数据行和数据列：首行表头与第 0 列序号列不可编辑，非法行列直接返回。
    if (grid_row <= 0 || grid_col <= 0) return;

    // 学号列不允许手动编辑
    if (grid_col == UI_FIELD_ID + 1) {
        ui_show_toast("学号不可修改");
        return;
    }

    // 总分由权重实时计算，不允许手动编辑
    if (grid_col == UI_FIELD_SCORE + 1) {
        ui_show_toast("总分由平时/期中/期末自动计算，不能修改");
        return;
    }

    // 绩点列由分数实时计算，不允许手动编辑
    if (grid_col == UI_FIELD_GPA + 1) {
        ui_show_toast("绩点由分数自动计算，不能修改");
        return;
    }

    // 排名列由系统自动计算，不允许手动编辑
    if (grid_col == UI_FIELD_RANK + 1) {
        ui_show_toast("排名由系统自动计算，不能修改");
        return;
    }

    // 把显示行号（grid_row-1）映射为数据数组中的真实学生索引，支持排序或过滤后的定位。
    int idx = data_get_display_index(grid_row - 1);
    // 若映射不到有效学生索引，说明该行没有对应数据，直接返回。
    if (idx < 0) return;

    // 缓冲区用于保存当前单元格的原始文本，作为编辑框初始内容。
    char current[256];
    // 按当前行列把单元格现有文本填入缓冲区，让用户在原值基础上修改。
    ui_fill_cell_text(grid_row, grid_col, current, sizeof(current));

    // 记录正在编辑的行号，供保存逻辑定位回网格。
    edit_grid_row = grid_row;
    // 记录正在编辑的列号，供保存逻辑判断字段。
    edit_grid_col = grid_col;
    // 记录正在编辑的学生真实索引，供保存逻辑更新数据。
    edit_student_index = idx;

    // 在学生列表对象上创建文本框，作为覆盖在原单元格上的编辑器。
    edit_ta = lv_textarea_create(obj_list);
    // 取当前单元格对象作为定位基准，让编辑器精确覆盖该格。
    lv_obj_t* base_cell = grid_cells[grid_row][grid_col];
    // 将编辑框移到该单元格相同坐标位置，模拟原地编辑。
    lv_obj_set_pos(edit_ta, lv_obj_get_x(base_cell), lv_obj_get_y(base_cell));
    // 把编辑框大小设为该列宽度与标准行高，外观与原单元格一致。
    lv_obj_set_size(edit_ta, col_widths[grid_col], 50);
    // 设置单行模式，学生字段按单行文本编辑。
    lv_textarea_set_one_line(edit_ta, true);
    // 限制最大输入长度 255，与数据字段容量保持一致。
    lv_textarea_set_max_length(edit_ta, 255);
    // 预填当前单元格文本，用户可直接修改。
    lv_textarea_set_text(edit_ta, current);
    // 允许点击光标定位到文本任意位置，方便修改中间内容。
    lv_textarea_set_cursor_click_pos(edit_ta, true);
    // 编辑框文字使用统一 UI 字体。
    lv_obj_set_style_text_font(edit_ta, ui_font(), 0);
    // 文字居中显示，与原单元格风格一致。
    lv_obj_set_style_text_align(edit_ta, LV_TEXT_ALIGN_CENTER, 0);
    // 给编辑框添加事件冒泡标志，使点击文本框不会阻断外层单元格点击逻辑。
    lv_obj_add_flag(edit_ta, LV_OBJ_FLAG_EVENT_BUBBLE);
    // 把编辑框移到前台，确保可正常显示和输入。
    lv_obj_move_foreground(edit_ta);
    // 绑定 READY 事件，用户确认输入后触发 on_edit_ready 保存。
    lv_obj_add_event_cb(edit_ta, on_edit_ready, LV_EVENT_READY, NULL);

    // 若存在键盘组 kb_group（外接键盘或实体按键组），则进入键盘支持分支。
    if (kb_group) {
        // 把编辑框加入键盘焦点组，使其可通过键盘导航。
        lv_group_add_obj(kb_group, edit_ta);
        // 让编辑框立即获得焦点，方便直接输入。
        lv_group_focus_obj(edit_ta);
        // 将键盘组设为编辑模式，使按键直接输入文本而不是导航。
        lv_group_set_editing(kb_group, true);
    }
}

/*
 * 表头“序号”单击：向下翻 UI_PAGE_ROWS 行；如果最后一行已经显示出来，则回到第一行。
 *
 * 主要逻辑：
 *   1. 已经能看到最后一行（滚动到底部）：跳回第一行。
 *   2. 否则立即向下翻 40 行，最多滚到最后一行的位置。
 */
static void ui_scroll_page_down(void)
{
    int display_count;
    lv_coord_t view_h, scroll_y, content_h, max_scroll, target;

    if (obj_list == NULL) return;

    display_count = data_get_display_count();
    if (display_count < 0) display_count = 0;
    if (display_count > 200) display_count = 200;

    view_h = lv_obj_get_height(obj_list);
    scroll_y = lv_obj_get_scroll_y(obj_list);
    content_h = (lv_coord_t)display_count * 50;
    max_scroll = (content_h > view_h) ? (content_h - view_h) : 0;

    // 已经能看到最后一行（滚动到底部）：跳回第一行。
    if (display_count > 0 && scroll_y + view_h >= content_h - 1) {
        lv_obj_scroll_to_y(obj_list, 0, LV_ANIM_OFF);
        return;
    }

    // 否则立即向下翻 40 行，最多滚到最后一行的位置。
    target = scroll_y + (lv_coord_t)40 * 50;
    if (target > max_scroll) target = max_scroll;
    lv_obj_scroll_to_y(obj_list, target, LV_ANIM_OFF);
}

/*
 * 定义网格单元格点击回调，处理删除、二次点击编辑和首次高亮。
 *
 * 主要逻辑：
 *   1. 从事件用户数据中取出打包的单元格编码，还原点击的是哪个单元格。
 *   2. 通过宏从编码中解析出行号。
 *   3. 通过宏从编码中解析出列号。
 *   4. 取得实际触发点击事件的单元格对象，后续与选中状态比较。
 *   5. 如果有编辑框且点的是其它位置，先保存并关闭编辑框
 *   6. 若已有编辑框且用户点击的不是编辑框本身，说明要切换到其他位置。
 */
static void on_cell_clicked(lv_event_t* e)
{
    // 从事件用户数据中取出打包的单元格编码，还原点击的是哪个单元格。
    int code = (int)(intptr_t)lv_event_get_user_data(e);
    // 通过宏从编码中解析出行号。
    int row = ((code) / 16);
    // 通过宏从编码中解析出列号。
    int col = ((code) % 16);
    // 取得实际触发点击事件的单元格对象，后续与选中状态比较。
    lv_obj_t* cell = lv_event_get_target(e);

    // 如果有编辑框且点的是其它位置，先保存并关闭编辑框
    // 若已有编辑框且用户点击的不是编辑框本身，说明要切换到其他位置。
    if (edit_ta && cell != edit_ta) {
        // 先保存并关闭当前编辑框，把当前修改落库并刷新界面。
        ui_save_and_close_edit();
        // 保存后会 refresh，旧 cell 可能已被重建，但 cell 是同一个 label，不影响
    }

    // 删除模式：只对数据行生效
    // 删除模式开启且点击的是数据行（row>0，非表头）时，进入删除流程。
    if (delete_enabled && row > 0) {
        // 获取被点击显示行对应的真实学生索引。
        int idx = data_get_display_index(row - 1);
        // 若索引有效，弹出该学生的删除确认框。
        if (idx >= 0) show_delete_popup(idx);
        // 处理完删除模式后直接返回，不再执行选中或编辑逻辑。
        return;
    }

    // 单击表头“序号”单元格：向下翻 40 行；已经显示最后一行时回到第一行。
    if (row == 0 && col == 0) {
        // 翻页前清掉之前的行/列高亮，避免高亮跟随滚动产生视觉干扰。
        last_sel_row = last_sel_col = -1;
        last_sel_cell = NULL;
        ui_repaint_cells_base();
        // 执行向下翻页，若已在底部则回到第一行。
        ui_scroll_page_down();
        // 阻止事件继续冒泡到屏幕点击处理。
        lv_event_stop_bubbling(e);
        return;
    }

    // 单击 平时/期中/期末 表头单元格：一次输入三项权重
    if (row == 0 &&
        (col == UI_FIELD_REGULAR + 1 ||
            col == UI_FIELD_MIDTERM + 1 ||
            col == UI_FIELD_FINAL + 1)) {
        ui_open_path_ta(7);
        lv_event_stop_bubbling(e);
        return;
    }

    // 再次点击同一个单元格：若可编辑则打开编辑框
    // 判断是否第二次点击同一个单元格，并且该格是数据区单元格（row>0 且 col>0），条件延续到下一行。
    if (last_sel_row == row && last_sel_col == col && last_sel_cell == cell &&
        row > 0 && col > 0) {
        ui_open_cell_editor(row, col);

        // 阻止本次点击继续冒泡到屏幕的 on_screen_clicked，
        // 否则屏幕会立刻把刚打开的编辑框关闭。
        lv_event_stop_bubbling(e);

        return;
    }

    // 第一次点击：整行整列高亮
    // 记录本次点击的行号，作为“第一次点击”的标记，供下一次点击判断是否同一格。
    last_sel_row = row;
    // 记录本次点击的列号，与行号共同标识被单击的网格位置。
    last_sel_col = col;
    // 记录本次点击的单元格对象指针，防止刷新重建后误把新单元格当作旧单元格。
    last_sel_cell = cell;

    // 先把全部单元格恢复到基础背景：白底 + 不及格行红色。
    // 这样上一次的黄/绿高亮会被清除，但红底单元格不会被刷白。
    ui_repaint_cells_base();

    // 只把点击行/列刷成绿色，其余单元格保持原来的背景色不变。
    for (int r = 0; r <= 200; r++) {
        for (int c = 0; c < 12; c++) {
            if (r == row || c == col) {
                ui_set_cell_bg(grid_cells[r][c], 0x00C853);  // 绿色
            }
        }
    }

    // 被点击格黄色
    ui_set_cell_bg(cell, 0xFFFF00);
}

/* ==================== 十一、屏幕全局点击 ==================== */

/*
 * 屏幕全局点击事件的回调函数签名，用于统一处理点击编辑框之外的区域。
 *
 * 主要逻辑：
 *   1. 获取本次事件实际点击到的 LVGL 对象 target，作为判断点击位置的依据。
 *   2. 点编辑框本身/子控件不关闭；点编辑框以外区域保存并关闭
 *   3. 若当前存在编辑文本框，且点击目标不属于编辑框或其子控件，则说明用户点了外部区域。
 *   4. 调用保存并关闭编辑框函数，把正在编辑的内容提交并移除编辑文本框。
 */
static void on_screen_clicked(lv_event_t* e)
{
    // 获取本次事件实际点击到的 LVGL 对象 target，作为判断点击位置的依据。
    lv_obj_t* target = lv_event_get_target(e);

    // 点编辑框本身/子控件不关闭；点编辑框以外区域保存并关闭
    // 若当前存在编辑文本框，且点击目标不属于编辑框或其子控件，则说明用户点了外部区域。
    if (edit_ta && !ui_target_belongs_to_edit(target)) {
        // 调用保存并关闭编辑框函数，把正在编辑的内容提交并移除编辑文本框。
        ui_save_and_close_edit();
    }
}

/* ==================== 十二、UI 创建 ==================== */

/*
 * 创建顶部工具栏的函数，负责搭建搜索区与右侧一排操作按钮。
 *
 * 主要逻辑：
 *   1. 顶部工具栏容器
 *   2. 在屏幕对象上创建顶部工具栏容器并存入 top_obj，作为搜索框和按钮的父对象。
 *   3. 把工具栏定位到屏幕左上角 (0,0)，使其横贯顶部。
 *   4. 把工具栏尺寸设为 1080 宽、80 高，匹配固定屏幕宽度并留出操作区高度。
 *   5. 清除 LVGL 容器默认背景/边框等样式，使顶部条呈现简洁白底效果。
 *   6. 取消工具栏自身可滚动标志，避免内容溢出时出现滚动条。
 */
static void ui_create_top_bar(lv_obj_t* scr)
{
    // 顶部工具栏容器
    // 在屏幕对象上创建顶部工具栏容器并存入 top_obj，作为搜索框和按钮的父对象。
    top_obj = lv_obj_create(scr);
    // 把工具栏定位到屏幕左上角 (0,0)，使其横贯顶部。
    lv_obj_set_pos(top_obj, 0, 0);
    // 把工具栏尺寸设为 1080 宽、80 高，匹配固定屏幕宽度并留出操作区高度。
    lv_obj_set_size(top_obj, 1080, 80);
    // 清除 LVGL 容器默认背景/边框等样式，使顶部条呈现简洁白底效果。
    ui_obj_clear_default(top_obj);
    // 取消工具栏自身可滚动标志，避免内容溢出时出现滚动条。
    lv_obj_clear_flag(top_obj, LV_OBJ_FLAG_SCROLLABLE);
    // 让工具栏可接收点击事件并允许事件向上冒泡，便于后续全局点击逻辑。
    lv_obj_add_flag(top_obj, LV_OBJ_FLAG_CLICKABLE | LV_OBJ_FLAG_EVENT_BUBBLE);

    // 搜索输入框（尺寸 300x60）
    // 在顶部栏创建搜索输入框并存入全局 search_ta。
    search_ta = lv_textarea_create(top_obj);
    // 把搜索框放在顶部栏左侧起始位置，纵向与顶部按钮对齐。
    lv_obj_set_pos(search_ta, 0, 10);
    // 按 UI_SEARCH_W、UI_TOP_BTN_H 设置搜索框尺寸。
    lv_obj_set_size(search_ta, 300, 60);
    // 设置搜索框为单行输入，回车后触发 ready 事件。
    lv_textarea_set_one_line(search_ta, true);
    // 限制搜索关键字最长 255 字符，防止超长输入。
    lv_textarea_set_max_length(search_ta, 255);
    // 设置输入框未输入时显示的提示文字，提示用户输入关键字后回车。
    lv_textarea_set_placeholder_text(search_ta, "搜索：输入关键字后回车");
    // 允许点击文本位置直接放置光标，方便修改关键字。
    lv_textarea_set_cursor_click_pos(search_ta, true);
    // 给输入正文使用程序统一字体。
    lv_obj_set_style_text_font(search_ta, ui_font(), 0);
    // 给占位符文本也设置统一字体，保证提示文字样式一致。
    lv_obj_set_style_text_font(search_ta, ui_font(), LV_PART_TEXTAREA_PLACEHOLDER);
    // 增加 48 像素左内边距，为左侧搜索图标预留空间。
    // 1. 清除 textarea 默认背景、边框、圆角、内边距
    lv_obj_set_style_bg_opa(search_ta, LV_OPA_TRANSP, 0);
    lv_obj_set_style_border_width(search_ta, 0, 0);
    lv_obj_set_style_radius(search_ta, 0, 0);
    lv_obj_set_style_pad_all(search_ta, 0, 0);

    // 2. 把 search.png 当作搜索框背景图，正好铺满 300x60
    {
        snprintf(search_bg_path, sizeof(search_bg_path),
            "%s%s", "A:btn_png/", "search.png");

        lv_obj_set_style_bg_image_src(search_ta, search_bg_path, 0);
        lv_obj_set_style_bg_image_opa(search_ta, LV_OPA_COVER, 0);
        lv_obj_set_style_bg_image_tiled(search_ta, false, 0);
    }

    // 3. 设置文字在胶囊内部的位置
    lv_obj_set_style_pad_left(search_ta, 28, 0);  // 距左侧边框
    lv_obj_set_style_pad_right(search_ta, 90, 0);  // 避开右侧放大镜
    lv_obj_set_style_pad_top(search_ta, 18, 0);  // 垂直居中
    lv_obj_set_style_pad_bottom(search_ta, 18, 0);

    // 4. 保持原来这两行事件注册不动
    lv_obj_add_flag(search_ta, LV_OBJ_FLAG_EVENT_BUBBLE);
    lv_obj_add_event_cb(search_ta, on_search_ready, LV_EVENT_READY, NULL);

    // 搜索框右侧放大镜的透明点击区域：点击后搜索并跳转到第一条匹配行
    {
        lv_obj_t* search_btn = lv_obj_create(top_obj);
        lv_obj_set_pos(search_btn, 300 - 60, 10);
        lv_obj_set_size(search_btn, 60, 60);
        lv_obj_set_style_bg_opa(search_btn, LV_OPA_TRANSP, 0);
        lv_obj_set_style_border_width(search_btn, 0, 0);
        lv_obj_set_style_radius(search_btn, 0, 0);
        lv_obj_set_style_pad_all(search_btn, 0, 0);
        lv_obj_add_flag(search_btn, LV_OBJ_FLAG_CLICKABLE);
        lv_obj_move_foreground(search_btn);
        lv_obj_add_event_cb(search_btn, on_search_button_clicked, LV_EVENT_CLICKED, NULL);
    }

    // 左侧：筛选
    // 创建筛选按钮并赋给全局 btn_filter：父对象为顶部栏、无文字、使用筛选图标。
    btn_filter = ui_make_small_button(top_obj, NULL, "filter.png",
        // 筛选按钮的横向起点取搜索框宽度加按钮间距，使它紧挨搜索框右侧。
        300 + 10,
        // 把筛选按钮点击事件绑定到 on_filter_clicked，用户数据传 NULL。
        on_filter_clicked, NULL);

    // 右侧：帮助、导入、导出、删除，每个间隔 10px，整体贴右边
    // 计算右侧按钮组最左侧起始横坐标：总宽1080减去4个按钮宽和3个间距，使整组贴右。
    int right_x = 1080 - 4 * 60 - 3 * 10;
    // 创建帮助按钮并赋给 btn_help，图标为帮助图标。
    btn_help = ui_make_small_button(top_obj, NULL, "help.png",
        // 把帮助按钮放在右侧按钮组当前横坐标，并绑定 on_help_clicked 回调。
        right_x, on_help_clicked, NULL);
    // 向右推进横坐标，为下一个按钮留出一个按钮宽和一个间距。
    right_x += 60 + 10;
    // 创建导入按钮并赋给 btn_import，图标为导入图标。
    btn_import = ui_make_small_button(top_obj, NULL, "import.png",
        // 把导入按钮放在更新后的横坐标，并绑定 on_import_clicked 回调。
        right_x, on_import_clicked, NULL);
    // 再次向右推进横坐标，用于放置导出按钮。
    right_x += 60 + 10;
    // 创建导出按钮并赋给 btn_export，图标为导出图标。
    btn_export = ui_make_small_button(top_obj, NULL, "export.png",
        // 把导出按钮放在当前横坐标，并绑定 on_export_clicked 回调。
        right_x, on_export_clicked, NULL);
    // 继续向右推进横坐标，用于放置删除按钮。
    right_x += 60 + 10;
    // 创建删除按钮并赋给 btn_delete，图标为删除图标。
    btn_delete = ui_make_small_button(top_obj, NULL, "delete.png",
        // 把删除按钮放在最右侧位置，并绑定 on_delete_button_clicked 回调。
        right_x, on_delete_button_clicked, NULL);
}

/*
 * 创建网格区域的函数，负责承载所有单元格和排序按钮。
 *
 * 主要逻辑：
 *   1. 可滚动数据区：从表头下方开始，只承载数据行。
 *   2. 清除容器默认外观，让网格区域只显示自绘单元格。
 *   3. 只允许纵向滚动，防止横向滚动导致表头与数据列错位。
 *   4. 设置滚动属性
 *   5. 让网格容器可点击且事件可冒泡，保证全局点击能收到。
 *   6. 固定表头容器：位于列表顶部，不随 obj_list 滚动。
 */
static void ui_create_grid_area(lv_obj_t* scr)
{
    // 可滚动数据区：从表头下方开始，只承载数据行。
    obj_list = lv_obj_create(scr);
    lv_obj_set_pos(obj_list, 0, 100 + 50);
    lv_obj_set_size(obj_list, 1080, 620 - 50);
    // 清除容器默认外观，让网格区域只显示自绘单元格。
    ui_obj_clear_default(obj_list);
    // 只允许纵向滚动，防止横向滚动导致表头与数据列错位。
    lv_obj_set_scroll_dir(obj_list, LV_DIR_VER);  // 设置滚动属性
    // 让网格容器可点击且事件可冒泡，保证全局点击能收到。
    lv_obj_add_flag(obj_list, LV_OBJ_FLAG_CLICKABLE | LV_OBJ_FLAG_EVENT_BUBBLE);

    // 固定表头容器：位于列表顶部，不随 obj_list 滚动。
    header_obj = lv_obj_create(scr);
    lv_obj_set_pos(header_obj, 0, 100);
    lv_obj_set_size(header_obj, 1080, 50);
    ui_obj_clear_default(header_obj);
    // 表头本身不滚动。
    lv_obj_remove_flag(header_obj, LV_OBJ_FLAG_SCROLLABLE);
    // 表头创建在 obj_list 之后，再显式置顶，保证滚动时数据行从表头下方穿过、被表头遮住。
    lv_obj_move_foreground(header_obj);

    // 用默认列宽初始化全部列的 col_widths，作为后续绘制与几何计算的基准。
    for (int c = 0; c < 12; c++) col_widths[c] = 120;
    // 创建表头行（header_obj）与数据行（obj_list）。
    ui_create_cells(obj_list);
    // 排序按钮固定在表头行上。
    ui_create_sort_buttons(header_obj);
    // 按当前列宽重新计算并应用所有单元格的位置尺寸。
    ui_apply_geometry();
}

/*
 * 创建完整 UI 的入口函数，接收屏幕对象后搭建所有界面元素。
 *
 * 主要逻辑：
 *   1. 屏幕底色设为浅灰，便于看清顶部白色条与下方白色网格
 *   2. 把屏幕底色设为浅灰 (0xE0E0E0)，衬托顶部白色条和下方白色网格。
 *   3. 设置背景完全不透明，避免透出桌面或下层内容。
 *   4. 设置屏幕默认字体为程序统一字体，使后续控件默认文本样式一致。
 *   5. 全局点击：点击编辑框以外区域时保存编辑
 *   6. 在屏幕上注册点击事件回调 on_screen_clicked，用于点击编辑框外部时保存关闭。
 */
void ui_create(lv_obj_t* scr)
{
    // 屏幕底色设为浅灰，便于看清顶部白色条与下方白色网格
    // 把屏幕底色设为浅灰 (0xE0E0E0)，衬托顶部白色条和下方白色网格。
    lv_obj_set_style_bg_color(scr, lv_color_hex(0xE0E0E0), 0);
    // 设置背景完全不透明，避免透出桌面或下层内容。
    lv_obj_set_style_bg_opa(scr, LV_OPA_COVER, 0);
    // 设置屏幕默认字体为程序统一字体，使后续控件默认文本样式一致。
    lv_obj_set_style_text_font(scr, ui_font(), 0);

    // 全局点击：点击编辑框以外区域时保存编辑
    // 在屏幕上注册点击事件回调 on_screen_clicked，用于点击编辑框外部时保存关闭。
    lv_obj_add_event_cb(scr, on_screen_clicked, LV_EVENT_CLICKED, NULL);

    // 键盘分组
    // 优先使用 SDL HAL 已经创建好的默认 group；如果没有则新建一个。
    kb_group = lv_group_get_default();
    if (kb_group == NULL) kb_group = lv_group_create();

    // 调用函数创建顶部工具栏。
    ui_create_top_bar(scr);
    // 新增按钮：紧挨在筛选按钮右侧，大小和其它顶部按钮一致
    btn_add = ui_make_small_button(top_obj, NULL, "add.png",
        300 + 10 + 60 + 10,
        on_add_clicked, NULL);
    // 调用函数创建下方网格区域。
    ui_create_grid_area(scr);

    // 文本框加入键盘分组；如果已经自动加入，则不要重复添加
    if (lv_obj_get_group(search_ta) != kb_group) {
        lv_group_add_obj(kb_group, search_ta);
    }
    // 让焦点保持在搜索框，避免焦点落到表格里的按钮上导致滚动
    lv_group_focus_obj(search_ta);

    // 初始显示网格（无数据时数据层应返回 200 空行）
    // 初次调用内部刷新，无数据时由数据层返回空行以显示空表格。
    ui_refresh_grid_internal();
}
