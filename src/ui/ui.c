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
 * 业务数据层函数（data_xxx / ui_help_xxx）由 core.c 实现，
 * ui.c 只调用 core.h 提供的接口，不直接操作学生数组。
 */

 /* 包含 UI 层头文件 ui.h；ui.h 内部再包含 core.h，提供数据层函数原型。 */
#include "ui.h"

/* 包含标准输入输出头文件，提供 snprintf 等格式化输出函数，用于拼接图标路径和提示文本。 */
#include <stdio.h>
/* 包含标准工具库头文件，提供内存分配、数值转换等通用 C 库声明。 */
#include <stdlib.h>
/* 包含字符串处理头文件，提供 strlen 等函数用于处理文本框内容。 */
#include <string.h>
/* 包含 inttypes.h，提供 PRIu64/PRIu16 等格式化宏，用于安全打印学号和排名。 */
#include <inttypes.h>


/* ======================== 私有常量 ======================== */

/* 定义自动列宽时单元格文字两侧各保留 8 像素留白，防止文字贴住格线。 */
#define CELL_PAD_X          8          /* 自动列宽时文字左右额外留白 */
/* 定义删除确认弹窗宽度为 500 像素，配合居中布局。 */
#define POPUP_W             500
/* 定义删除确认弹窗高度为 200 像素，可容纳标题与操作按钮。 */
#define POPUP_H             200
/* 定义导入/导出路径输入框宽度为 400 像素，适配弹窗宽度。 */
#define PATH_TA_W           400
/* 定义路径输入框高度为 60 像素，便于点击输入长路径。 */
#define PATH_TA_H           60
/* 定义 Toast 提示框宽度为 520 像素，能显示较长的操作结果信息。 */
#define TOAST_W             520
/* 定义 Toast 提示框高度为 100 像素，给文本留出显示空间。 */
#define TOAST_H             100
/* 定义 Toast 显示 2000 毫秒后自动消失。 */
#define TOAST_MS            2000
/* 定义搜索命中高亮在 5000 毫秒后自动清除。 */
#define SEARCH_HILIGHT_MS   5000

/* ======================== 私有工具宏 ======================== */

/* 把单元格的行/列编码成 int，再存入 user_data；避免为每个格动态分配内存 */
/* 把单元格的网格行列编码成 row*16+col 的整数，作为 user_data 存入控件，避免动态分配内存。 */
#define CELL_CODE(row, col) (((int)(row)) * 16 + (int)(col))
/* 从编码值中除以 16 取整还原行号。 */
#define CELL_ROW(code)      ((code) / 16)
/* 从编码值中对 16 取余还原列号。 */
#define CELL_COL(code)      ((code) % 16)

/* ======================== 私有类型/对象句柄 ======================== */

/* 定义 9 列表头标题的静态常量字符串数组，长度与 UI_COL_NUM 一致。 */
static const char* ui_headers[UI_COL_NUM] = {
    /* 依次填入表头文字：序号、年级、班级、学号、姓名、性别、分数、绩点、排名。 */
    "序号", "年级", "班级", "学号", "姓名", "性别", "分数", "绩点", "排名"
};

/* 保存顶部 1080x80 工具栏容器指针，供后续创建按钮和刷新布局时复用。 */
static lv_obj_t* top_obj;                  /* 顶部 1080x80 工具栏容器 */
/* 保存下方 1080x560 网格表格容器指针，网格单元格都挂在此容器上。 */
static lv_obj_t* obj_list;                 /* 下方 1080x560 网格容器 */
/* 保存搜索输入框对象指针，读取搜索关键字时直接使用。 */
static lv_obj_t* search_ta;                /* 搜索输入框 */
/* 保存筛选按钮对象指针，用于设置图标和点击回调。 */
static lv_obj_t* btn_filter;               /* 筛选按钮 */
/* 保存帮助按钮对象指针，用于触发帮助图片轮播。 */
static lv_obj_t* btn_help;                 /* 帮助按钮 */
/* 保存导入按钮对象指针，点击后弹出路径输入框。 */
static lv_obj_t* btn_import;               /* 导入按钮 */
/* 保存导出按钮对象指针，点击后弹出路径输入框。 */
static lv_obj_t* btn_export;               /* 导出按钮 */
/* 保存删除按钮对象指针，该按钮支持开关状态切换。 */
static lv_obj_t* btn_delete;               /* 删除按钮（开/关状态） */

static lv_obj_t* search_ta;
static char search_bg_path[512];   /* 搜索框背景图路径缓存 */

/* 保存 LVGL 键盘分组对象指针，用于将搜索框等控件加入键盘导航。 */
static lv_group_t* kb_group;               /* 文本输入分组 */

/* 网格：grid_cells[0] 是表头行，grid_cells[1..200] 是数据行 */
/* 声明表格单元格二维数组：第 0 行是表头，第 1 至 200 行显示学生数据，共 9 列。 */
static lv_obj_t* grid_cells[UI_MAX_DATA_ROWS + 1][UI_COL_NUM];
/* 保存按内容自动计算的 9 列列宽，供表头和数据格对齐。 */
static lv_coord_t col_widths[UI_COL_NUM];

/* 单元格选中/编辑状态 */
/* 记录上次选中的数据行号，-1 表示当前无选中单元格。 */
static int last_sel_row = -1;
/* 记录上次选中的列号，-1 表示当前无选中单元格。 */
static int last_sel_col = -1;
/* 记录上次选中的单元格对象，便于切换选中时清除旧高亮。 */
static lv_obj_t* last_sel_cell = NULL;
/* 保存当前正在编辑的文本框对象，NULL 表示未进入编辑状态。 */
static lv_obj_t* edit_ta = NULL;           /* 当前单元格编辑框 */
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
/* 保存待删除学生在数据数组中的下标，供确认回调删除。 */
static int delete_student_index = -1;

/* 帮助图片状态 */
/* 保存帮助图片按钮对象，点击后切换帮助图片内容。 */
static lv_obj_t* help_imagebtn = NULL;
/* 记录当前帮助图片索引，从 0 开始轮播。 */
static int help_index = 0;
/* 记录帮助图片总张数，用于索引越界时回绕。 */
static int help_count = 0;

/* 导入/导出路径输入状态 */
/* 保存导入/导出路径输入框对象，确认时读取用户输入的路径。 */
static lv_obj_t* path_ta = NULL;
/* 记录当前路径输入用途：0 无操作、1 导入、2 导出，决定确认后调用 data_import_csv 还是 data_export_csv。 */
static int path_action = 0;                 /* 0=无, 1=导入, 2=导出 */

/* 筛选弹层状态 */
/* 保存筛选一级类别列表对象（年级/班级/性别/分数/全部）。 */
static lv_obj_t* filter_list1 = NULL;
/* 保存筛选二级选项列表对象，展示某个类别下的具体选项。 */
static lv_obj_t* filter_list2 = NULL;
/* 记录二级列表当前对应的 ui_field_t 字段，用于生成选项。 */
static int filter_field = 0;                /* 当前二级列表对应 ui_field_t */
/* 声明最多 64 个筛选项、每项 MAX_FIELD_LEN 字符的缓冲区，接收数据层返回的选项。 */
static char filter_opt_buf[64][MAX_FIELD_LEN];

/* 搜索高亮 / 导出提示定时器 */
/* 保存搜索高亮清除定时器对象，延迟后恢复单元格白底。 */
static lv_timer_t* search_timer = NULL;
/* 保存 Toast 自动隐藏定时器对象，到时删除提示框。 */
static lv_timer_t* toast_timer = NULL;
/* 保存 Toast 提示框对象，便于创建后定时移除。 */
static lv_obj_t* toast_obj = NULL;

/* 表头排序按钮：年级、班级、性别、分数 */
/* 保存 4 个表头排序按钮对象（年级、班级、性别、分数）。 */
static lv_obj_t* sort_buttons[4];
/* 保存 4 个排序按钮的状态：0 未点击、1 正序、-1 倒序。 */
static int sort_states[4];   /* 0=未点击，1=当前正序，-1=当前倒序 */

/* 四个排序按钮对应的 StudentCSV 字段 */
/* 定义 4 个排序按钮对应的字段常量数组，长度与排序按钮数量一致。 */
static const int sort_fields[4] = {
    /* 初始化排序字段为年级、班级、性别、分数四个 ui_field_t 枚举值。 */
    UI_FIELD_GRADE, UI_FIELD_CLASS, UI_FIELD_GENDER, UI_FIELD_SCORE
};




/* ======================== 前置声明 ======================== */

/* 前置声明内部刷新网格函数，使后面定义可互相调用。 */
static void ui_refresh_grid_internal(void);
/* 前置声明布局应用函数，创建后统一计算对象位置尺寸。 */
static void ui_apply_geometry(void);
/* 前置声明单元格文本填充函数，buf 和 buf_size 指定输出缓冲区。 */
static void ui_fill_cell_text(int grid_row, int grid_col, char* buf, size_t buf_size);
/* 前置声明按当前显示行数自动分配列宽的函数。 */
static void ui_auto_columns(int display_count);
/* 前置声明设置单元格背景色的函数，color 为十六进制 RGB 值。 */
static void ui_set_cell_bg(lv_obj_t* cell, uint32_t color);
/* 前置声明将所有数据单元格背景恢复为白色的函数。 */
static void ui_reset_all_cells_white(void);
/* 前置声明关闭编辑框并丢弃改动的函数。 */
static void ui_close_edit(void);
/* 前置声明保存编辑内容并关闭编辑框的函数。 */
static void ui_save_and_close_edit(void);
/* 前置声明在指定网格行列上打开编辑框的函数。 */
static void ui_open_cell_editor(int grid_row, int grid_col);
/* 前置声明关闭筛选弹层列表的函数。 */
static void ui_close_filter_lists(void);
/* 前置声明显示 Toast 消息的函数，msg 为提示文本。 */
static void ui_show_toast(const char* msg);
/* 前置声明清除搜索高亮的定时器回调函数。 */
static void ui_clear_search_hilight(lv_timer_t* timer);
/* 前置声明判断点击目标是否属于当前编辑框或其子控件的函数。 */
static bool ui_target_belongs_to_edit(lv_obj_t* target);
/* 前置声明屏幕点击回调，用于点击编辑/筛选外部时收起浮层。 */
static void on_screen_clicked(lv_event_t* e);
/* 前置声明单元格点击回调，负责选中高亮和编辑交互。 */
static void on_cell_clicked(lv_event_t* e);
/* 前置声明删除按钮点击回调，切换删除开关或打开确认框。 */
static void on_delete_button_clicked(lv_event_t* e);
/* 前置声明按学生下标显示删除确认弹窗的函数。 */
static void show_delete_popup(int student_index);
/* 前置声明给按钮附加图标文件的函数，icon_name 为图标名。 */
static void ui_attach_icon(lv_obj_t* parent, const char* icon_name);
/* 前置声明在表头创建排序按钮的函数。 */
static void ui_create_sort_buttons(lv_obj_t* parent);
/* 前置声明重新摆放排序按钮位置的函数，列宽变化后调用。 */
static void ui_position_sort_buttons(void);
/* 前置声明排序按钮点击回调，切换升降序并刷新数据。 */
static void on_sort_clicked(lv_event_t* e);


/* ======================== 小工具函数 ======================== */
static const lv_font_t* g_ui_font = &lv_font_source_han_sans_sc_16_cjk;

static const lv_font_t* ui_font(void)
{
    return g_ui_font;
}

void ui_set_font(const lv_font_t* font)
{
    if (font != NULL) {
        g_ui_font = font;
    }
}


/* 定义对象默认样式清理函数，把控件设为无边框、无圆角、白底、统一字体。 */
static void ui_obj_clear_default(lv_obj_t* obj)
{
    /* 设置对象边框宽度为 0，去掉 LVGL 默认边框。 */
    lv_obj_set_style_border_width(obj, 0, 0);
    /* 设置对象圆角为 0，让表格单元格呈现直角矩形。 */
    lv_obj_set_style_radius(obj, 0, 0);
    /* 设置对象四周内边距为 0，使文字不产生多余留白。 */
    lv_obj_set_style_pad_all(obj, 0, 0);
    /* 设置对象背景色为白色 0xFFFFFF，作为单元格/面板默认底色。 */
    lv_obj_set_style_bg_color(obj, lv_color_hex(0xFFFFFF), 0);
    /* 设置背景不透明度为完全覆盖 LV_OPA_COVER，确保白色真正显示。 */
    lv_obj_set_style_bg_opa(obj, LV_OPA_COVER, 0);
    /* 设置对象文字字体为 ui_font() 返回的中文字体，0 表示主样式。 */
    lv_obj_set_style_text_font(obj, ui_font(), 0);
    /* 设置滚动条为 LV_SCROLLBAR_MODE_AUTO，内容超出时才显示滚动条。 */
    lv_obj_set_scrollbar_mode(obj, LV_SCROLLBAR_MODE_AUTO);
}

/**
 * 从单行文本框中取出干净文本（去掉首尾空白和换行）。
 */
 /* 定义从文本框取干净文本的函数：去掉首尾空白和内部换行后存入 out。 */
static void ui_get_ta_text(lv_obj_t* ta, char* out, size_t out_size)
{
    /* 调用 lv_textarea_get_text 获取文本框当前内容指针。 */
    const char* txt = lv_textarea_get_text(ta);
    /* 定义 i/j 两个读写游标和 len 保存源文本长度。 */
    size_t i, j, len;

    /* 如果获取到的文本指针为空，则当作空字符串，避免后续 strlen 对空指针操作。 */
    if (txt == NULL) txt = "";
    /* 用 strlen 计算源文本长度，作为遍历边界。 */
    len = strlen(txt);

    /* 去掉开头空白 */
    /* 将源文本游标 i 初始化为 0，准备从开头跳过空白。 */
    i = 0;
    /* 循环跳过开头的空格、制表符、回车和换行，使 i 指向第一个有效字符。 */
    while (i < len && (txt[i] == ' ' || txt[i] == '\t' || txt[i] == '\r' || txt[i] == '\n')) i++;

    /* 复制到 out */
    /* 将输出游标 j 初始化为 0，准备写入 out 缓冲区。 */
    j = 0;
    /* 逐字符遍历源文本，j+1<out_size 保证末尾可写入 NUL。 */
    while (i < len && j + 1 < out_size) {
        /* 遇到回车或换行时只移动源游标并跳过，达到删除文本中换行的目的。 */
        if (txt[i] == '\r' || txt[i] == '\n') { i++; continue; }
        /* 把当前非换行字符复制到输出缓冲区，并同步前进两个游标。 */
        out[j++] = txt[i++];
    }

    /* 去掉末尾空白 */
    /* 从输出末尾向前删除空格和制表符，完成去尾空白。 */
    while (j > 0 && (out[j - 1] == ' ' || out[j - 1] == '\t')) j--;
    /* 在 out[j] 写入字符串结束符 NUL，形成标准 C 字符串。 */
    out[j] = '\0';
}

