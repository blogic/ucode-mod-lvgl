/*
 * SPDX-License-Identifier: GPL-2.0-only
 *
 * Copyright (C) 2026 John Crispin <john@phrozen.org>
 */

#ifndef UCODE_LV_H
#define UCODE_LV_H

#include <ucode/lib.h>
#include <ucode/vm.h>

#include <lvgl.h>

#define UCLV_WIDGET_TYPE	"lv.widget"
#define UCLV_SERIES_MAX		4
#define UCLV_EVENT_MAX		8

enum {
	WIDGET_UV_ON_EVENT,
	__WIDGET_UV_MAX
};

/**
 * uclv_grid_t - the track sizes of a grid container
 * @col: column sizes, closed with LV_GRID_TEMPLATE_LAST
 * @row: row sizes, likewise
 *
 * Its own allocation rather than a member of uclv_widget_t, because LVGL stores
 * the two pointers and re-reads them on every layout pass. They have to outlive
 * the resource, which is freed as soon as ucode drops its last reference to a
 * widget that is still on screen.
 */
typedef struct {
	int32_t *col;
	int32_t *row;
} uclv_grid_t;

/**
 * uclv_widget_t - the ucode side of an LVGL object
 * @obj: weak handle, cleared on LV_EVENT_DELETE because LVGL owns the tree
 * @vm: the VM an event handler runs in
 * @res: the ucode resource, borrowed unless @anchored
 * @anchored: a reference is held, so LVGL can reach the event handler after
 *            ucode has dropped its last one
 * @series: chart series handles
 * @series_count: how many are in use
 * @codes: the event codes that already have a C dispatcher
 * @code_count: how many are in use
 * @grid: grid track sizes, or NULL, owned by the object's delete event
 * @tile_col: column, on a tile
 * @tile_row: row, on a tile
 */
typedef struct {
	lv_obj_t *obj;
	uc_vm_t *vm;
	uc_value_t *res;
	bool anchored;
	lv_chart_series_t *series[UCLV_SERIES_MAX];
	size_t series_count;
	lv_event_code_t codes[UCLV_EVENT_MAX];
	size_t code_count;
	uclv_grid_t *grid;
	int tile_col;
	int tile_row;
} uclv_widget_t;

uc_value_t *uclv_widget_wrap(uc_vm_t *vm, lv_obj_t *obj);
lv_obj_t *uclv_widget_obj(uc_vm_t *vm);
void uclv_widget_register(uc_vm_t *vm);
const lv_font_t *uclv_font(size_t index);
size_t uclv_font_add(const lv_font_t *font);
const lv_image_dsc_t *uclv_image(size_t index);
size_t uclv_image_add(const lv_image_dsc_t *dsc);

#endif /* UCODE_LV_H */
