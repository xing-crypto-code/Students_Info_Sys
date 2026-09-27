/**
 * @file ui.h
 * @brief 学生成绩信息管理系统 —— LVGL v9.5 UI 框架层头文件。
 *
 * 本文件只描述 UI 层对外的接口和界面配置常量。
 * 数据层的结构体、CSV 函数、筛选/搜索/排序接口全部由 core.h 提供。
 */

#ifndef UI_H
#define UI_H

#include <stddef.h>

 /* LVGL v9.5 主头文件 */
#include "lvgl/lvgl.h"

/* 数据核心层：StudentCSV、MAX_FIELD_LEN、data_* / ui_help_* 函数 */
#include "src/core/core.h"

#ifdef __cplusplus
extern "C" {
#endif

    /* ======================== 尺寸常量 ======================== */

#define UI_SCR_W              1080   /* 窗口/屏幕宽 */
#define UI_SCR_H              720    /* 窗口/屏幕高 */
#define UI_TOP_H              80     /* 顶部工具栏高 */
#define UI_LIST_W             1080   /* 下方表格区域宽 */
#define UI_LIST_H             620    /* 下方表格区域高 */
#define UI_LIST_X             0      /* 下方表格区域 X 坐标 */
#define UI_LIST_Y             100    /* 下方表格区域 Y 坐标 */

#define UI_MAX_DATA_ROWS      200    /* 表格默认最多显示 200 行数据 */
#define UI_COL_NUM            9      /* 第 1 列序号 + 8 列数据 */
#define UI_ROW_H              50     /* 固定行高 */
#define UI_DEFAULT_COL_W      120    /* 无数据时列宽默认值 */

#define UI_TOP_BTN_Y          10     /* 顶部按钮 Y 坐标 */
#define UI_TOP_BTN_H          60     /* 顶部按钮高度 */
#define UI_BTN_GAP            10     /* 相邻按钮间距 */
#define UI_SEARCH_W           300    /* 搜索框宽度 */
#define UI_SMALL_BTN_W        60     /* 小按钮宽度 */

/* ======================== 图标配置 ======================== */

/**
 * 图标文件夹地址。
 *
 * 默认使用 LVGL 文件系统路径 "A:icons/"；
 * 实际部署时请把该宏改成你的图标文件夹地址。
 */
#define UI_ICON_FOLDER "A:btn_png/"

 /* 顶部搜索框/按钮图标文件名 */
#define UI_ICON_SEARCH  "search.png"
#define UI_ICON_FILTER  "filter.png"
#define UI_ICON_HELP    "help.png"
#define UI_ICON_IMPORT  "import.png"
#define UI_ICON_EXPORT  "export.png"
#define UI_ICON_DELETE  "delete.png"

/* 表头排序按钮图标文件名（四个排序按钮共用） */
#define UI_ICON_SORT    "sort.png"

/* ======================== 字段/筛选枚举 ======================== */

/**
 * 表格第 2~9 列对应的学生结构体字段。
 * 数值必须与 core.h 中的 stu_field_t 完全一致。
 */
    typedef enum {
        UI_FIELD_GRADE = 0,   /* 年级   */
        UI_FIELD_CLASS = 1,   /* 班级   */
        UI_FIELD_ID = 2,   /* 学号   */
        UI_FIELD_NAME = 3,   /* 姓名   */
        UI_FIELD_GENDER = 4,   /* 性别   */
        UI_FIELD_SCORE = 5,   /* 分数   */
        UI_FIELD_GPA = 6,   /* 绩点   */
        UI_FIELD_RANK = 7,   /* 排名   */
        UI_FIELD_COUNT = 8
    } ui_field_t;

    /**
     * 筛选第一级列表中的类别。
     */
    typedef enum {
        UI_FILTER_GRADE = 0,
        UI_FILTER_CLASS = 1,
        UI_FILTER_GENDER = 2,
        UI_FILTER_SCORE = 3,
        UI_FILTER_ALL = 4
    } ui_filter_kind_t;

    /* ======================== UI 框架对外接口 ======================== */

    /**
     * 创建完整 UI。
     * @param scr 通常传 lv_screen_active()。
     */
    void ui_create(lv_obj_t* scr);

    /** 从 core 读取当前显示数据并刷新网格。 */
    void ui_refresh_grid(void);

    /** 将内部键盘分组绑定到移植层的 keypad 输入设备。 */
    void ui_bind_keyboard(lv_indev_t* kb);

    void ui_set_font(const lv_font_t* font);

#ifdef __cplusplus
}
#endif

#endif /* UI_H */