/**
 * 判断目标对象是否为编辑框自身或编辑框的子对象。
 */
 /* 定义判断函数：从 target 沿父链上溯，判断其是否属于当前编辑框 edit_ta。 */
static bool ui_target_belongs_to_edit(lv_obj_t* target)
{
    /* 从目标控件开始循环，逐级检查父对象链。 */
    while (target) {
        /* 若当前节点就是编辑框 edit_ta，说明点击位置在编辑区内部，返回 true。 */
        if (target == edit_ta) return true;
        /* 获取当前节点的父对象，继续向上检查更上层容器。 */
        target = lv_obj_get_parent(target);
    }
    /* 整个父链都没有遇到 edit_ta，说明点击在编辑框外部，返回 false。 */
    return false;
}

/**
 * 创建一个顶部功能按钮：固定 60x60。
 * @param icon_name 图标文件名；传 NULL 则使用 text 显示文字。
 * @param text      文字备用标签；传 NULL 且无图标时按钮为空。
 */
 /* 定义创建顶部小按钮的辅助函数，参数包含父对象、文字、图标名、x 坐标、回调与用户数据。 */
static lv_obj_t* ui_make_small_button(lv_obj_t* parent, const char* text,
    /* 签名续行：icon_name 为图标文件名，传 NULL 或空串时不使用图标。 */
    const char* icon_name,
    /* 签名续行：x 为按钮水平坐标，cb 是点击回调，user_data 会通过事件传给回调。 */
    int x, lv_event_cb_t cb, void* user_data)
{
    /* 调用 lv_button_create 在 parent 上创建按钮并返回对象句柄。 */
    lv_obj_t* btn = lv_button_create(parent);
    /* 将按钮放置在 (x, UI_TOP_BTN_Y=10)，x 由调用方传入以排列多个按钮。 */
    lv_obj_set_pos(btn, x, UI_TOP_BTN_Y);
    /* 设置按钮宽高为 UI_SMALL_BTN_W x UI_TOP_BTN_H（60x60），符合顶部工具栏布局。 */
    lv_obj_set_size(btn, UI_SMALL_BTN_W, UI_TOP_BTN_H);
    /* 设置按钮背景色 */
    lv_obj_set_style_bg_color(btn, lv_color_hex(0xFFFFFF), 0);
    lv_obj_set_style_bg_opa(btn, LV_OPA_COVER, 0);
    /* 设置按钮文字颜色（有文字时用） */
    lv_obj_set_style_text_color(btn, lv_color_hex(0x000000), 0);
    /* 设置按钮文字字体为统一中文字体，保证按钮上中文正常显示。 */
    lv_obj_set_style_text_font(btn, ui_font(), 0);
    /* 添加 LV_OBJ_FLAG_EVENT_BUBBLE 标志，使按钮点击事件可冒泡给父级处理。 */
    lv_obj_add_flag(btn, LV_OBJ_FLAG_EVENT_BUBBLE);

    /* 判断 icon_name 非空且不是空字符串，决定是否使用图标模式。 */
    if (icon_name != NULL && icon_name[0] != '\0') {
        /* 调用 ui_attach_icon 把 icon_name 指定的图标挂到按钮上。 */
        ui_attach_icon(btn, icon_name);
    }
    /* 没有有效图标时，如果 text 非空，则退化为文字按钮。 */
    else if (text != NULL) {
        /* 在按钮内部创建 label 标签对象，用于显示按钮文字。 */
        lv_obj_t* label = lv_label_create(btn);
        /* 调用 lv_label_set_text 把按钮显示文字设为 text。 */
        lv_label_set_text(label, text);
        /* 设置 label 字体为统一中文字体，保证备用文字中文正常。 */
        lv_obj_set_style_text_font(label, ui_font(), 0);
        /* 调用 lv_obj_center 将 label 在按钮内居中。 */
        lv_obj_center(label);
    }

    /* 给按钮绑定 LV_EVENT_CLICKED 点击事件回调 cb，并把 user_data 传给回调。 */
    lv_obj_add_event_cb(btn, cb, LV_EVENT_CLICKED, user_data);
    /* 返回新按钮对象，供调用方保存或继续设置位置/样式。 */
    return btn;
}

/**
 * 在父对象中心添加一个图标图片。
 * @param icon_name ui.h 中定义的图标文件名，例如 "filter.png"
 */
 /* ui_attach_icon 静态函数：在指定父对象中心附加一个图标图片，供界面各处复用图标。 */
static void ui_attach_icon(lv_obj_t* parent, const char* icon_name)
{
    /* 若图标名为空或首字符是字符串结束符则直接返回，避免用无效路径创建图片。 */
    if (icon_name == NULL || icon_name[0] == '\0') return;

    /* 在 parent 上创建 LVGL 图片对象 img，作为后续设置图标来源和居中的基础。 */
    lv_obj_t* img = lv_image_create(parent);
    /* 声明 512 字节字符数组 path，用于存放图标目录与文件名的完整路径。 */
    char path[512];
    /* 将 UI_ICON_FOLDER 和 icon_name 安全拼接进 path，限制长度防止缓冲区溢出。 */
    snprintf(path, sizeof(path), "%s%s", UI_ICON_FOLDER, icon_name);
    /* 把拼接好的路径设置为图片对象 img 的图像来源。 */
    lv_image_set_src(img, path);
    /* 让图片对象在父对象内部水平垂直居中显示。 */
    lv_obj_center(img);
}

/* ======================== 单元格工具 ======================== */

/* ui_set_cell_bg 单元格工具函数：把指定颜色设置为单元格的不透明背景色。 */
static void ui_set_cell_bg(lv_obj_t* cell, uint32_t color)
{
    /* 设置单元格背景色为传入的 color 色值，样式选择器 0 表示默认状态。 */
    lv_obj_set_style_bg_color(cell, lv_color_hex(color), 0);
    /* 把背景透明度设为 LV_OPA_COVER，确保背景完全覆盖底层内容。 */
    lv_obj_set_style_bg_opa(cell, LV_OPA_COVER, 0);
}

/* ui_set_cell_style 函数：统一设置每个表格格子的边框、圆角、内边距和文字样式。 */
static void ui_set_cell_style(lv_obj_t* cell)
{
    /* 设置 1 像素边框宽度，使单元格之间呈现清晰的表格分隔线。 */
    lv_obj_set_style_border_width(cell, 1, 0);
    /* 边框颜色设为浅灰 0xCCCCCC，形成常见的表格网格线观感。 */
    lv_obj_set_style_border_color(cell, lv_color_hex(0xCCCCCC), 0);
    /* 圆角设为 0，让单元格保持直角方形，符合数据表格风格。 */
    lv_obj_set_style_radius(cell, 0, 0);
    /* 先清除四周内边距，之后只通过上内边距精细控制文字垂直位置。 */
    lv_obj_set_style_pad_all(cell, 0, 0);
    /* 单元格默认背景设为白色，保证未选中时底色干净统一。 */
    lv_obj_set_style_bg_color(cell, lv_color_hex(0xFFFFFF), 0);
    /* 背景透明度设为完全不透明，防止下层内容透过单元格显示。 */
    lv_obj_set_style_bg_opa(cell, LV_OPA_COVER, 0);
    /* 文字颜色设为深色 0x222222，保证在白色背景上清晰可读。 */
    lv_obj_set_style_text_color(cell, lv_color_hex(0x222222), 0);
    /* 文字字体使用 ui_font() 返回的界面字体，与整体 UI 字体保持一致。 */
    lv_obj_set_style_text_font(cell, ui_font(), 0);
    /* 文字水平居中对齐，使表头和数据内容都位于格子中央。 */
    lv_obj_set_style_text_align(cell, LV_TEXT_ALIGN_CENTER, 0);

    /* 用上内边距让单行文字在 50px 行高内接近垂直居中 */
    /* 按行高与字体行高的差值计算上内边距，使单行文字在 50px 行高内接近垂直居中。 */
    int pad_top = (UI_ROW_H - (int)lv_font_get_line_height(ui_font())) / 2;
    /* 若字体行高大于行高导致计算值为负，则归零，避免设置非法负内边距。 */
    if (pad_top < 0) pad_top = 0;
    /* 应用计算出的上内边距，实现单元格文字在固定行高内近似垂直居中。 */
    lv_obj_set_style_pad_top(cell, pad_top, 0);

    /* 设置 label 长文本模式为省略号，超长内容不换行而是显示省略号。 */
    lv_label_set_long_mode(cell, LV_LABEL_LONG_MODE_DOTS);
    /* 为单元格添加可点击和事件冒泡标志，使其可被点击且事件能传给父对象。 */
    lv_obj_add_flag(cell, LV_OBJ_FLAG_CLICKABLE | LV_OBJ_FLAG_EVENT_BUBBLE);
}

/**
 * 创建 201 x 9 个 label 单元格（表头 1 行 + 数据 200 行）。
 */
 /* ui_create_cells 函数：创建 201 行 × 9 列的 label 单元格，含 1 行表头和 200 行数据。 */
static void ui_create_cells(lv_obj_t* parent)
{
    /* 外层循环遍历 0 到 UI_MAX_DATA_ROWS 行，其中 0 是表头行，1 起是数据行。 */
    for (int r = 0; r <= UI_MAX_DATA_ROWS; r++) {
        /* 内层循环遍历 0 到 UI_COL_NUM-1 列，逐格创建整个表格区域。 */
        for (int c = 0; c < UI_COL_NUM; c++) {
            /* 在 parent 上创建 label 对象 cell，用它显示每个格子的文本内容。 */
            lv_obj_t* cell = lv_label_create(parent);
            /* 将新建单元格指针保存到全局二维数组 grid_cells[r][c]，供布局与刷新函数使用。 */
            grid_cells[r][c] = cell;
            /* 调用统一单元格样式函数，让所有格子具备一致的边框、字体和背景。 */
            ui_set_cell_style(cell);
            /* 为单元格注册点击事件回调 on_cell_clicked，监听 LV_EVENT_CLICKED 点击事件。 */
            lv_obj_add_event_cb(cell, on_cell_clicked, LV_EVENT_CLICKED,
                /* 把 CELL_CODE(r,c) 编码后的行列值作为用户数据传给回调，用于识别被点击的单元格。 */
                (void*)(intptr_t)CELL_CODE(r, c));
        }
    }
}

/**
 * 重新摆放所有单元格的位置/尺寸。
 * 没有数据时每列保持 120px，因此总宽 1080px；列宽自适应后仍从左开始排列。
 */
 /* ui_apply_geometry 函数：按当前列宽重新摆放所有单元格的位置和大小。 */
static void ui_apply_geometry(void)
{
    /* 定义并初始化当前行顶部纵坐标 y 为 0，从表格左上角开始排布。 */
    lv_coord_t y = 0;

    /* 外层循环遍历表头行和全部数据行，逐行设置单元格坐标。 */
    for (int r = 0; r <= UI_MAX_DATA_ROWS; r++) {
        /* 每一行开始时把横向坐标 x 重置为 0，保证各行都从左侧边缘开始排列。 */
        lv_coord_t x = 0;
        /* 内层循环遍历该行所有列，按各列宽度逐个放置单元格。 */
        for (int c = 0; c < UI_COL_NUM; c++) {
            /* 将 grid_cells[r][c] 移动到当前行列的 (x,y) 位置。 */
            lv_obj_set_pos(grid_cells[r][c], x, y);
            /* 设置单元格宽度为 col_widths[c]、高度为 UI_ROW_H，形成行列尺寸一致的表格。 */
            lv_obj_set_size(grid_cells[r][c], col_widths[c], UI_ROW_H);
            /* 横向坐标累加当前列宽，为放置下一列预留位置。 */
            x += col_widths[c];
        }
        /* 一行排完后纵坐标增加一个行高，准备排布下一行。 */
        y += UI_ROW_H;
    }

    /* 表头排序按钮跟随列宽移动 */
    /* 调用排序按钮布局函数，让四个表头排序按钮跟随列宽变化移动到新位置。 */
    ui_position_sort_buttons();
}

/**
 * 重新摆放四个表头排序按钮的位置。
 * 排序按钮位于对应表头格右侧，高度 40px，上下居中。
 */
 /* ui_position_sort_buttons 函数：重新定位四个表头排序按钮，使其贴住对应列右侧。 */
static void ui_position_sort_buttons(void)
{
    /* 遍历 4 个排序按钮，分别处理年级、班级、性别、分数对应的表头按钮。 */
    for (int i = 0; i < 4; i++) {
        /* 若该排序按钮尚未创建则跳过，避免对空指针做布局操作。 */
        if (sort_buttons[i] == NULL) continue;

        /* 取出第 i 个排序按钮对应的数据字段编号，用于确定它属于哪一列。 */
        int field = sort_fields[i];
        /* 因为第 0 列是序号列，数据字段所在的实际表格列号为 field+1。 */
        int col = field + 1;

        /* 定义横向坐标 x 并从 0 开始累加，用于计算按钮在目标列左侧的位置。 */
        lv_coord_t x = 0;
        /* 将目标列之前所有列宽累加到 x，得到该列左边缘的横坐标。 */
        for (int c = 0; c < col; c++) x += col_widths[c];
        /* 在列左边缘基础上再加列宽并减按钮宽 20 和 2px 留白，让按钮贴在列右缘内侧。 */
        x += col_widths[col] - 20 - 2;   /* 贴右留 2px */

        /* 设置按钮横坐标 x、纵坐标 5px，使高 40px 的按钮在 50px 表头行内垂直居中。 */
        lv_obj_set_pos(sort_buttons[i], x, 5);   /* (50 - 40) / 2 = 5 */
    }
}

