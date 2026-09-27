# ui.c / ui.h 函数分类说明

> 文件位置：
>
> - `src/ui/ui.h`
> - `src/ui/ui.c`
>
> 本文按功能分类说明 UI 层的函数、原型、可见范围和大致定义位置。

---

## 一、UI 对外接口（ui.h 对外暴露，main.c 调用）

| 函数 | 原型 | 功能说明 | 定义位置 |
| --- | --- | --- | --- |
| `ui_create` | `void ui_create(lv_obj_t* scr);` | 在指定屏幕对象上创建完整 UI | ui.h:101 / ui.c:1970 |
| `ui_refresh_grid` | `void ui_refresh_grid(void);` | 从 core 读取数据并刷新网格 | ui.h:104 / ui.c:1960 |
| `ui_bind_keyboard` | `void ui_bind_keyboard(lv_indev_t* kb);` | 将键盘输入设备绑定到 UI 键盘分组 | ui.h:107 / ui.c:2004 |
| `ui_set_font` | `void ui_set_font(const lv_font_t* font);` | 设置整个 UI 使用的字体 | ui.h:109 / ui.c:222 |

---

## 二、字体与通用样式工具

| 函数 | 原型 | 功能说明 | 定义位置 |
| --- | --- | --- | --- |
| `ui_font` | `static const lv_font_t* ui_font(void);` | 返回当前 UI 使用的字体 | ui.c:217 |
| `ui_obj_clear_default` | `static void ui_obj_clear_default(lv_obj_t* obj);` | 清除对象默认背景、边框、圆角、内边距 | ui.c:231 |
| `ui_get_ta_text` | `static void ui_get_ta_text(lv_obj_t* ta, char* out, size_t out_size);` | 从文本框读取内容并去掉首尾空白和换行 | ui.c:253 |
| `ui_target_belongs_to_edit` | `static bool ui_target_belongs_to_edit(lv_obj_t* target);` | 判断点击目标是否属于当前编辑框 | ui.c:293 |
| `ui_make_small_button` | `static lv_obj_t* ui_make_small_button(lv_obj_t* parent, const char* text, const char* icon_name, int x, lv_event_cb_t cb, void* user_data);` | 创建顶部 60x60 小按钮，可显示图标或文字 | ui.c:312 |
| `ui_attach_icon` | `static void ui_attach_icon(lv_obj_t* parent, const char* icon_name);` | 在对象中心创建并显示图标 | ui.c:362 |

---

## 三、网格单元格

### 1. 单元格创建与样式

| 函数 | 原型 | 功能说明 | 定义位置 |
| --- | --- | --- | --- |
| `ui_set_cell_bg` | `static void ui_set_cell_bg(lv_obj_t* cell, uint32_t color);` | 设置单元格背景色 | ui.c:382 |
| `ui_set_cell_style` | `static void ui_set_cell_style(lv_obj_t* cell);` | 设置单元格边框、背景、字体、居中、点击等样式 | ui.c:391 |
| `ui_create_cells` | `static void ui_create_cells(lv_obj_t* parent);` | 创建 201 行 × 9 列的单元格 | ui.c:430 |
| `ui_apply_geometry` | `static void ui_apply_geometry(void);` | 按当前列宽排列所有单元格 | ui.c:455 |

### 2. 表头排序按钮

| 函数 | 原型 | 功能说明 | 定义位置 |
| --- | --- | --- | --- |
| `ui_position_sort_buttons` | `static void ui_position_sort_buttons(void);` | 摆放四个表头排序按钮 | ui.c:487 |
| `on_sort_clicked` | `static void on_sort_clicked(lv_event_t* e);` | 点击排序按钮，切换正序/倒序并刷新 | ui.c:515 |
| `ui_create_sort_buttons` | `static void ui_create_sort_buttons(lv_obj_t* parent);` | 创建年级、班级、性别、分数表头排序按钮 | ui.c:561 |

### 3. 单元格内容与列宽

