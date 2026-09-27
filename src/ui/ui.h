/*
 * @file ui.h
 * @brief 学生成绩信息管理系统 —— LVGL v9.5 界面层头文件。
 *
 * 本文件只声明 UI 层对外接口和界面配置枚举；
 * 学生结构体、CSV 读写、筛选/搜索/排序等由 core.h 提供。
 */

#ifndef UI_H
#define UI_H

#include <stddef.h>

/* LVGL v9.5 主头文件。 */
#include "lvgl/lvgl.h"

/* 数据核心层：StudentCSV、stu_field_t、data_* / ui_help_* 函数原型。 */
#include "src/core/core.h"

#ifdef __cplusplus
extern "C" {
#endif

/* ==================================================================
 * 一、界面配置
 * ================================================================== */

/* 主界面尺寸：1080×720，main.c 创建窗口时与 UI 层共用。 */
enum {
    UI_SCR_W = 1080,   /* 窗口/屏幕宽度 */
    UI_SCR_H = 720     /* 窗口/屏幕高度 */
};

/* ==================================================================
 * 二、字段编号与筛选枚举
 * ================================================================== */

/*
 * 表格数据列对应的学生字段编号。
 * 数值必须与 core.h 中的 stu_field_t 完全一致，UI 层会把它
 * 直接传给 data_sort_students()、data_apply_filter() 等数据层函数。
 */
typedef enum {
    UI_FIELD_GRADE = 0,    /* 年级   */
    UI_FIELD_CLASS = 1,    /* 班级   */
    UI_FIELD_ID = 2,       /* 学号   */
    UI_FIELD_NAME = 3,     /* 姓名   */
    UI_FIELD_GENDER = 4,   /* 性别   */
    UI_FIELD_REGULAR = 5,  /* 平时   */
    UI_FIELD_MIDTERM = 6,  /* 期中   */
    UI_FIELD_FINAL = 7,    /* 期末   */
    UI_FIELD_SCORE = 8,    /* 总分   */
    UI_FIELD_GPA = 9,      /* 绩点   */
    UI_FIELD_RANK = 10,    /* 排名   */
    UI_FIELD_COUNT = 11    /* 字段总数 */
} ui_field_t;

/* 筛选第一级列表中的类别。 */
typedef enum {
    UI_FILTER_GRADE = 0,   /* 按年级筛选 */
    UI_FILTER_CLASS = 1,   /* 按班级筛选 */
    UI_FILTER_GENDER = 2,  /* 按性别筛选 */
    UI_FILTER_SCORE = 3,   /* 按分数段筛选 */
    UI_FILTER_ALL = 4      /* 全部（清除筛选） */
} ui_filter_kind_t;

/* ==================================================================
 * 三、UI 对外接口
 * ================================================================== */

/*
 * 创建完整 UI：顶部工具栏、固定表头、12 列数据网格和所有弹窗。
 * @param scr 父屏幕对象，通常传 lv_screen_active()。
 */
void ui_create(lv_obj_t* scr);

/* 设置界面统一字体（由 main.c 加载中文字体后调用）。 */
void ui_set_font(const lv_font_t* font);

/*
 * 模拟点击顶部“帮助”按钮，用于首次运行时自动显示帮助。
 * @return 1 帮助浮层创建成功；0 创建失败（例如帮助图片缺失）。
 */
int ui_show_help(void);

#ifdef __cplusplus
}
#endif

#endif /* UI_H */