/**
 * 点击表头排序按钮：同一按钮第一次正序，第二次倒序，之后循环切换。
 */
 /* on_sort_clicked 回调：点击表头排序按钮时切换升降序，并触发数据排序和界面刷新。 */
static void on_sort_clicked(lv_event_t* e)
{
    /* 从事件用户数据中取出被点击排序按钮对应的字段编号。 */
    int field = (int)(intptr_t)lv_event_get_user_data(e);
    /* 把索引 idx 初始化为 -1，用于标记是否在排序按钮数组中找到该字段。 */
    int idx = -1;
    /* 遍历 4 个排序按钮的字段配置，查找与点击字段相同的项。 */
    for (int i = 0; i < 4; i++) {
        /* 当 sort_fields[i] 等于点击的字段时，说明找到了对应排序按钮。 */
        if (sort_fields[i] == field) {
            /* 把找到的数组下标保存到 idx，供后续读取排序状态。 */
            idx = i;
            /* 找到匹配按钮后立即跳出循环，无需继续遍历。 */
            break;
        }
    }
    /* 若未找到对应字段索引则直接返回，防止后续访问无效的排序状态。 */
    if (idx < 0) return;

    /* 声明布尔变量 ascending，用于记录本次点击后应采用的排序方向。 */
    bool ascending;
    /* 若该字段当前记录为升序状态 1，则本次点击需要切换为降序。 */
    if (sort_states[idx] == 1) {
        /* 将排序方向标记为 false，表示本次要降序排列。 */
        ascending = false;
        /* 把该字段的排序状态更新为 -1，记录当前已切换为降序。 */
        sort_states[idx] = -1;
    }
    /* else 分支处理当前不是升序状态的情况，即当前为降序或初始状态时改为升序。 */
    else {
        /* 将排序方向标记为 true，表示本次要升序排列。 */
        ascending = true;
        /* 把该字段的排序状态更新为 1，记录当前已切换为升序。 */
        sort_states[idx] = 1;
    }

    /* 调用数据层函数 data_sort_students，按指定字段和方向对学生数据进行排序。 */
    data_sort_students(field, ascending);
    /* 排序完成后刷新表格内部显示，让界面立即呈现新的学生顺序。 */
    ui_refresh_grid_internal();
}

/**
 * 创建表头排序按钮：年级、班级、性别、分数。
 */
 /* ui_create_sort_buttons 函数：创建 4 个表头排序按钮，覆盖年级、班级、性别、分数。 */
static void ui_create_sort_buttons(lv_obj_t* parent)
{
    /* 循环创建 4 个排序按钮，每个按钮对应 sort_fields 中预设的一个数据字段。 */
    for (int i = 0; i < 4; i++) {
        /* 在父对象上创建 LVGL 按钮并保存到全局 sort_buttons[i]，供布局和回调使用。 */
        sort_buttons[i] = lv_button_create(parent);
        /* 设置按钮尺寸为 20×40 像素，形成窄长形可点击区域以容纳排序图标。 */
        lv_obj_set_size(sort_buttons[i], 20, 40);
        /* 去掉按钮边框，使排序按钮在表头中只呈现为图标而不是带边框的控件。 */
        lv_obj_set_style_border_width(sort_buttons[i], 0, 0);
        /* 圆角设为 0，让按钮保持直角外观，与表格直角单元格风格统一。 */
        lv_obj_set_style_radius(sort_buttons[i], 0, 0);
        /* 按钮背景设为透明，避免遮挡表头单元格的文字内容。 */
        lv_obj_set_style_bg_opa(sort_buttons[i], LV_OPA_TRANSP, 0);
        /* 添加可点击和事件冒泡标志，使排序按钮可被点击并允许事件继续冒泡。 */
        lv_obj_add_flag(sort_buttons[i], LV_OBJ_FLAG_CLICKABLE | LV_OBJ_FLAG_EVENT_BUBBLE);

        /* 使用同一个 sort.png 图标 */
        /* 声明 512 字节路径缓冲区，用于拼接图标目录和排序图标文件名。 */
        char path[512];
        /* 将 UI_ICON_FOLDER 与 UI_ICON_SORT 拼成排序图标的完整路径写入 path。 */
        snprintf(path, sizeof(path), "%s%s", UI_ICON_FOLDER, UI_ICON_SORT);
        /* 在排序按钮内部创建图片对象 img，用来显示排序箭头图标。 */
        lv_obj_t* img = lv_image_create(sort_buttons[i]);
        /* 把拼接好的路径设为图片对象 img 的图像来源，加载排序图标。 */
        lv_image_set_src(img, path);
        /* 让排序图标在按钮内部水平垂直居中显示。 */
        lv_obj_center(img);

        /* 为排序按钮注册 LV_EVENT_CLICKED 点击回调 on_sort_clicked。 */
        lv_obj_add_event_cb(sort_buttons[i], on_sort_clicked, LV_EVENT_CLICKED,
            /* 把当前按钮对应的字段编号 sort_fields[i] 作为用户数据传给回调，用于识别排序列。 */
            (void*)(intptr_t)sort_fields[i]);
    }
}

/**
 * 读取一个单元格应显示的文本。
 * @param grid_row 0=表头行；1..200=数据行。
 * @param grid_col 0=序号列；1..8=数据列。
 */
 /* ui_fill_cell_text 函数：根据网格行列位置把应显示的文本内容写入缓冲区。 */
static void ui_fill_cell_text(int grid_row, int grid_col, char* buf, size_t buf_size)
{
    /* 若目标缓冲区大小为 0 则直接返回，避免后续写入造成越界。 */
    if (buf_size == 0) return;
    /* 先把缓冲区首字符设为结束符，保证没有内容时缓冲区仍是合法空字符串。 */
    buf[0] = '\0';

    /* 表头行 */
    /* 判断 grid_row 是否为 0，即当前是否为表头行。 */
    if (grid_row == 0) {
        /* 表头行中仅当列号在合法列范围内时才复制对应表头文本。 */
        if (grid_col >= 0 && grid_col < UI_COL_NUM) {
            /* 从 ui_headers 数组中取出该列表头字符串并格式化写入 buf。 */
            snprintf(buf, buf_size, "%s", ui_headers[grid_col]);
        }
        /* 表头行内容填充完成后直接返回，不再进入数据行处理逻辑。 */
        return;
    }

    /* 第 1 列：显示行号，从 1 开始 */
    /* 判断是否为数据行第 0 列，即左侧的序号列。 */
    if (grid_col == 0) {
        /* 把网格行号 grid_row 格式化为十进制数字写入 buf，作为从 1 开始的行号。 */
        snprintf(buf, buf_size, "%d", grid_row);
        /* 序号列处理完成后立即返回，避免继续读取学生数据。 */
        return;
    }

    /* 第 2~9 列：从数据层取学生 */
    /* 将网格数据行号减 1，换算成数据数组下标，因为第 0 行被表头占用。 */
    int display_row = grid_row - 1;
    /* 通过数据层获取当前显示顺序下的学生索引，以支持排序后的显示顺序。 */
    int idx = data_get_display_index(display_row);
    /* 若显示索引小于 0，表示该行当前没有有效学生数据。 */
    if (idx < 0) {
        /* 把缓冲区置空，使该单元格显示为空。 */
        buf[0] = '\0';
        /* 无效索引时直接返回，避免继续用无效下标读取学生数组。 */
        return;
    }

    /* 获取数据层学生数组指针 s，用于按索引读取当前学生的各字段。 */
    StudentCSV* s = data_get_all();
    /* 若学生数组为空说明尚未加载数据，需要清空单元格文本。 */
    if (s == NULL) {
        /* 将缓冲区首字符置为结束符，使单元格显示为空字符串。 */
        buf[0] = '\0';
        /* 数据为空时直接返回，避免对空指针解引用。 */
        return;
    }
    /* 将指针 s 移动到当前显示索引对应的学生记录，后续即可安全读取该学生字段。 */
    s = &s[idx];

    /* 用去除序号列后的列索引做 switch 分发，选择要显示的学生字段。 */
    switch (grid_col - 1) {
        /* case 年级字段：本格显示学生年级字符串。 */
    case UI_FIELD_GRADE:
        /* 把当前学生的 grade 字符串写入 buf。 */
        snprintf(buf, buf_size, "%s", s->grade);
        /* 结束年级分支，防止继续落入下一个 case。 */
        break;
        /* case 班级字段：本格显示学生班级字符串。 */
    case UI_FIELD_CLASS:
        /* 把当前学生的 class_name 字符串写入 buf。 */
        snprintf(buf, buf_size, "%s", s->class_name);
        /* 结束班级分支，防止继续落入下一个 case。 */
        break;
        /* case 学号字段：本格显示学生学号。 */
    case UI_FIELD_ID:
        /* 用 PRIu64 格式把 64 位无符号学号 s->id 写入 buf。 */
        snprintf(buf, buf_size, "%" PRIu64, s->id);
        /* 结束学号分支，防止继续落入下一个 case。 */
        break;
        /* case 姓名字段：本格显示学生姓名。 */
    case UI_FIELD_NAME:
        /* 把当前学生的 name 字符串写入 buf。 */
        snprintf(buf, buf_size, "%s", s->name);
        /* 结束姓名分支，防止继续落入下一个 case。 */
        break;
        /* case 性别字段：本格显示学生性别中文文本。 */
    case UI_FIELD_GENDER:
        /* 根据 s->gender 真假分别写入“男”或“女”。 */
        snprintf(buf, buf_size, "%s", s->gender ? "男" : "女");
        /* 结束性别分支，防止继续落入下一个 case。 */
        break;
        /* case 分数段：本格显示学生成绩。 */
    case UI_FIELD_SCORE:
        /* 用保留一位小数的格式把 s->score 浮点成绩写入 buf。 */
        snprintf(buf, buf_size, "%.1f", s->score);
        /* 结束分数分支，防止继续落入下一个 case。 */
        break;
        /* case GPA 字段：本格显示学生绩点。 */
    case UI_FIELD_GPA:
        /* 用保留一位小数的格式把 s->gpa 写入 buf。 */
        snprintf(buf, buf_size, "%.1f", s->gpa);
        /* 结束 GPA 分支，防止继续落入下一个 case。 */
        break;
        /* case 排名段：本格显示学生排名。 */
    case UI_FIELD_RANK:
        /* 用 PRIu16 格式把 16 位无符号排名 s->rank 写入 buf。 */
        snprintf(buf, buf_size, "%" PRIu16, s->rank);
        /* 结束排名分支，防止继续落入下一个 case。 */
        break;
        /* default 兜底分支：遇到未知列索引时清空缓冲区。 */
    default:
        /* 把缓冲区置空，避免未定义的列显示残留或垃圾内容。 */
        buf[0] = '\0';
        /* 结束默认分支的 switch 流程。 */
        break;
    }
}

/**
 * 根据单元格最长文本自动调整列宽；最小列宽 120px。
 */
 /* 定义自动列宽函数：接收当前可见数据行数，根据每列最长文本重新计算列宽。 */
static void ui_auto_columns(int display_count)
{
    /* 声明局部字符缓冲区，用于临时存放每个单元格要显示的文本。 */
    char buf[128];

    /* 外层循环遍历全部列，逐列独立计算应显示的宽度。 */
    for (int c = 0; c < UI_COL_NUM; c++) {
        /* 每一列先从默认列宽出发，后续用实际文本宽度逐渐扩大该值。 */
        int max_w = UI_DEFAULT_COL_W;

        /* 表头 + 当前可见数据行 */
        /* 内层循环遍历表头行和所有当前可见数据行，让这些行的内容参与测宽。 */
        for (int r = 0; r <= display_count; r++) {
            /* 调用填充函数生成第 r 行第 c 列的实际显示文本，测宽前先取得真实内容。 */
            ui_fill_cell_text(r, c, buf, sizeof(buf));
            /* 若该单元格文本为空则跳过此格，空内容不参与列宽计算。 */
            if (buf[0] == '\0') continue;

            /* 声明 LVGL 点结构，用来接收文本测量返回的宽高。 */
            lv_point_t size;
            /* 调用 LVGL 文本测量接口，传入缓冲区、当前界面字体与默认样式参数，计算文本尺寸。 */
            lv_text_get_size(&size, buf, ui_font(), 0, 0,
                /* 补充传入坐标上限与文本标志参数，表示不限制换行宽度且不启用特殊文本标志。 */
                LV_COORD_MAX, LV_TEXT_FLAG_NONE);
            /* 用测量出的文本宽度加上左右内边距，得到该单元格需要的像素宽度。 */
            int w = (int)size.x + CELL_PAD_X * 2;
            /* 若当前文本所需宽度超过本列已有最大值，则更新 max_w，保证列能容纳全部内容。 */
            if (w > max_w) max_w = w;
        }
        /* 把该列最终宽度写回全局列宽数组，供后续界面布局使用。 */
        col_widths[c] = (lv_coord_t)max_w;
    }
}

/**
 * 把所有可见/隐藏单元格背景统一恢复成白色（表头也可保持浅灰？这里统一白色）。
 */
 /* 定义辅助函数：把所有单元格背景统一恢复成白色，用于清除高亮或选中状态。 */