| 函数 | 原型 | 功能说明 | 定义位置 |
| --- | --- | --- | --- |
| `ui_fill_cell_text` | `static void ui_fill_cell_text(int grid_row, int grid_col, char* buf, size_t buf_size);` | 读取指定网格单元格应显示的文本 | ui.c:603 |
| `ui_auto_columns` | `static void ui_auto_columns(int display_count);` | 根据内容自动计算列宽 | ui.c:719 |
| `ui_reset_all_cells_white` | `static void ui_reset_all_cells_white(void);` | 把所有单元格背景恢复为白色 | ui.c:757 |

### 4. 网格刷新

| 函数 | 原型 | 功能说明 | 定义位置 |
| --- | --- | --- | --- |
| `ui_refresh_grid_internal` | `static void ui_refresh_grid_internal(void);` | 读取数据、重算列宽、填充文本、更新显示 | ui.c:777 |
| `ui_refresh_grid` | `void ui_refresh_grid(void);` | 对外刷新接口 | ui.c:1960 |

---

## 四、搜索功能

| 函数 | 原型 | 功能说明 | 定义位置 |
| --- | --- | --- | --- |
| `on_search_ready` | `static void on_search_ready(lv_event_t* e);` | 搜索框回车，调用 data_find_matches 并高亮匹配行 | ui.c:839 |
| `ui_clear_search_hilight` | `static void ui_clear_search_hilight(lv_timer_t* timer);` | 搜索高亮定时结束，恢复白色网格 | ui.c:900 |

---

## 五、筛选功能

| 函数 | 原型 | 功能说明 | 定义位置 |
| --- | --- | --- | --- |
| `ui_close_filter_lists` | `static void ui_close_filter_lists(void);` | 关闭一级、二级筛选列表 | ui.c:913 |
| `on_filter_option_clicked` | `static void on_filter_option_clicked(lv_event_t* e);` | 点击二级筛选选项，调用 data_apply_filter | ui.c:932 |
| `on_filter_kind_clicked` | `static void on_filter_kind_clicked(lv_event_t* e);` | 点击年级/班级/性别/分数/全部 | ui.c:961 |
| `on_filter_clicked` | `static void on_filter_clicked(lv_event_t* e);` | 打开或关闭一级筛选列表 | ui.c:1054 |

---

## 六、帮助功能

| 函数 | 原型 | 功能说明 | 定义位置 |
| --- | --- | --- | --- |
| `ui_set_help_image` | `static void ui_set_help_image(int index);` | 设置 help_imagebtn 显示第 index 张帮助图 | ui.c:1114 |
| `on_help_image_clicked` | `static void on_help_image_clicked(lv_event_t* e);` | 点击帮助图片，切换下一张或删除按钮 | ui.c:1130 |
| `on_help_clicked` | `static void on_help_clicked(lv_event_t* e);` | 点击帮助按钮，创建 1080x80 图片按钮 | ui.c:1158 |

---

## 七、导入 / 导出

| 函数 | 原型 | 功能说明 | 定义位置 |
| --- | --- | --- | --- |
| `on_path_ready` | `static void on_path_ready(lv_event_t* e);` | 路径输入框回车，调用 data_import_csv 或 data_export_csv | ui.c:1194 |
| `ui_open_path_ta` | `static void ui_open_path_ta(int action);` | 打开导入/导出路径输入框 | ui.c:1262 |
| `on_import_clicked` | `static void on_import_clicked(lv_event_t* e);` | 导入按钮点击事件 | ui.c:1321 |
| `on_export_clicked` | `static void on_export_clicked(lv_event_t* e);` | 导出按钮点击事件 | ui.c:1331 |

---

## 八、删除功能

| 函数 | 原型 | 功能说明 | 定义位置 |
| --- | --- | --- | --- |
| `on_delete_button_clicked` | `static void on_delete_button_clicked(lv_event_t* e);` | 切换删除模式开/关，并改变按钮颜色 | ui.c:1343 |
| `on_delete_cancel` | `static void on_delete_cancel(lv_event_t* e);` | 删除确认框“取消” | ui.c:1365 |
| `on_delete_confirm` | `static void on_delete_confirm(lv_event_t* e);` | 删除确认框“确定” | ui.c:1379 |
| `show_delete_popup` | `static void show_delete_popup(int student_index);` | 显示 500x200 删除确认弹窗 | ui.c:1403 |

---

## 九、Toast 提示

| 函数 | 原型 | 功能说明 | 定义位置 |
| --- | --- | --- | --- |
| `on_toast_clicked` | `static void on_toast_clicked(lv_event_t* e);` | 点击 Toast 立即关闭 | ui.c:1529 |
| `on_toast_timer` | `static void on_toast_timer(lv_timer_t* timer);` | Toast 定时结束自动关闭 | ui.c:1550 |
| `ui_show_toast` | `static void ui_show_toast(const char* msg);` | 创建 Toast 提示并 2 秒后关闭 | ui.c:1566 |

---

## 十、单元格编辑

| 函数 | 原型 | 功能说明 | 定义位置 |
| --- | --- | --- | --- |
| `ui_close_edit` | `static void ui_close_edit(void);` | 删除编辑框并清理编辑状态 | ui.c:1623 |
| `ui_save_and_close_edit` | `static void ui_save_and_close_edit(void);` | 保存编辑内容并关闭编辑框 | ui.c:1639 |
| `on_edit_ready` | `static void on_edit_ready(lv_event_t* e);` | 编辑框回车事件 | ui.c:1667 |
| `ui_open_cell_editor` | `static void ui_open_cell_editor(int grid_row, int grid_col);` | 在指定单元格上打开编辑框 | ui.c:1676 |
| `on_cell_clicked` | `static void on_cell_clicked(lv_event_t* e);` | 单元格点击，处理选中、删除、二次点击编辑 | ui.c:1743 |

---

## 十一、全局点击

| 函数 | 原型 | 功能说明 | 定义位置 |
| --- | --- | --- | --- |
| `on_screen_clicked` | `static void on_screen_clicked(lv_event_t* e);` | 点击编辑框外部时保存并关闭编辑框 | ui.c:1817 |

---

## 十二、UI 构建

| 函数 | 原型 | 功能说明 | 定义位置 |
| --- | --- | --- | --- |
| `ui_create_top_bar` | `static void ui_create_top_bar(lv_obj_t* scr);` | 创建顶部工具栏、搜索框、筛选/帮助/导入/导出/删除按钮 | ui.c:1833 |
| `ui_create_grid_area` | `static void ui_create_grid_area(lv_obj_t* scr);` | 创建下方网格容器、单元格和排序按钮 | ui.c:1931 |
| `ui_create` | `void ui_create(lv_obj_t* scr);` | 创建完整 UI | ui.c:1970 |

---

## 十三、函数调用关系简图

```text
main.c
  │
  ├── ui_create()
  │     ├── ui_create_top_bar()
  │     │     ├── ui_make_small_button()
  │     │     ├── ui_attach_icon()
  │     │     └── 搜索框回调 on_search_ready()
  │     ├── ui_create_grid_area()
  │     │     ├── ui_create_cells()
  │     │     ├── ui_create_sort_buttons()
  │     │     └── ui_apply_geometry()
  │     └── ui_refresh_grid_internal()
  │
  ├── ui_bind_keyboard()
  └── ui_refresh_grid()

事件回调
  ├── on_filter_clicked()
  │     └── on_filter_kind_clicked()
  │           └── on_filter_option_clicked()
  ├── on_help_clicked()
  │     └── on_help_image_clicked()
  ├── on_import_clicked() / on_export_clicked()
  │     └── ui_open_path_ta()
  │           └── on_path_ready()
  ├── on_delete_button_clicked()
  │     └── show_delete_popup()
  │           ├── on_delete_cancel()
  │           └── on_delete_confirm()
  ├── on_cell_clicked()
  │     └── ui_open_cell_editor()
  │           └── on_edit_ready()
  └── on_screen_clicked()
        └── ui_save_and_close_edit()
```