static void ui_reset_all_cells_white(void)
{
    /* 外层行循环覆盖从表头到最大数据行的全部网格位置。 */
    for (int r = 0; r <= UI_MAX_DATA_ROWS; r++) {
        /* 内层列循环遍历每一列，逐个单元格恢复颜色。 */
        for (int c = 0; c < UI_COL_NUM; c++) {
            /* 直接设置指定单元格背景色为白色 0xFFFFFF。 */
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
 /* 定义网格核心刷新函数：负责从数据层取可见行数、重算列宽、重新填充并清理高亮。 */
static void ui_refresh_grid_internal(void)
{
    /* 调用数据层获取当前应显示的数据行数，数值会受筛选和搜索影响。 */
    int display_count = data_get_display_count();
    /* 若数据层返回负数则按 0 处理，避免出现无效的负行数。 */
    if (display_count < 0) display_count = 0;
    /* 若可见行数超过界面支持的最大数据行数则截断，防止数组越界。 */
    if (display_count > UI_MAX_DATA_ROWS) display_count = UI_MAX_DATA_ROWS;

    /* 先重算列宽并摆放位置 */
    /* 先把全部列宽重置为默认值，再重新进行自动测量，避免旧列宽残留。 */
    for (int c = 0; c < UI_COL_NUM; c++) col_widths[c] = UI_DEFAULT_COL_W;
    /* 根据当前可见行数调用自动列宽函数，重算各列宽度。 */
    ui_auto_columns(display_count);
    /* 把新列宽和行列位置应用到网格对象上，完成几何布局。 */
    ui_apply_geometry();

    /* 声明局部缓冲区，在逐格填充文本时复用。 */
    char buf[128];

    /* 外层行循环覆盖表头与所有可能的数据行位置，统一处理显示或隐藏。 */
    for (int r = 0; r <= UI_MAX_DATA_ROWS; r++) {
        /* 判断当前行是否大于实际可见行数，是则属于需要隐藏的空白行。 */
        bool is_data_row_hidden = (r > display_count);

        /* 内层列循环逐格设置文本和隐藏标志。 */
        for (int c = 0; c < UI_COL_NUM; c++) {
            /* 生成该行该列应显示的文本放入 buf，可能来自表头或数据层。 */
            ui_fill_cell_text(r, c, buf, sizeof(buf));
            /* 把文本设置到对应 LVGL 标签上，真正更新界面显示内容。 */
            lv_label_set_text(grid_cells[r][c], buf);

            /* 若当前行在可见范围之外，进入隐藏分支。 */
            if (is_data_row_hidden) {
                /* 给单元格添加 LVGL 隐藏标志，使这行不再显示。 */
                lv_obj_add_flag(grid_cells[r][c], LV_OBJ_FLAG_HIDDEN);
            }
            /* 否则当前行在可见范围内，进入显示分支。 */
            else {
                /* 清除单元格的隐藏标志，让该行恢复显示。 */
                lv_obj_clear_flag(grid_cells[r][c], LV_OBJ_FLAG_HIDDEN);
            }
        }
    }

    /* 刷新后不保留临时高亮/选中状态 */
    /* 将上次选中的行号和列号同时置为 -1，表示当前没有选中项。 */
    last_sel_row = last_sel_col = -1;
    /* 把上次选中的单元格对象指针清空，防止继续引用旧对象。 */
    last_sel_cell = NULL;
    /* 把所有单元格背景重置为白色，清除上一次遗留的选中或搜索高亮。 */
    ui_reset_all_cells_white();
    /* 调用 LVGL 使列表对象失效并重绘，让刷新结果立刻出现在屏幕上。 */
    lv_obj_invalidate(obj_list);
}

/* ======================== 顶部按钮回调 ======================== */

/**
 * 搜索框回车：调用外部 data_find_matches() 取得匹配行，整行黄色高亮 2 秒。
 */
 /* 定义搜索框回车回调：读取关键词、查找匹配行并整行黄色高亮。 */
static void on_search_ready(lv_event_t* e)
{
    /* 显式忽略事件参数，避免编译器产生未使用参数告警。 */
    (void)e;
    /* 声明关键词缓冲区，最多保存 255 个字符加结束符。 */
    char keyword[256];

    /* 搜索前先保存并关闭正在编辑的单元格，防止刷新覆盖用户输入。 */
    ui_save_and_close_edit();
    /* 从搜索输入框取出当前文本存入 keyword，作为搜索关键词。 */
    ui_get_ta_text(search_ta, keyword, sizeof(keyword));

    /* 清掉上一次高亮定时器 */
    /* 若已存在上次搜索的高亮定时器，进入清理分支。 */
    if (search_timer) {
        /* 删除旧的高亮定时器，避免多次搜索时旧回调继续运行。 */
        lv_timer_delete(search_timer);
        /* 把定时器全局指针置空，表示当前没有进行中的高亮定时。 */
        search_timer = NULL;
    }

    /* 声明整数数组保存数据层返回的匹配数据行索引。 */
    int matches[UI_MAX_DATA_ROWS];
    /* 调用数据层搜索函数，把匹配行写入 matches 并返回匹配数量。 */
    int match_count = data_find_matches(keyword, matches, UI_MAX_DATA_ROWS);
    /* 若匹配数量超过数组容量则截断，保证后续循环不会越界。 */
    if (match_count > UI_MAX_DATA_ROWS) match_count = UI_MAX_DATA_ROWS;

    /* 搜索后先刷新整个网格，清掉旧的文本和临时高亮状态。 */
    ui_refresh_grid_internal();

    /* 遍历全部匹配的数据行，准备为它们整行设置高亮。 */
    for (int i = 0; i < match_count; i++) {
        /* 取出当前匹配项对应的数据行号。 */
        int display_row = matches[i];
        /* 若行号越界则跳过该项，避免访问不存在的网格单元格。 */
        if (display_row < 0 || display_row >= UI_MAX_DATA_ROWS) continue;
        /* 将数据行号加 1 得到网格行号，因为第 0 行是表头，数据从第 1 行开始。 */
        int grid_row = display_row + 1;
        /* 对匹配行的每一列分别处理，以实现整行高亮。 */
        for (int c = 0; c < UI_COL_NUM; c++) {
            /* 把该行当前列的背景设置为黄色 0xFFFF00，标记为搜索命中。 */
            ui_set_cell_bg(grid_cells[grid_row][c], 0xFFFF00);
        }
    }

    /* 只要有匹配项就创建一次性定时器，让高亮在指定时间后自动消失。 */
    if (match_count > 0) {
        /* 创建 LVGL 定时器，回调指向清除高亮函数，延时为搜索高亮保留毫秒数。 */
        search_timer = lv_timer_create(ui_clear_search_hilight,
            /* 补充定时器创建参数：不传递用户数据，并结束 lv_timer_create 调用。 */
            SEARCH_HILIGHT_MS, NULL);
        /* 若定时器创建成功则设置只重复一次，确保高亮到时后自动结束。 */
        if (search_timer) lv_timer_set_repeat_count(search_timer, 1);
    }
}

/**
 * 搜索高亮时间到：恢复白色网格。
 */
 /* 定义搜索高亮定时器回调：清空定时器指针并刷新网格恢复白色。 */
static void ui_clear_search_hilight(lv_timer_t* timer)
{
    /* 显式忽略定时器参数，避免未使用参数告警。 */
    (void)timer;
    /* 先把全局定时器指针置空，表示高亮定时器已经结束。 */
    search_timer = NULL;
    /* 若列表对象仍存在则重新刷新网格，把所有黄色高亮恢复为白色。 */
    if (obj_list) ui_refresh_grid_internal();
}

/* ---------- 筛选 ---------- */

/* 定义关闭一级和二级筛选列表的函数，用于筛选操作结束后的清理。 */
static void ui_close_filter_lists(void)
{
    /* 若二级筛选列表对象存在，进入删除分支。 */
    if (filter_list2) {
        /* 删除二级列表对象，释放其占用的 LVGL 资源。 */
        lv_obj_delete(filter_list2);
        /* 把二级列表指针置空，避免后续重复删除或访问悬空指针。 */
        filter_list2 = NULL;
    }
    /* 若一级筛选列表对象存在，进入删除分支。 */
    if (filter_list1) {
        /* 删除一级列表对象，关闭已展开的筛选入口。 */
        lv_obj_delete(filter_list1);
        /* 把一级列表指针置空，保持状态一致。 */
        filter_list1 = NULL;
    }
}

/* 定义二级筛选项点击回调：从编码中解析字段与选项并应用筛选。 */
static void on_filter_option_clicked(lv_event_t* e)
{
    /* 从 LVGL 事件用户数据中取出打包的整数编码，该编码同时包含字段号和选项号。 */
    int code = (int)(intptr_t)lv_event_get_user_data(e);
    /* 编码除以 64 得到字段号，用于确定要筛选哪个字段。 */
    int field = code / 64;
    /* 编码对 64 取余得到选项下标，用于确定选中第几个筛选项。 */
    int index = code % 64;

    /* 应用筛选前先保存并关闭编辑中的单元格，避免数据冲突。 */
    ui_save_and_close_edit();
    /* 校验字段号是否在合法字段范围内，并准备校验选项下标。 */
    if (field >= 0 && field < UI_FIELD_COUNT &&
        /* 继续校验选项下标小于 64 且对应选项文本非空，全部通过才执行筛选。 */
        index >= 0 && index < 64 && filter_opt_buf[index][0] != '\0') {
        /* 调用数据层应用指定字段的具体筛选值，更新底层数据集合。 */
        data_apply_filter(field, filter_opt_buf[index]);
    }
    /* 筛选应用完成后关闭两个筛选列表，让界面恢复简洁。 */
    ui_close_filter_lists();
    /* 重新刷新网格，使新的筛选结果立即展示出来。 */
    ui_refresh_grid_internal();
}

/**
 * 第一级筛选列表：年级/班级/性别/分数/全部。
 * “全部”直接清除筛选；其它打开右侧二级列表。
 */
 /* 定义一级筛选类别点击回调，处理年级、班级、性别、分数和全部入口。 */
static void on_filter_kind_clicked(lv_event_t* e)
{
    /* 从事件用户数据中取出被点击的筛选类别编号。 */
    int kind = (int)(intptr_t)lv_event_get_user_data(e);

    /* 切换筛选前先保存并关闭正在编辑的单元格，防止筛选刷新丢失输入。 */
    ui_save_and_close_edit();

    /* 若点击的是“全部”类别，进入清除筛选的分支。 */
    if (kind == UI_FILTER_ALL) {
        /* 调用数据层清除当前筛选条件，恢复显示全部学生。 */
        data_clear_filter();
        /* 清除筛选后关闭所有筛选下拉列表。 */
        ui_close_filter_lists();
        /* 刷新网格以显示未筛选的完整数据。 */
        ui_refresh_grid_internal();
        /* 处理完“全部”后直接返回，不再继续创建二级列表。 */
        return;
    }

    /* 换算成 StudentCSV 字段号 */
    /* 进入按筛选类别分发到具体字段的 switch 分支。 */
    switch (kind) {
        /* 年级类别对应学生数据的年级字段，记录后跳出分支。 */
    case UI_FILTER_GRADE:  filter_field = UI_FIELD_GRADE;  break;
        /* 班级类别对应班级字段，记录到全局筛选字段后跳出。 */
    case UI_FILTER_CLASS:  filter_field = UI_FIELD_CLASS;  break;
        /* 性别类别对应性别字段，用于打开性别选项列表。 */
    case UI_FILTER_GENDER: filter_field = UI_FIELD_GENDER; break;
        /* 分数类别对应分数字段，用于打开分数筛选范围列表。 */
    case UI_FILTER_SCORE:  filter_field = UI_FIELD_SCORE;  break;
        /* 遇到无法识别的类别直接返回，不改变当前筛选字段。 */
    default: return;
    }

    /* 向数据层请求该字段可用的筛选项，最多 64 项，存入全局选项缓冲区。 */
    int n = data_get_filter_options(filter_field, filter_opt_buf, 64);
    /* 若返回数量超过 64 则截断，防止后续访问越界。 */
    if (n > 64) n = 64;

    /* 关闭旧的二级列表 */
    /* 若已存在旧的二级列表，先删除再重建，避免多个列表叠加。 */
    if (filter_list2) {
        /* 删除旧的二级列表对象，释放界面资源。 */
        lv_obj_delete(filter_list2);
        /* 把二级列表指针置空，等待重新创建。 */
        filter_list2 = NULL;
    }

    /* 若该字段没有可选筛选项则直接返回，不弹出空列表。 */
    if (n <= 0) return;

    /* 二级列表放在一级列表右侧 */
    /* 获取当前活动屏幕作为父对象，用于把二级列表创建在正确层级。 */
    lv_obj_t* parent = lv_screen_active();
    /* 创建 LVGL 列表对象作为二级选项列表，并保存到全局指针供后续关闭。 */
    filter_list2 = lv_list_create(parent);
    /* 把二级列表移动到右侧固定坐标 (900,100)，与一级列表并排显示。 */
    lv_obj_set_pos(filter_list2, 510, 10);
    /* 设置二级列表的尺寸为 170x300，为多个选项留出滚动空间。 */
    lv_obj_set_size(filter_list2, 200, 250);
    /* 将二级列表背景设为白色，与整体界面风格保持一致。 */
    lv_obj_set_style_bg_color(filter_list2, lv_color_hex(0xFFFFFF), 0);
    /* 设置二级列表边框宽度为 1 像素，让列表边界清晰。 */
    lv_obj_set_style_border_width(filter_list2, 1, 0);
    /* 设置边框颜色为灰色，使列表与浅色背景之间有适度区分。 */
    lv_obj_set_style_border_color(filter_list2, lv_color_hex(0x888888), 0);
    /* 把二级列表的文本字体设置为界面统一字体，保证中文正常显示。 */
    lv_obj_set_style_text_font(filter_list2, ui_font(), 0);
    /* 开启事件冒泡标志，使列表内的点击事件可以继续向上层传播。 */
    lv_obj_add_flag(filter_list2, LV_OBJ_FLAG_EVENT_BUBBLE);
    /* 把二级列表移到前台，避免被其它界面对象遮挡。 */
    lv_obj_move_foreground(filter_list2);

    /* 遍历该字段的全部筛选项，为每个选项创建一个按钮。 */
    for (int i = 0; i < n; i++) {
        /* 在二级列表中新增带文字的按钮，文本为该筛选项名称。 */
        lv_obj_t* btn = lv_list_add_button(filter_list2, NULL, filter_opt_buf[i]);
        /* 仅当按钮创建成功时才注册事件与设置样式，防止空指针操作。 */
        if (btn) {
            /* 为按钮注册点击回调，传入字段号与选项序号编码作为用户数据。 */
            lv_obj_add_event_cb(btn, on_filter_option_clicked, LV_EVENT_CLICKED,
                /* 补充编码细节：使用 filter_field*64+i，使回调能还原字段号和选项号。 */
                (void*)(intptr_t)(filter_field * 64 + i));
            /* 把按钮文本字体设置为界面统一字体，保证文字渲染一致。 */
            lv_obj_set_style_text_font(btn, ui_font(), 0);
            /* 给按钮开启事件冒泡标志，使点击事件能按预期向上传递。 */
            lv_obj_add_flag(btn, LV_OBJ_FLAG_EVENT_BUBBLE);
        }
    }
}

/* 筛选按钮的 LVGL 点击回调函数签名；用户点击界面上的“筛选”控件时进入此函数。 */
static void on_filter_clicked(lv_event_t* e)
{
    /* 显式忽略 LVGL 传入的事件参数 e，避免编译器报告未使用参数，同时本回调不需要读取事件细节。 */
    (void)e;
    /* 先保存并关闭当前可能正在进行的表格编辑，防止弹出筛选列表时残留编辑状态或导致数据不一致。 */
    ui_save_and_close_edit();

    /* 若已打开则先关闭，实现“再点一次收起” */
    /* 若全局筛选列表控件 filter_list1 已存在，说明列表已经展开，再次点击按钮应执行收起逻辑。 */
    if (filter_list1) {
        /* 调用关闭函数销毁所有已展开的筛选列表并清理相关全局指针，实现“再点一次收起”的交互。 */
        ui_close_filter_lists();
        /* 收起后立即结束回调，不再继续创建新的筛选列表。 */
        return;
    }

    /* 获取当前活动屏幕作为父容器，保证筛选下拉列表创建在用户当前可见的屏幕上。 */
    lv_obj_t* parent = lv_screen_active();
    /* 在父屏幕上新建一个 LVGL 列表并保存到全局 filter_list1，供之后判断展开状态和统一关闭使用。 */
    filter_list1 = lv_list_create(parent);
    /* 把筛选列表定位到屏幕坐标 (700,100)，使其出现在表格右上方的固定下拉位置。 */
    lv_obj_set_pos(filter_list1, 400, 10);
    /* 将列表尺寸设为宽 170、高 300，让 5 个筛选选项完整显示且尽量不遮挡主要数据区域。 */
    lv_obj_set_size(filter_list1, 100, 250);
    /* 列表背景设为白色，使下拉菜单在彩色界面中保持清晰易读。 */
    lv_obj_set_style_bg_color(filter_list1, lv_color_hex(0xFFFFFF), 0);
    /* 为列表设置 1 像素边框，让筛选菜单与页面内容之间有明确边界。 */
    lv_obj_set_style_border_width(filter_list1, 1, 0);
    /* 边框颜色使用灰色，形成柔和但可见的分隔，避免纯白背景融为一体。 */
    lv_obj_set_style_border_color(filter_list1, lv_color_hex(0x888888), 0);
    /* 列表文字统一使用 UI 自定义字体，保证中文筛选项显示正常且风格一致。 */
    lv_obj_set_style_text_font(filter_list1, ui_font(), 0);
    /* 给列表加上事件冒泡标志，使列表内部的点击行为可以向上传播，配合程序统一的关闭/背景点击机制。 */
    lv_obj_add_flag(filter_list1, LV_OBJ_FLAG_EVENT_BUBBLE);
    /* 把筛选列表移到前台，避免表格或其他控件遮挡刚弹出的下拉菜单。 */
    lv_obj_move_foreground(filter_list1);

    /* 定义 5 个筛选选项文本：年级、班级、性别、分数、全部，覆盖主要筛选维度并提供恢复全部数据的入口。 */
    const char* texts[] = { "年级", "班级", "性别", "分数", "全部" };
    /* 循环遍历 5 个筛选项，逐个创建按钮并复用同一个点击回调，用循环序号 i 区分选项类型。 */
    for (int i = 0; i < 5; i++) {
        /* 在筛选列表中创建一个带文本的按钮项，返回按钮对象供后续绑定事件和设置样式。 */
        lv_obj_t* btn = lv_list_add_button(filter_list1, NULL, texts[i]);
        /* 检查按钮是否成功创建，避免按钮为空时继续调用 LVGL API 导致空指针崩溃。 */
        if (btn) {
            /* 为筛选按钮注册点击回调 on_filter_kind_clicked，点击某个筛选项时触发对应的筛选逻辑。 */
            lv_obj_add_event_cb(btn, on_filter_kind_clicked, LV_EVENT_CLICKED,
                /* 把循环序号 i 转换为指针作为事件用户数据传入，回调收到后可据此判断用户点击的是哪一项筛选。 */
                (void*)(intptr_t)i);
            /* 按钮文字也设置成 UI 自定义字体，确保中文选项在按钮上正常显示。 */
            lv_obj_set_style_text_font(btn, ui_font(), 0);
            /* 让按钮点击事件冒泡到父列表，便于统一处理点击外部收起筛选列表等交互。 */
            lv_obj_add_flag(btn, LV_OBJ_FLAG_EVENT_BUBBLE);
        }
    }
}

/* ---------- 帮助 ---------- */

/* 帮助图片设置函数签名；index 表示要显示第几张帮助图，负责把对应图片源设置到帮助按钮上。 */
static void ui_set_help_image(int index)
{
    /* 若帮助图片按钮尚未创建或已被删除，直接返回，避免对空指针进行操作。 */
    if (help_imagebtn == NULL) return;
    /* 调用数据层函数取得指定索引的帮助图片源，图片源可能来自内存或文件资源。 */
    const void* src = ui_help_get_image_src(index);
    /* 若取到的图片源为空，直接返回，防止把无效源设置给图片按钮。 */
    if (src == NULL) return;

    /* 把帮助图片按钮在“松开”状态下显示的图标设为该帮助图，呈现正常可见的帮助内容。 */
    lv_imagebutton_set_src(help_imagebtn, LV_IMAGEBUTTON_STATE_RELEASED, NULL, src, NULL);
    /* 同时设置“按下”状态下的图标，让用户按压时图片仍有内容反馈，不会变成空白按钮。 */
    lv_imagebutton_set_src(help_imagebtn, LV_IMAGEBUTTON_STATE_PRESSED, NULL, src, NULL);
}

/* 点击帮助图片按钮的回调函数签名；用于在浏览多张帮助图时切换下一张或关闭帮助。 */
static void on_help_image_clicked(lv_event_t* e)
{
    /* 忽略 LVGL 事件参数，本回调只需要维护全局帮助索引和按钮状态。 */
    (void)e;
    /* 若帮助图片按钮已被删除则直接返回，防止继续访问已销毁的控件。 */
    if (help_imagebtn == NULL) return;

    /* 帮助索引加一，表示用户请求查看下一张帮助图片。 */
    help_index++;
    /* 若新索引已到达或超过帮助图总数，说明当前是最后一张，点击后应结束帮助浏览。 */
    if (help_index >= help_count) {
        /* 已是最后一张，点击后删除帮助图片按钮 */
        /* 删除帮助图片按钮控件，把帮助展示区域从界面上移除。 */
        lv_obj_delete(help_imagebtn);
        /* 将全局 help_imagebtn 置空，避免留下悬空指针，同时用于后续判断帮助是否已关闭。 */
        help_imagebtn = NULL;
        /* 把帮助索引重置为 0，下次打开帮助时从第一张图开始。 */
        help_index = 0;
        /* 把帮助图片数量清零，表示当前没有正在展示的帮助图集合。 */
        help_count = 0;
    }
    else {
        /* 还没到最后一张时，用更新后的索引重新设置帮助按钮上的图片，切换到下一张帮助内容。 */
        ui_set_help_image(help_index);
    }
}

/* 点击“帮助”按钮的回调函数签名；用于打开并浏览帮助图片。 */
static void on_help_clicked(lv_event_t* e)
{
    /* 忽略 LVGL 事件参数，帮助打开逻辑只依赖全局状态和界面对象。 */
    (void)e;
    /* 打开帮助前先保存并关闭正在编辑的单元格，避免编辑浮层与帮助图片相互遮挡或状态冲突。 */
    ui_save_and_close_edit();

    /* 若帮助图片按钮已存在说明帮助已处于打开状态，重复点击直接忽略，防止堆叠多个帮助图。 */
    if (help_imagebtn != NULL) return;   /* 已存在则忽略 */

    /* 从数据层获取帮助图片总数并保存到全局，用于判断浏览到最后一张的结束条件。 */
    help_count = ui_help_get_image_count();
    /* 若没有可用的帮助图片则直接返回，不创建无内容的帮助按钮。 */
    if (help_count <= 0) return;

    /* 把帮助索引初始化为 0，准备从第一张帮助图开始展示。 */
    help_index = 0;
    /* 在当前活动屏幕上创建图片按钮并保存到全局 help_imagebtn，作为帮助图的显示载体。 */
    help_imagebtn = lv_imagebutton_create(lv_screen_active());
    /* 把帮助图片按钮放在屏幕左上角 (0,0)，覆盖顶部用于展示帮助条。 */
    lv_obj_set_pos(help_imagebtn, 0, 0);
    /* 将帮助图片按钮尺寸设为宽 1080、高 80，对应屏幕顶部帮助栏的显示范围。 */
    lv_obj_set_size(help_imagebtn, 1080, 80);
    /* 给帮助图片按钮加事件冒泡标志，使点击行为能按程序统一规则传播处理。 */
    lv_obj_add_flag(help_imagebtn, LV_OBJ_FLAG_EVENT_BUBBLE);
    /* 把帮助图片按钮移到最前，确保它显示在所有界面控件之上。 */
    lv_obj_move_foreground(help_imagebtn);
    /* 给帮助图片按钮绑定点击回调，点击图片即可查看下一张或关闭帮助。 */
    lv_obj_add_event_cb(help_imagebtn, on_help_image_clicked, LV_EVENT_CLICKED, NULL);
    /* 初始调用设置函数显示第 0 张帮助图。 */
    ui_set_help_image(0);
}

/* ---------- 导入 / 导出 ---------- */

/* 路径输入框回车事件回调签名；用户在导入/导出路径框中按回车后触发。 */
static void on_path_ready(lv_event_t* e)
{
    /* 忽略 LVGL 事件参数，路径处理只需要文本框内容和全局 action 标识。 */
    (void)e;
    /* 若路径输入框已不存在则直接返回，防止读取已释放控件的文本。 */
    if (path_ta == NULL) return;

    /* 在栈上分配 1024 字节路径缓冲区，足以容纳常见 CSV 文件路径并留出结尾空字符空间。 */
    char path[1024];
    /* 先把全局操作类型保存到局部 action，因为下面会删除输入框并清空全局值，后续分支仍需要此类型。 */
    int action = path_action;

    /* 从当前路径输入框中读取用户输入的路径到 path 缓冲区，函数内部会做长度限制，避免越界。 */
    ui_get_ta_text(path_ta, path, sizeof(path));

    /* 回车后删除路径输入框，完成一次路径输入并释放对应 UI 控件。 */
    lv_obj_delete(path_ta);
    /* 把全局 path_ta 置空，避免引用已删除对象，并标识当前没有活动路径输入框。 */
    path_ta = NULL;
    /* 清空全局 path_action，防止下一次打开输入框前误用旧的导入/导出操作类型。 */
    path_action = 0;

    /* 若保存下来的 action 为 1，表示用户执行的是 CSV 导入操作。 */
    if (action == 1) {
        /* 调用数据层函数按 path 导入 CSV 文件；返回值 r 表示成功导入的条数，负数表示失败。 */
        int r = data_import_csv(path);
        /* 导入后立即刷新网格内部数据视图，让导入的新数据及时显示在表格上。 */
        ui_refresh_grid_internal();
        /* 若返回值为非负，说明导入成功，进入成功提示分支。 */
        if (r >= 0) {
            /* 分配 256 字节消息缓冲区，用来拼接包含导入条数的成功提示文字。 */
            char msg[256];
            /* 把“导入成功：N 条数据”格式化到 msg 中，让用户看到实际导入数量。 */
            snprintf(msg, sizeof(msg), "导入成功：%d 条数据", r);
            /* 调用界面 toast 函数弹出导入成功提示。 */
            ui_show_toast(msg);
        }
        else {
            /* 导入返回负值时提示用户检查文件路径是否正确、文件是否可读。 */
            ui_show_toast("导入失败，请检查文件路径");
        }
    }
    /* 若 action 为 2，表示用户执行的是 CSV 导出操作。 */
    else if (action == 2) {
        /* 调用数据层函数把当前学生数据导出到 path；返回值 r 表示是否成功。 */
        int r = data_export_csv(path);
        /* 导出返回非负值说明文件写入成功，进入成功提示分支。 */
        if (r >= 0) {
            /* 分配 1200 字节缓冲区，因为成功提示要包含完整导出路径，路径可能较长。 */
            char msg[1200];
            /* 把“文件已导出至'实际路径'”拼进提示消息，告知用户保存位置。 */
            snprintf(msg, sizeof(msg), "文件已导出至'%s'", path);
            /* 弹出导出成功 toast，反馈导出结果。 */
            ui_show_toast(msg);
        }
        else {
            /* 导出失败时提示用户检查保存路径是否有效、目录是否可写。 */
            ui_show_toast("导出失败，请检查保存路径");
        }
    }
}

/**
 * 创建与按钮中心对齐的路径输入框。
 * @param center_x 按钮中心 X 坐标
 * @param action   1=导入, 2=导出
 */
 /* 创建路径输入框的函数签名；center_x 为触发按钮中心 X，action 表示本次操作是导入(1)还是导出(2)。 */
static void ui_open_path_ta(int action)
{
    /* 打开路径输入框前先保存并关闭当前编辑，避免输入框和表格编辑同时占用界面焦点。 */
    ui_save_and_close_edit();

    /* 同一时间只保留一个路径输入框 */
    /* 若已存在路径输入框，先删除旧框，保证同一时间只有一个路径输入框。 */
    if (path_ta) {
        /* 销毁旧的路径输入框控件，清理上一次输入界面。 */
        lv_obj_delete(path_ta);
        /* 将旧输入框全局指针置空，之后创建新输入框时不会引用已删除对象。 */
        path_ta = NULL;
    }

    /* 把本次操作类型存入全局 path_action，供回车回调在 on_path_ready 中判断导入或导出。 */
    path_action = action;
    /* 在当前活动屏幕上创建单行文本框并存入全局 path_ta，作为路径输入控件。 */
    path_ta = lv_textarea_create(lv_screen_active());
    /* 让输入框展示在(400,10)处 */
    lv_obj_set_pos(path_ta, 400, 10);
    /* 设置 2 像素边框。 */
    lv_obj_set_style_border_width(path_ta, 2, 0);
    /* 设置边框颜色；不想要颜色可以删掉这一行。 */
    lv_obj_set_style_border_color(path_ta, lv_color_hex(0x333333), 0);
    /* 清除全部内边距，保证内边缘为 0。 */
    lv_obj_set_style_pad_all(path_ta, 0, 0);
    /* 使用统一常量设置输入框宽高，保证导入和导出弹出框尺寸一致。 */
    lv_obj_set_size(path_ta, PATH_TA_W, PATH_TA_H);
    /* 把文本框设为单行模式，因为路径不应换行，回车键专门用于提交路径。 */
    lv_textarea_set_one_line(path_ta, true);
    /* 限制最大输入长度为 1023 字符，与后面 1024 字节缓冲区配合防止路径溢出。 */
    lv_textarea_set_max_length(path_ta, 1023);
    /* 按 action 显示不同占位提示，导入时提示输入 CSV 路径，导出时提示输入导出路径。 */
    lv_textarea_set_placeholder_text(path_ta, action == 1 ? "输入 CSV 文件路径，回车导入" : "输入导出文件路径，回车导出");
    /* 允许点击文本框任意位置移动光标，方便修改路径中间内容。 */
    lv_textarea_set_cursor_click_pos(path_ta, true);
    /* 文本框正文使用 UI 字体，保证中文路径和提示文字正常渲染。 */
    lv_obj_set_style_text_font(path_ta, ui_font(), 0);
    /* 占位符文本也设置为 UI 字体，避免默认字体显示中文占位提示时出现乱码。 */
    lv_obj_set_style_text_font(path_ta, ui_font(), LV_PART_TEXTAREA_PLACEHOLDER);
    /* 给输入框加事件冒泡标志，使其与程序中点击外部收起/保存等统一交互兼容。 */
    lv_obj_add_flag(path_ta, LV_OBJ_FLAG_EVENT_BUBBLE);
    /* 把路径输入框移到最前，避免被顶部按钮或其他控件遮挡。 */
    lv_obj_move_foreground(path_ta);
    /* 给文本框绑定 LV_EVENT_READY（单行文本框回车）事件到 on_path_ready，实现回车后立即导入或导出。 */
    lv_obj_add_event_cb(path_ta, on_path_ready, LV_EVENT_READY, NULL);

    /* 若系统键盘分组 kb_group 存在，说明程序启用了实体/虚拟键盘输入支持，需要把输入框纳入分组。 */
    if (kb_group) {
        /* 把路径输入框加入键盘焦点分组，使键盘可在不同输入控件间切换焦点。 */
        lv_group_add_obj(kb_group, path_ta);
        /* 让路径输入框立即获得焦点，用户打开后可直接开始输入路径。 */
        lv_group_focus_obj(path_ta);
        /* 把键盘分组设为编辑模式，让键盘按键直接输入到文本框而不是用于导航。 */
        lv_group_set_editing(kb_group, true);
    }
}

/* “导入”按钮的点击回调签名；点击后打开导入路径输入框。 */
static void on_import_clicked(lv_event_t* e)
{
    /* 忽略 LVGL 事件参数，导入按钮固定使用预设的中心坐标和导入 action。 */
    (void)e;
    /* 导入按钮中心 X = 880 + 30 = 910 */
    /* 以按钮中心 X=910、action=1 打开路径输入框，等待用户输入待导入 CSV 的路径。 */
    ui_open_path_ta(1);
}

/* “导出”按钮的点击回调签名；点击后打开导出路径输入框。 */
static void on_export_clicked(lv_event_t* e)
{
    /* 忽略 LVGL 事件参数，导出按钮固定使用预设的中心坐标和导出 action。 */
    (void)e;
    /* 导出按钮中心 X = 950 + 30 = 980 */
    /* 以按钮中心 X=980、action=2 打开路径输入框，等待用户输入导出保存路径。 */
    ui_open_path_ta(2);
}

/* ---------- 删除 ---------- */

/* 删除模式切换按钮的点击回调签名；控制是否进入“点击网格即删除”的模式。 */
static void on_delete_button_clicked(lv_event_t* e)
{
    /* 忽略 LVGL 事件参数，本回调只需翻转删除模式开关并更新按钮颜色。 */
    (void)e;
    /* 切换删除模式前先保存并关闭当前编辑，防止删除状态与编辑状态同时存在造成误操作。 */
    ui_save_and_close_edit();
    /* 翻转 delete_enabled 开关，原来关闭则开启删除模式，原来开启则关闭删除模式。 */
    delete_enabled = !delete_enabled;

    /* 若删除模式已开启，进入状态提示分支。 */
    if (delete_enabled) {
        /* 开启状态：红色底表示当前点击网格会进入删除确认 */
        /* 把删除按钮背景设为浅红色，让用户明确知道当前点击表格会进入删除确认流程。 */
        lv_obj_set_style_bg_color(btn_delete, lv_color_hex(0xFF5555), 0);
    }
    else {
        /* 关闭删除模式时把删除按钮恢复为白色背景，表示回到普通选择/编辑模式。 */
        lv_obj_set_style_bg_color(btn_delete, lv_color_hex(0xFFFFFF), 0);
    }
}

/* 删除确认弹窗“取消”按钮的回调签名；取消后关闭弹窗且不删除数据。 */
static void on_delete_cancel(lv_event_t* e)
{
    /* 忽略 LVGL 事件参数，取消逻辑只需要操作全局弹窗对象。 */
    (void)e;
    /* 若删除确认弹窗 delete_popup 仍存在，则将其关闭。 */
    if (delete_popup) {
        /* 销毁删除确认弹窗控件。 */
        lv_obj_delete(delete_popup);
        /* 把弹窗全局指针置空，标记弹窗已关闭并避免悬空引用。 */
        delete_popup = NULL;
    }
}

/* 删除确认弹窗“确定”按钮的回调签名；确认后真正执行学生数据删除。 */
static void on_delete_confirm(lv_event_t* e)
{
    /* 忽略 LVGL 事件参数，确认逻辑依赖之前记录的待删除学生索引。 */
    (void)e;
    /* 仅当待删除学生索引 delete_student_index 有效（非负）时才执行删除，防止误删无效记录。 */
    if (delete_student_index >= 0) {
        /* 调用数据层删除函数，按索引删除该学生对应的完整记录。 */
        data_delete_student(delete_student_index);
        /* 删除后刷新表格内部网格，使界面中对应的行立即消失。 */
        ui_refresh_grid_internal();
    }
    /* 无论删除动作是否执行，只要确认弹窗仍存在就关闭它，完成整个确认流程。 */
    if (delete_popup) {
        /* 销毁删除确认弹窗控件。 */
        lv_obj_delete(delete_popup);
        /* 把弹窗全局指针置空，表示当前没有打开的删除确认弹窗。 */
        delete_popup = NULL;
    }
}

/**
 * 显示居中的删除确认弹窗。
 */
 /* 显示删除确认弹窗的函数签名；student_index 为待删除学生在数据数组中的索引。 */
static void show_delete_popup(int student_index)
{
    /* 若已有删除确认弹窗存在，先删除旧弹窗，避免连续触发时屏幕上叠加多个确认弹窗。 */
    if (delete_popup) {
        /* 先销毁可能已存在的删除确认弹窗对象，避免重复弹窗叠加。 */
        lv_obj_delete(delete_popup);
        /* 将全局弹窗指针置空，防止悬空引用后续误删已销毁对象。 */
        delete_popup = NULL;
    }

    /* 取得全部学生记录数组指针，用于在弹窗标题中显示学生姓名与年级。 */
    StudentCSV* all = data_get_all();
    /* 默认提示文本为空字符串，当取不到学生或索引无效时使用。 */
    const char* who = "";
    /* 用静态缓冲区保存拼好的“姓名(年级)”，使其在函数返回后仍有效。 */
    static char who_buf[256];
    /* 仅当数据数组有效且当前学生索引合法时才尝试拼接学生信息。 */
    if (all && student_index >= 0) {
        /* 把当前学生的姓名和年级按“姓名(年级)”格式写入缓冲区。 */
        snprintf(who_buf, sizeof(who_buf), "%s(%s)", all[student_index].name,
            /* 补充第二个格式化参数即年级字符串，完成整条提示信息的拼接。 */
            all[student_index].grade);
        /* 将 who 指向静态缓冲区，后续标签创建时即可显示该学生信息。 */
        who = who_buf;
    }

    /* 记录即将删除的学生在数据数组中的索引，供确认回调执行删除。 */
    delete_student_index = student_index;
    /* 在活动屏幕上创建删除确认弹窗对象，作为覆盖层显示。 */
    delete_popup = lv_obj_create(lv_screen_active());
    /* 设置弹窗宽高为预定义的 POPUP_W 和 POPUP_H，统一弹窗尺寸。 */
    lv_obj_set_size(delete_popup, POPUP_W, POPUP_H);
    /* 让弹窗在屏幕上居中显示，视觉上作为模态对话框。 */
    lv_obj_center(delete_popup);

    lv_obj_set_style_pad_all(delete_popup, 0, 0);

    /* 将弹窗背景设为白色，与主界面配色协调。 */
    lv_obj_set_style_bg_color(delete_popup, lv_color_hex(0xFFFFFF), 0);
    /* 设置背景完全不透明，避免透过下层界面看到重影。 */
    lv_obj_set_style_bg_opa(delete_popup, LV_OPA_COVER, 0);
    /* 设置 2 像素边框，让弹窗与背景有明确分界。 */
    lv_obj_set_style_border_width(delete_popup, 2, 0);
    /* 边框使用灰色，呈现柔和但清晰的轮廓。 */
    lv_obj_set_style_border_color(delete_popup, lv_color_hex(0x666666), 0);
    /* 设置圆角半径为 8 像素，使弹窗外观更友好。 */
    lv_obj_set_style_radius(delete_popup, 8, 0);
    /* 允许弹窗事件冒泡，便于后续点击背景或父级时处理关闭逻辑。 */
    lv_obj_add_flag(delete_popup, LV_OBJ_FLAG_EVENT_BUBBLE);
    /* 将弹窗移到前台，确保它显示在所有其他对象之上。 */
    lv_obj_move_foreground(delete_popup);

    /* 上方 100px：提示文字 */
    /* 在弹窗中创建标签对象作为提示文字区。 */
    lv_obj_t* tip = lv_label_create(delete_popup);
    /* 分配局部缓冲区用于生成提示文本。 */
    char tip_buf[512];
    /* 拼接删除确认提示，显示要删除的学生姓名（who 字符串）。 */
    snprintf(tip_buf, sizeof(tip_buf), "你确定要删除'%s'的所有信息吗？", who);
    /* 把拼好的提示文本设置到标签上。 */
    lv_label_set_text(tip, tip_buf);
    /* 将提示标签定位到弹窗内坐标 (10,20)，即上方留出边距。 */
    lv_obj_set_pos(tip, 10, 20);
    /* 设置提示标签宽度为弹窗宽度减 20、高度 60，使文字有足够显示区域。 */
    lv_obj_set_size(tip, POPUP_W - 20, 60);
    /* 提示文字居中排列，保证多行或短句视觉整齐。 */
    lv_obj_set_style_text_align(tip, LV_TEXT_ALIGN_CENTER, 0);
    /* 使用统一的 UI 字体，保证中文按程序主题显示。 */
    lv_obj_set_style_text_font(tip, ui_font(), 0);

    /* 下方左右对称：取消(绿) 与 确定(红) */
    /* 创建取消按钮对象，父对象为弹窗。 */
    lv_obj_t* cancel = lv_button_create(delete_popup);
    /* 把取消按钮放到弹窗内 x=150、y=130 的位置，形成左右布局左侧。 */
    lv_obj_set_pos(cancel, 150, 130);
    /* 设置取消按钮尺寸为 80×50，保证可点击区域足够。 */
    lv_obj_set_size(cancel, 80, 50);
    /* 取消按钮背景设为绿色（0x00C853），提示该操作安全。 */
    lv_obj_set_style_bg_color(cancel, lv_color_hex(0x00C853), 0);
    /* 取消按钮背景完全不透明，颜色显示准确。 */
    lv_obj_set_style_bg_opa(cancel, LV_OPA_COVER, 0);
    /* 按钮文字设为白色，与绿色背景形成对比。 */
    lv_obj_set_style_text_color(cancel, lv_color_hex(0xFFFFFF), 0);
    /* 按钮文字使用统一 UI 字体。 */
    lv_obj_set_style_text_font(cancel, ui_font(), 0);
    /* 在取消按钮内创建标签，用于显示按钮文本。 */
    lv_obj_t* cancel_l = lv_label_create(cancel);
    /* 设置标签文字为“取消”。 */
    lv_label_set_text(cancel_l, "取消");
    /* 取消按钮标签同样使用统一字体，避免默认字体造成乱码或样式不一。 */
    lv_obj_set_style_text_font(cancel_l, ui_font(), 0);
    /* 让“取消”文字在按钮内部居中。 */
    lv_obj_center(cancel_l);
    /* 为取消按钮绑定点击事件，点击后调用 on_delete_cancel 关闭弹窗。 */
    lv_obj_add_event_cb(cancel, on_delete_cancel, LV_EVENT_CLICKED, NULL);
    /* 允许取消按钮事件冒泡，保持与其他控件事件机制一致。 */
    lv_obj_add_flag(cancel, LV_OBJ_FLAG_EVENT_BUBBLE);

    /* 创建确定删除按钮，父对象同样为弹窗。 */
    lv_obj_t* confirm = lv_button_create(delete_popup);
    /* 把确定按钮放在 x=270、y=130，与取消按钮左右对称。 */
    lv_obj_set_pos(confirm, 270, 130);
    /* 设置确定按钮尺寸 80×50。 */
    lv_obj_set_size(confirm, 80, 50);
    /* 确定按钮背景设为红色（0xE53935），表示删除危险操作。 */
    lv_obj_set_style_bg_color(confirm, lv_color_hex(0xE53935), 0);
    /* 确定按钮背景不透明。 */
    lv_obj_set_style_bg_opa(confirm, LV_OPA_COVER, 0);
    /* 确定按钮文字设为白色，便于在红底上阅读。 */
    lv_obj_set_style_text_color(confirm, lv_color_hex(0xFFFFFF), 0);
    /* 确定按钮文字使用统一 UI 字体。 */
    lv_obj_set_style_text_font(confirm, ui_font(), 0);
    /* 在确定按钮内创建标签作为文字。 */
    lv_obj_t* confirm_l = lv_label_create(confirm);
    /* 设置标签文字为“确定”。 */
    lv_label_set_text(confirm_l, "确定");
    /* 确定按钮标签使用统一字体。 */
    lv_obj_set_style_text_font(confirm_l, ui_font(), 0);
    /* 让“确定”文字在按钮内居中。 */
    lv_obj_center(confirm_l);
    /* 绑定确定按钮点击事件，点击后调用 on_delete_confirm 真正执行删除。 */
    lv_obj_add_event_cb(confirm, on_delete_confirm, LV_EVENT_CLICKED, NULL);
    /* 允许确定按钮事件冒泡，保持事件链一致。 */
    lv_obj_add_flag(confirm, LV_OBJ_FLAG_EVENT_BUBBLE);
}

/* ---------- Toast 弹窗 ---------- */

/* 定义点击 Toast 时的回调函数，参数为 LVGL 事件对象。 */
static void on_toast_clicked(lv_event_t* e)
{
    /* 将事件参数强制置空使用，消除未使用参数编译警告，因为点击处理不需要事件数据。 */
    (void)e;
    /* 若已有自动关闭定时器在运行，先进入清理分支。 */
    if (toast_timer) {
        /* 删除旧的 toast 定时器，停止之前安排的自动关闭。 */
        lv_timer_delete(toast_timer);
        /* 把定时器指针置空，避免之后访问已删除的定时器。 */
        toast_timer = NULL;
    }
    /* 若屏幕上还显示着旧 toast 对象，则进入清理分支。 */
    if (toast_obj) {
        /* 删除 toast 标签对象，将其从界面移除。 */
        lv_obj_delete(toast_obj);
        /* toast 对象指针置空，避免悬空引用。 */
        toast_obj = NULL;
    }
}

/* 定义 Toast 自动关闭的定时器回调，由 lv_timer 到期触发。 */
static void on_toast_timer(lv_timer_t* timer)
{
    /* 忽略定时器参数以消除编译警告，本回调只需清理 toast。 */
    (void)timer;
    /* 先把定时器全局指针置空，表示当前没有待执行的 toast 定时器。 */
    toast_timer = NULL;
    /* 若 toast 对象仍在屏幕上，则进行删除。 */
    if (toast_obj) {
        /* 删除 toast 标签对象。 */
        lv_obj_delete(toast_obj);
        /* toast 指针置空，防止后续重复删除。 */
        toast_obj = NULL;
    }
}

/* 定义显示 Toast 提示的函数，msg 为要展示的文本。 */
static void ui_show_toast(const char* msg)
{
    /* 若已有 toast 正在显示，先删除旧对象，避免多个提示重叠。 */
    if (toast_obj) {
        /* 删除旧的 toast 标签。 */
        lv_obj_delete(toast_obj);
        /* 旧 toast 指针置空。 */
        toast_obj = NULL;
    }
    /* 若已有自动关闭定时器，先删除旧定时器。 */
    if (toast_timer) {
        /* 删除旧的定时器，避免旧回调影响新提示。 */
        lv_timer_delete(toast_timer);
        /* 定时器指针置空。 */
        toast_timer = NULL;
    }

    /* 在活动屏幕上新建标签对象作为 toast 提示。 */
    toast_obj = lv_label_create(lv_screen_active());
    /* 把调用方传入的提示文本设置到 toast 标签。 */
    lv_label_set_text(toast_obj, msg);
    /* 设置 toast 的宽高为预定义尺寸，使提示框大小统一。 */
    lv_obj_set_size(toast_obj, TOAST_W, TOAST_H);
    /* 将 toast 在屏幕中央显示，吸引用户注意。 */
    lv_obj_center(toast_obj);
    /* toast 背景设为白色，作为浮层提示框。 */
    lv_obj_set_style_bg_color(toast_obj, lv_color_hex(0xFFFFFF), 0);
    /* toast 背景不透明。 */
    lv_obj_set_style_bg_opa(toast_obj, LV_OPA_COVER, 0);
    /* 设置 2 像素边框，让提示框边缘清晰。 */
    lv_obj_set_style_border_width(toast_obj, 2, 0);
    /* 边框用浅灰色，柔和且与内容区分。 */
    lv_obj_set_style_border_color(toast_obj, lv_color_hex(0x888888), 0);
    /* 设置 8 像素圆角。 */
    lv_obj_set_style_radius(toast_obj, 8, 0);
    /* 正文文字设为深灰色，保证可读性。 */
    lv_obj_set_style_text_color(toast_obj, lv_color_hex(0x222222), 0);
    /* 正文使用统一 UI 字体。 */
    lv_obj_set_style_text_font(toast_obj, ui_font(), 0);
    /* 文本居中排列。 */
    lv_obj_set_style_text_align(toast_obj, LV_TEXT_ALIGN_CENTER, 0);
    /* 给 toast 添加可点击与事件冒泡标志，使点击 toast 能触发关闭回调。 */
    lv_obj_add_flag(toast_obj, LV_OBJ_FLAG_CLICKABLE | LV_OBJ_FLAG_EVENT_BUBBLE);
    /* 把 toast 移到前台，避免被网格或其他控件遮挡。 */
    lv_obj_move_foreground(toast_obj);
    /* 绑定点击回调，用户点击 toast 时立即关闭它。 */
    lv_obj_add_event_cb(toast_obj, on_toast_clicked, LV_EVENT_CLICKED, NULL);

    /* 创建单次定时器，延迟 TOAST_MS 后调用 on_toast_timer 自动关闭 toast。 */
    toast_timer = lv_timer_create(on_toast_timer, TOAST_MS, NULL);
    /* 若定时器创建成功，把重复次数设为 1，保证只自动关闭一次。 */
    if (toast_timer) lv_timer_set_repeat_count(toast_timer, 1);
}

/* ======================== 单元格点击 / 编辑 ======================== */

/* 关闭单元格编辑器：删除编辑输入框并重置编辑状态。 */
static void ui_close_edit(void)
{
    /* 若当前存在编辑文本框 edit_ta，则进入清理分支。 */
    if (edit_ta) {
        /* 从界面删除编辑文本框。 */
        lv_obj_delete(edit_ta);
        /* 文本框指针置空。 */
        edit_ta = NULL;
    }
    /* 把当前编辑位置的行列都设为 -1，表示没有正在编辑的单元格。 */
    edit_grid_row = edit_grid_col = -1;
    /* 把当前编辑学生索引设为 -1，表示没有正在编辑的学生。 */
    edit_student_index = -1;
}

/* 保存当前编辑框内容并关闭编辑器。 */
static void ui_save_and_close_edit(void)
{
    /* 若编辑框已不存在，直接返回，无需保存。 */
    if (edit_ta == NULL) return;

    /* 局部缓冲区存放从输入框读取的文本。 */
    char text[256];
    /* 记录正在编辑的学生索引，后续更新用。 */
    int idx = edit_student_index;
    /* 将网格列号转换为数据字段编号：第 0 列是序号列不可编辑，所以列号减 1。 */
    int field = edit_grid_col - 1;   /* 第1列序号不可编辑，能进入编辑则 col>=1 */

    /* 从编辑框中读取用户输入的文本到 text 缓冲区。 */
    ui_get_ta_text(edit_ta, text, sizeof(text));
    /* 关闭并销毁编辑框，避免界面残留输入控件。 */
    ui_close_edit();

    /* 仅在学生索引和字段编号均合法时执行数据更新。 */
    if (idx >= 0 && field >= 0 && field < UI_FIELD_COUNT) {
        /* 调用数据层函数把指定学生第 field 个字段更新为输入文本。 */
        data_update_student_field(idx, field, text);
    }

    /* 若学生列表对象仍存在，刷新网格显示，使编辑结果立即反映到界面。 */
    if (obj_list) ui_refresh_grid_internal();
}

/* 定义文本框编辑完成（LV_EVENT_READY）的回调。 */
static void on_edit_ready(lv_event_t* e)
{
    /* 忽略事件参数。 */
    (void)e;
    /* 编辑完成后保存内容并关闭编辑框。 */
    ui_save_and_close_edit();
}

/* 打开某个网格单元格的编辑器，grid_row/grid_col 为网格行列。 */
static void ui_open_cell_editor(int grid_row, int grid_col)
{
    /* 只能编辑数据行、数据列（col>=1） */
    /* 只允许编辑数据行和数据列：首行表头与第 0 列序号列不可编辑，非法行列直接返回。 */
    if (grid_row <= 0 || grid_col <= 0) return;

    /* 把显示行号（grid_row-1）映射为数据数组中的真实学生索引，支持排序或过滤后的定位。 */
    int idx = data_get_display_index(grid_row - 1);
    /* 若映射不到有效学生索引，说明该行没有对应数据，直接返回。 */
    if (idx < 0) return;

    /* 缓冲区用于保存当前单元格的原始文本，作为编辑框初始内容。 */
    char current[256];
    /* 按当前行列把单元格现有文本填入缓冲区，让用户在原值基础上修改。 */
    ui_fill_cell_text(grid_row, grid_col, current, sizeof(current));

    /* 记录正在编辑的行号，供保存逻辑定位回网格。 */
    edit_grid_row = grid_row;
    /* 记录正在编辑的列号，供保存逻辑判断字段。 */
    edit_grid_col = grid_col;
    /* 记录正在编辑的学生真实索引，供保存逻辑更新数据。 */
    edit_student_index = idx;

    /* 在学生列表对象上创建文本框，作为覆盖在原单元格上的编辑器。 */
    edit_ta = lv_textarea_create(obj_list);
    /* 取当前单元格对象作为定位基准，让编辑器精确覆盖该格。 */
    lv_obj_t* base_cell = grid_cells[grid_row][grid_col];
    /* 将编辑框移到该单元格相同坐标位置，模拟原地编辑。 */
    lv_obj_set_pos(edit_ta, lv_obj_get_x(base_cell), lv_obj_get_y(base_cell));
    /* 把编辑框大小设为该列宽度与标准行高，外观与原单元格一致。 */
    lv_obj_set_size(edit_ta, col_widths[grid_col], UI_ROW_H);
    /* 设置单行模式，学生字段按单行文本编辑。 */
    lv_textarea_set_one_line(edit_ta, true);
    /* 限制最大输入长度 255，与数据字段容量保持一致。 */
    lv_textarea_set_max_length(edit_ta, 255);
    /* 预填当前单元格文本，用户可直接修改。 */
    lv_textarea_set_text(edit_ta, current);
    /* 允许点击光标定位到文本任意位置，方便修改中间内容。 */
    lv_textarea_set_cursor_click_pos(edit_ta, true);
    /* 编辑框文字使用统一 UI 字体。 */
    lv_obj_set_style_text_font(edit_ta, ui_font(), 0);
    /* 文字居中显示，与原单元格风格一致。 */
    lv_obj_set_style_text_align(edit_ta, LV_TEXT_ALIGN_CENTER, 0);
    /* 给编辑框添加事件冒泡标志，使点击文本框不会阻断外层单元格点击逻辑。 */
    lv_obj_add_flag(edit_ta, LV_OBJ_FLAG_EVENT_BUBBLE);
    /* 把编辑框移到前台，确保可正常显示和输入。 */
    lv_obj_move_foreground(edit_ta);
    /* 绑定 READY 事件，用户确认输入后触发 on_edit_ready 保存。 */
    lv_obj_add_event_cb(edit_ta, on_edit_ready, LV_EVENT_READY, NULL);

    /* 若存在键盘组 kb_group（外接键盘或实体按键组），则进入键盘支持分支。 */
    if (kb_group) {
        /* 把编辑框加入键盘焦点组，使其可通过键盘导航。 */
        lv_group_add_obj(kb_group, edit_ta);
        /* 让编辑框立即获得焦点，方便直接输入。 */
        lv_group_focus_obj(edit_ta);
        /* 将键盘组设为编辑模式，使按键直接输入文本而不是导航。 */
        lv_group_set_editing(kb_group, true);
    }
}

/**
 * 单击网格单元格：
 *  - 删除模式开启时，弹删除确认框；
 *  - 否则第一次点击高亮行/列；再次点击同一可编辑单元格则打开编辑框。
 */
 /* 定义网格单元格点击回调，处理删除、二次点击编辑和首次高亮。 */
static void on_cell_clicked(lv_event_t* e)
{
    /* 从事件用户数据中取出打包的单元格编码，还原点击的是哪个单元格。 */
    int code = (int)(intptr_t)lv_event_get_user_data(e);
    /* 通过宏从编码中解析出行号。 */
    int row = CELL_ROW(code);
    /* 通过宏从编码中解析出列号。 */
    int col = CELL_COL(code);
    /* 取得实际触发点击事件的单元格对象，后续与选中状态比较。 */
    lv_obj_t* cell = lv_event_get_target(e);

    /* 如果有编辑框且点的是其它位置，先保存并关闭编辑框 */
    /* 若已有编辑框且用户点击的不是编辑框本身，说明要切换到其他位置。 */
    if (edit_ta && cell != edit_ta) {
        /* 先保存并关闭当前编辑框，把当前修改落库并刷新界面。 */
        ui_save_and_close_edit();
        /* 保存后会 refresh，旧 cell 可能已被重建，但 cell 是同一个 label，不影响 */
    }

    /* 删除模式：只对数据行生效 */
    /* 删除模式开启且点击的是数据行（row>0，非表头）时，进入删除流程。 */
    if (delete_enabled && row > 0) {
        /* 获取被点击显示行对应的真实学生索引。 */
        int idx = data_get_display_index(row - 1);
        /* 若索引有效，弹出该学生的删除确认框。 */
        if (idx >= 0) show_delete_popup(idx);
        /* 处理完删除模式后直接返回，不再执行选中或编辑逻辑。 */
        return;
    }

    /* 再次点击同一个单元格：若可编辑则打开编辑框 */
    /* 判断是否第二次点击同一个单元格，并且该格是数据区单元格（row>0 且 col>0），条件延续到下一行。 */
    if (last_sel_row == row && last_sel_col == col && last_sel_cell == cell &&
        row > 0 && col > 0) {
        ui_open_cell_editor(row, col);

        /* 阻止本次点击继续冒泡到屏幕的 on_screen_clicked，
           否则屏幕会立刻把刚打开的编辑框关闭。 */
        lv_event_stop_bubbling(e);

        return;
    }

    /* 第一次点击：整行整列高亮 */
    /* 记录本次点击的行号，作为“第一次点击”的标记，供下一次点击判断是否同一格。 */
    last_sel_row = row;
    /* 记录本次点击的列号，与行号共同标识被单击的网格位置。 */
    last_sel_col = col;
    /* 记录本次点击的单元格对象指针，防止刷新重建后误把新单元格当作旧单元格。 */
    last_sel_cell = cell;

    /* 遍历表头加全部数据行（第0行到第200行），准备按行列高亮刷新整行整列背景。 */
    for (int r = 0; r <= UI_MAX_DATA_ROWS; r++) {
        /* 遍历全部9列，使每个单元格都能被判断是否属于被点击行或列。 */
        for (int c = 0; c < UI_COL_NUM; c++) {
            /* 若单元格处于被点击行或被点击列，则需要改变背景色。 */
            if (r == row || c == col) {
                /* 把行/列命中的单元格背景设为绿色，形成整行整列高亮效果。 */
                ui_set_cell_bg(grid_cells[r][c], 0x00C853);   /* 绿色 */
            }
            /* 未被点击行/列命中的单元格走另一分支。 */
            else {
                /* 把这些未命中单元格背景恢复为白色，清除上一次高亮留下的颜色。 */
                ui_set_cell_bg(grid_cells[r][c], 0xFFFFFF);   /* 白色 */
            }
        }
    }
    /* 把真正被点击的单元格背景设为黄色，与行列绿色区分，直观标出用户选中的格子。 */
    ui_set_cell_bg(cell, 0xFFFF00);                            /* 被点击格黄色 */
}

/* ======================== 屏幕全局点击 ======================== */

/* 屏幕全局点击事件的回调函数签名，用于统一处理点击编辑框之外的区域。 */
static void on_screen_clicked(lv_event_t* e)
{
    /* 获取本次事件实际点击到的 LVGL 对象 target，作为判断点击位置的依据。 */
    lv_obj_t* target = lv_event_get_target(e);

    /* 点编辑框本身/子控件不关闭；点编辑框以外区域保存并关闭 */
    /* 若当前存在编辑文本框，且点击目标不属于编辑框或其子控件，则说明用户点了外部区域。 */
    if (edit_ta && !ui_target_belongs_to_edit(target)) {
        /* 调用保存并关闭编辑框函数，把正在编辑的内容提交并移除编辑文本框。 */
        ui_save_and_close_edit();
    }
}

/* ======================== UI 创建 ======================== */

/* 创建顶部工具栏的函数，负责搭建搜索区与右侧一排操作按钮。 */
static void ui_create_top_bar(lv_obj_t* scr)
{
    /* 顶部工具栏容器 */
    /* 在屏幕对象上创建顶部工具栏容器并存入 top_obj，作为搜索框和按钮的父对象。 */
    top_obj = lv_obj_create(scr);
    /* 把工具栏定位到屏幕左上角 (0,0)，使其横贯顶部。 */
    lv_obj_set_pos(top_obj, 0, 0);
    /* 把工具栏尺寸设为 1080 宽、80 高，匹配固定屏幕宽度并留出操作区高度。 */
    lv_obj_set_size(top_obj, 1080, 80);
    /* 清除 LVGL 容器默认背景/边框等样式，使顶部条呈现简洁白底效果。 */
    ui_obj_clear_default(top_obj);
    /* 取消工具栏自身可滚动标志，避免内容溢出时出现滚动条。 */
    lv_obj_clear_flag(top_obj, LV_OBJ_FLAG_SCROLLABLE);
    /* 让工具栏可接收点击事件并允许事件向上冒泡，便于后续全局点击逻辑。 */
    lv_obj_add_flag(top_obj, LV_OBJ_FLAG_CLICKABLE | LV_OBJ_FLAG_EVENT_BUBBLE);

    /* 搜索输入框（尺寸 300x60） */
    /* 在顶部栏创建搜索输入框并存入全局 search_ta。 */
    search_ta = lv_textarea_create(top_obj);
    /* 把搜索框放在顶部栏左侧起始位置，纵向与顶部按钮对齐。 */
    lv_obj_set_pos(search_ta, 0, UI_TOP_BTN_Y);
    /* 按 UI_SEARCH_W、UI_TOP_BTN_H 设置搜索框尺寸。 */
    lv_obj_set_size(search_ta, UI_SEARCH_W, UI_TOP_BTN_H);
    /* 设置搜索框为单行输入，回车后触发 ready 事件。 */
    lv_textarea_set_one_line(search_ta, true);
    /* 限制搜索关键字最长 255 字符，防止超长输入。 */
    lv_textarea_set_max_length(search_ta, 255);
    /* 设置输入框未输入时显示的提示文字，提示用户输入关键字后回车。 */
    lv_textarea_set_placeholder_text(search_ta, "搜索：输入关键字后回车");
    /* 允许点击文本位置直接放置光标，方便修改关键字。 */
    lv_textarea_set_cursor_click_pos(search_ta, true);
    /* 给输入正文使用程序统一字体。 */
    lv_obj_set_style_text_font(search_ta, ui_font(), 0);
    /* 给占位符文本也设置统一字体，保证提示文字样式一致。 */
    lv_obj_set_style_text_font(search_ta, ui_font(), LV_PART_TEXTAREA_PLACEHOLDER);
    /* 增加 48 像素左内边距，为左侧搜索图标预留空间。 */
    /* 1. 清除 textarea 默认背景、边框、圆角、内边距 */
    lv_obj_set_style_bg_opa(search_ta, LV_OPA_TRANSP, 0);
    lv_obj_set_style_border_width(search_ta, 0, 0);
    lv_obj_set_style_radius(search_ta, 0, 0);
    lv_obj_set_style_pad_all(search_ta, 0, 0);

    /* 2. 把 search.png 当作搜索框背景图，正好铺满 300x60 */
    {
        snprintf(search_bg_path, sizeof(search_bg_path),
            "%s%s", UI_ICON_FOLDER, UI_ICON_SEARCH);

        lv_obj_set_style_bg_image_src(search_ta, search_bg_path, 0);
        lv_obj_set_style_bg_image_opa(search_ta, LV_OPA_COVER, 0);
        lv_obj_set_style_bg_image_tiled(search_ta, false, 0);
    }

    /* 3. 设置文字在胶囊内部的位置 */
    lv_obj_set_style_pad_left(search_ta, 28, 0);    /* 距左侧边框 */
    lv_obj_set_style_pad_right(search_ta, 90, 0);   /* 避开右侧放大镜 */
    lv_obj_set_style_pad_top(search_ta, 18, 0);     /* 垂直居中 */
    lv_obj_set_style_pad_bottom(search_ta, 18, 0);

    /* 4. 保持原来这两行事件注册不动 */
    lv_obj_add_flag(search_ta, LV_OBJ_FLAG_EVENT_BUBBLE);
    lv_obj_add_event_cb(search_ta, on_search_ready, LV_EVENT_READY, NULL);

    /* 左侧：筛选 */
    /* 创建筛选按钮并赋给全局 btn_filter：父对象为顶部栏、无文字、使用筛选图标。 */
    btn_filter = ui_make_small_button(top_obj, NULL, UI_ICON_FILTER,
        /* 筛选按钮的横向起点取搜索框宽度加按钮间距，使它紧挨搜索框右侧。 */
        UI_SEARCH_W + UI_BTN_GAP,
        /* 把筛选按钮点击事件绑定到 on_filter_clicked，用户数据传 NULL。 */
        on_filter_clicked, NULL);

    /* 右侧：帮助、导入、导出、删除，每个间隔 10px，整体贴右边 */
    /* 计算右侧按钮组最左侧起始横坐标：总宽1080减去4个按钮宽和3个间距，使整组贴右。 */
    int right_x = 1080 - 4 * UI_SMALL_BTN_W - 3 * UI_BTN_GAP;
    /* 创建帮助按钮并赋给 btn_help，图标为帮助图标。 */
    btn_help = ui_make_small_button(top_obj, NULL, UI_ICON_HELP,
        /* 把帮助按钮放在右侧按钮组当前横坐标，并绑定 on_help_clicked 回调。 */
        right_x, on_help_clicked, NULL);
    /* 向右推进横坐标，为下一个按钮留出一个按钮宽和一个间距。 */
    right_x += UI_SMALL_BTN_W + UI_BTN_GAP;
    /* 创建导入按钮并赋给 btn_import，图标为导入图标。 */
    btn_import = ui_make_small_button(top_obj, NULL, UI_ICON_IMPORT,
        /* 把导入按钮放在更新后的横坐标，并绑定 on_import_clicked 回调。 */
        right_x, on_import_clicked, NULL);
    /* 再次向右推进横坐标，用于放置导出按钮。 */
    right_x += UI_SMALL_BTN_W + UI_BTN_GAP;
    /* 创建导出按钮并赋给 btn_export，图标为导出图标。 */
    btn_export = ui_make_small_button(top_obj, NULL, UI_ICON_EXPORT,
        /* 把导出按钮放在当前横坐标，并绑定 on_export_clicked 回调。 */
        right_x, on_export_clicked, NULL);
    /* 继续向右推进横坐标，用于放置删除按钮。 */
    right_x += UI_SMALL_BTN_W + UI_BTN_GAP;
    /* 创建删除按钮并赋给 btn_delete，图标为删除图标。 */
    btn_delete = ui_make_small_button(top_obj, NULL, UI_ICON_DELETE,
        /* 把删除按钮放在最右侧位置，并绑定 on_delete_button_clicked 回调。 */
        right_x, on_delete_button_clicked, NULL);
}

/* 创建网格区域的函数，负责承载所有单元格和排序按钮。 */
static void ui_create_grid_area(lv_obj_t* scr)
{
    /* 在屏幕对象上创建网格容器并存入全局 obj_list。 */
    obj_list = lv_obj_create(scr);
    /* 把网格容器放到由 UI_LIST_X、UI_LIST_Y 指定的位置。 */
    lv_obj_set_pos(obj_list, UI_LIST_X, UI_LIST_Y);
    /* 按 UI_LIST_W、UI_LIST_H 设置网格区域尺寸。 */
    lv_obj_set_size(obj_list, UI_LIST_W, UI_LIST_H);
    /* 清除容器默认外观，让网格区域只显示自绘单元格。 */
    ui_obj_clear_default(obj_list);
    /* 允许网格区域在内容超出时向四个方向滚动。 */
    lv_obj_set_scroll_dir(obj_list, LV_DIR_ALL);
    /* 让网格容器可点击且事件可冒泡，保证全局点击能收到。 */
    lv_obj_add_flag(obj_list, LV_OBJ_FLAG_CLICKABLE | LV_OBJ_FLAG_EVENT_BUBBLE);

    /* 用默认列宽初始化全部9列的 col_widths，作为后续绘制与几何计算的基准。 */
    for (int c = 0; c < UI_COL_NUM; c++) col_widths[c] = UI_DEFAULT_COL_W;
    /* 创建所有网格单元格对象并保存到 grid_cells 数组中。 */
    ui_create_cells(obj_list);
    /* 创建各列的排序按钮。 */
    ui_create_sort_buttons(obj_list);
    /* 按当前列宽重新计算并应用所有单元格的位置尺寸。 */
    ui_apply_geometry();
}

/**
 * 对外刷新接口：删除/导入/筛选/编辑后调用。
 */
 /* 对外刷新接口：删除、导入、筛选、编辑完成后由外部调用。 */
void ui_refresh_grid(void)
{
    /* 调用内部刷新函数重新读取数据并重建/更新网格内容。 */
    ui_refresh_grid_internal();
}

/**
 * 创建完整 UI。
 */
 /* 创建完整 UI 的入口函数，接收屏幕对象后搭建所有界面元素。 */
void ui_create(lv_obj_t* scr)
{
    /* 屏幕底色设为浅灰，便于看清顶部白色条与下方白色网格 */
    /* 把屏幕底色设为浅灰 (0xE0E0E0)，衬托顶部白色条和下方白色网格。 */
    lv_obj_set_style_bg_color(scr, lv_color_hex(0xE0E0E0), 0);
    /* 设置背景完全不透明，避免透出桌面或下层内容。 */
    lv_obj_set_style_bg_opa(scr, LV_OPA_COVER, 0);
    /* 设置屏幕默认字体为程序统一字体，使后续控件默认文本样式一致。 */
    lv_obj_set_style_text_font(scr, ui_font(), 0);

    /* 全局点击：点击编辑框以外区域时保存编辑 */
    /* 在屏幕上注册点击事件回调 on_screen_clicked，用于点击编辑框外部时保存关闭。 */
    lv_obj_add_event_cb(scr, on_screen_clicked, LV_EVENT_CLICKED, NULL);

    /* 键盘分组 */
    /* 优先使用 SDL HAL 已经创建好的默认 group；如果没有则新建一个。 */
    kb_group = lv_group_get_default();
    if (kb_group == NULL) kb_group = lv_group_create();

    /* 调用函数创建顶部工具栏。 */
    ui_create_top_bar(scr);
    /* 调用函数创建下方网格区域。 */
    ui_create_grid_area(scr);

    /* 文本框加入键盘分组 */
    /* 把搜索框加入键盘分组，使其可用键盘焦点切换。 */
    lv_group_add_obj(kb_group, search_ta);

    /* 初始显示网格（无数据时数据层应返回 200 空行） */
    /* 初次调用内部刷新，无数据时由数据层返回空行以显示空表格。 */
    ui_refresh_grid_internal();
}

/* 对外绑定实体键盘输入设备到 UI 键盘分组，供方向键操作使用。 */
void ui_bind_keyboard(lv_indev_t* kb)
{
    /* 若输入设备与键盘分组均存在，则把分组设给该输入设备。 */
    if (kb && kb_group) lv_indev_set_group(kb, kb_group);
}
