/*
 * SPDX-License-Identifier: GPL-2.0-only
 *
 * Copyright (C) 2026 John Crispin <john@phrozen.org>
 */

#include <stdlib.h>
#include <string.h>

#include "ucode-lv.h"

typedef enum {
	SP_INT,
	SP_COLOR,
	SP_OPA,
	SP_FONT,
	SP_BOOL,
} uclv_style_kind_t;

typedef struct {
	const char *name;
	uclv_style_kind_t kind;
	union {
		void (*i)(lv_obj_t *, int32_t, lv_style_selector_t);
		void (*c)(lv_obj_t *, lv_color_t, lv_style_selector_t);
		void (*o)(lv_obj_t *, lv_opa_t, lv_style_selector_t);
		void (*f)(lv_obj_t *, const lv_font_t *, lv_style_selector_t);
		void (*b)(lv_obj_t *, bool, lv_style_selector_t);
	} fn;
} uclv_style_prop_t;

static void
uclv_set_bg_grad_dir(lv_obj_t *obj, int32_t value, lv_style_selector_t sel)
{
	lv_obj_set_style_bg_grad_dir(obj, (lv_grad_dir_t)value, sel);
}

static void
uclv_set_text_align(lv_obj_t *obj, int32_t value, lv_style_selector_t sel)
{
	lv_obj_set_style_text_align(obj, (lv_text_align_t)value, sel);
}

static const uclv_style_prop_t style_props[] = {
	{ "arc_color",		SP_COLOR, { .c = lv_obj_set_style_arc_color } },
	{ "arc_opa",		SP_OPA,   { .o = lv_obj_set_style_arc_opa } },
	{ "arc_rounded",	SP_BOOL,  { .b = lv_obj_set_style_arc_rounded } },
	{ "arc_width",		SP_INT,   { .i = lv_obj_set_style_arc_width } },
	{ "bg_color",		SP_COLOR, { .c = lv_obj_set_style_bg_color } },
	{ "bg_grad_color",	SP_COLOR, { .c = lv_obj_set_style_bg_grad_color } },
	{ "bg_grad_dir",	SP_INT,   { .i = uclv_set_bg_grad_dir } },
	{ "bg_grad_opa",	SP_OPA,   { .o = lv_obj_set_style_bg_grad_opa } },
	{ "bg_main_opa",	SP_OPA,   { .o = lv_obj_set_style_bg_main_opa } },
	{ "bg_opa",		SP_OPA,   { .o = lv_obj_set_style_bg_opa } },
	{ "border_color",	SP_COLOR, { .c = lv_obj_set_style_border_color } },
	{ "border_width",	SP_INT,   { .i = lv_obj_set_style_border_width } },
	{ "clip_corner",	SP_BOOL,  { .b = lv_obj_set_style_clip_corner } },
	{ "height",		SP_INT,   { .i = lv_obj_set_style_height } },
	{ "image_recolor",	SP_COLOR, { .c = lv_obj_set_style_image_recolor } },
	{ "image_recolor_opa",	SP_OPA,   { .o = lv_obj_set_style_image_recolor_opa } },
	{ "line_color",		SP_COLOR, { .c = lv_obj_set_style_line_color } },
	{ "line_rounded",	SP_BOOL,  { .b = lv_obj_set_style_line_rounded } },
	{ "line_width",		SP_INT,   { .i = lv_obj_set_style_line_width } },
	{ "opa",		SP_OPA,   { .o = lv_obj_set_style_opa } },
	{ "pad_all",		SP_INT,   { .i = lv_obj_set_style_pad_all } },
	{ "pad_bottom",		SP_INT,   { .i = lv_obj_set_style_pad_bottom } },
	{ "pad_column",		SP_INT,   { .i = lv_obj_set_style_pad_column } },
	{ "pad_left",		SP_INT,   { .i = lv_obj_set_style_pad_left } },
	{ "pad_right",		SP_INT,   { .i = lv_obj_set_style_pad_right } },
	{ "pad_row",		SP_INT,   { .i = lv_obj_set_style_pad_row } },
	{ "pad_top",		SP_INT,   { .i = lv_obj_set_style_pad_top } },
	{ "radius",		SP_INT,   { .i = lv_obj_set_style_radius } },
	{ "shadow_width",	SP_INT,   { .i = lv_obj_set_style_shadow_width } },
	{ "text_align",		SP_INT,   { .i = uclv_set_text_align } },
	{ "text_color",		SP_COLOR, { .c = lv_obj_set_style_text_color } },
	{ "text_font",		SP_FONT,  { .f = lv_obj_set_style_text_font } },
	{ "text_letter_space",	SP_INT,   { .i = lv_obj_set_style_text_letter_space } },
	{ "text_line_space",	SP_INT,   { .i = lv_obj_set_style_text_line_space } },
	{ "width",		SP_INT,   { .i = lv_obj_set_style_width } },
};

static const lv_image_dsc_t **loaded;
static size_t loaded_count;

size_t
uclv_image_add(const lv_image_dsc_t *dsc)
{
	const lv_image_dsc_t **grown;

	grown = realloc(loaded, (loaded_count + 1) * sizeof(*loaded));

	if (!grown)
		return SIZE_MAX;

	loaded = grown;
	loaded[loaded_count++] = dsc;

	return loaded_count - 1;
}

const lv_image_dsc_t *
uclv_image(size_t index)
{
	return index < loaded_count ? loaded[index] : NULL;
}

static const lv_font_t **loaded_fonts;
static size_t loaded_font_count;

size_t
uclv_font_add(const lv_font_t *font)
{
	const lv_font_t **grown;

	grown = realloc(loaded_fonts,
			(loaded_font_count + 1) * sizeof(*loaded_fonts));

	if (!grown)
		return SIZE_MAX;

	loaded_fonts = grown;
	loaded_fonts[loaded_font_count++] = font;

	return loaded_font_count - 1;
}

const lv_font_t *
uclv_font(size_t index)
{
	return index < loaded_font_count ? loaded_fonts[index] : NULL;
}

lv_obj_t *
uclv_widget_obj(uc_vm_t *vm)
{
	uclv_widget_t *w = _uc_fn_thisval(vm, UCLV_WIDGET_TYPE);

	if (!w || !w->obj) {
		uc_vm_raise_exception(vm, EXCEPTION_RUNTIME,
				      "widget has already been deleted");
		return NULL;
	}

	return w->obj;
}

static uclv_widget_t *
uclv_widget_this(uc_vm_t *vm)
{
	uclv_widget_t *w = _uc_fn_thisval(vm, UCLV_WIDGET_TYPE);

	if (!w || !w->obj) {
		uc_vm_raise_exception(vm, EXCEPTION_RUNTIME,
				      "widget has already been deleted");
		return NULL;
	}

	return w;
}

static lv_obj_t *
uclv_widget_typed(uc_vm_t *vm, const lv_obj_class_t *class, const char *what)
{
	lv_obj_t *obj = uclv_widget_obj(vm);

	if (!obj)
		return NULL;

	if (!lv_obj_check_type(obj, class)) {
		uc_vm_raise_exception(vm, EXCEPTION_TYPE,
				      "this method needs %s", what);
		return NULL;
	}

	return obj;
}

static void
uclv_widget_event(lv_event_t *e)
{
	uclv_widget_t *w = lv_event_get_user_data(e);
	uc_value_t *handlers, *cb, *res;
	lv_event_code_t code;
	uc_vm_t *vm;
	size_t i;

	if (!w || !w->obj || !w->res)
		return;

	vm = w->vm;

	/* uc_vm_call() clears a pending exception before it runs. */
	if (vm->exception.type != EXCEPTION_NONE)
		return;

	handlers = ucv_resource_value_get(w->res, WIDGET_UV_ON_EVENT);

	if (ucv_type(handlers) != UC_ARRAY)
		return;

	code = lv_event_get_code(e);

	/* A handler may delete the widget or add another one, and w lives in the
	   resource, so hold both and re-read the length on every pass. */
	res = ucv_get(w->res);
	ucv_get(handlers);

	for (i = 0; i + 1 < ucv_array_length(handlers); i += 2) {
		if ((lv_event_code_t)ucv_to_integer(ucv_array_get(handlers, i)) != code)
			continue;

		cb = ucv_array_get(handlers, i + 1);

		if (!ucv_is_callable(cb))
			continue;

		uc_vm_stack_push(vm, ucv_get(cb));
		uc_vm_stack_push(vm, ucv_int64_new(code));

		if (uc_vm_call(vm, false, 1) != EXCEPTION_NONE)
			break;

		ucv_put(uc_vm_stack_pop(vm));

		if (!w->obj)
			break;
	}

	ucv_put(handlers);

	ucv_put(res);
}

static void
uclv_widget_deleted(lv_event_t *e)
{
	uclv_widget_t *w = lv_event_get_user_data(e);
	uc_value_t *res;

	if (!w || !w->obj)
		return;

	/* Order matters: the last put runs uclv_widget_free(), which must not
	   touch an object LVGL is destroying. */
	w->obj = NULL;

	if (!w->anchored)
		return;

	res = w->res;
	w->res = NULL;
	w->anchored = false;

	ucv_resource_persistent_set(res, false);
	ucv_put(res);
}

static void
uclv_widget_free(void *ptr)
{
	uclv_widget_t *w = ptr;

	if (!w->obj)
		return;

	/* The resource died first, so LVGL must not reach this block again. */
	lv_obj_remove_event_cb_with_user_data(w->obj, uclv_widget_event, w);
	lv_obj_remove_event_cb_with_user_data(w->obj, uclv_widget_deleted, w);
	lv_obj_set_user_data(w->obj, NULL);

	w->obj = NULL;
	w->res = NULL;
}

uc_value_t *
uclv_widget_wrap(uc_vm_t *vm, lv_obj_t *obj)
{
	uclv_widget_t *w;
	uc_value_t *res;

	if (!obj)
		return NULL;

	w = lv_obj_get_user_data(obj);

	if (w && w->res)
		return ucv_get(w->res);

	res = ucv_resource_create_ex(vm, UCLV_WIDGET_TYPE, (void **)&w,
				     __WIDGET_UV_MAX, sizeof(*w));

	if (!res)
		return NULL;

	w->obj = obj;
	w->vm = vm;
	w->res = res;

	lv_obj_set_user_data(obj, w);
	lv_obj_add_event_cb(obj, uclv_widget_deleted, LV_EVENT_DELETE, w);

	return res;
}

static bool
uclv_style_apply(uc_vm_t *vm, lv_obj_t *obj, const char *name, uc_value_t *val,
		 lv_style_selector_t sel)
{
	const uclv_style_prop_t *p = NULL;
	size_t i, font;

	for (i = 0; i < ARRAY_SIZE(style_props); i++) {
		if (!strcmp(style_props[i].name, name)) {
			p = &style_props[i];
			break;
		}
	}

	if (!p) {
		uc_vm_raise_exception(vm, EXCEPTION_TYPE,
				      "unknown style property: %s", name);
		return false;
	}

	switch (p->kind) {
	case SP_INT:
		p->fn.i(obj, (int32_t)ucv_to_integer(val), sel);
		break;

	case SP_COLOR:
		p->fn.c(obj, lv_color_hex((uint32_t)ucv_to_unsigned(val)), sel);
		break;

	case SP_OPA:
		p->fn.o(obj, (lv_opa_t)ucv_to_integer(val), sel);
		break;

	case SP_BOOL:
		p->fn.b(obj, ucv_is_truish(val), sel);
		break;

	case SP_FONT: {
		const lv_font_t *face;

		font = (size_t)ucv_to_integer(val);
		face = uclv_font(font);

		if (!face) {
			uc_vm_raise_exception(vm, EXCEPTION_RUNTIME,
					      "no such font: %zu", font);
			return false;
		}

		p->fn.f(obj, face, sel);
		break;
	}
	}

	return true;
}

/**
 * uc_lv_style() - w.style(props, [selector])
 * @props: style properties by name, such as bg_color, text_font, pad_all
 * @selector: a part ORed with a state, default PART_MAIN
 *
 * Return: true, or null when a property is not known.
 */
static uc_value_t *
uc_lv_style(uc_vm_t *vm, size_t nargs)
{
	lv_obj_t *obj = uclv_widget_obj(vm);
	uc_value_t *props = uc_fn_arg(0);
	uc_value_t *selarg = uc_fn_arg(1);
	lv_style_selector_t sel;

	if (!obj)
		return NULL;

	if (ucv_type(props) != UC_OBJECT) {
		uc_vm_raise_exception(vm, EXCEPTION_TYPE,
				      "expecting an object of style properties");
		return NULL;
	}

	sel = selarg ? (lv_style_selector_t)ucv_to_unsigned(selarg) : LV_PART_MAIN;

	ucv_object_foreach(props, key, val)
		if (!uclv_style_apply(vm, obj, key, val, sel))
			return NULL;

	return ucv_boolean_new(true);
}

/**
 * uc_lv_set() - w.set(props)
 * @props: any of x, y, w, h, align, align_to, align_mode, align_x, align_y
 *
 * Each axis is written on its own, so a key that is absent leaves that axis
 * alone.
 *
 * Return: true.
 */
static uc_value_t *
uc_lv_set(uc_vm_t *vm, size_t nargs)
{
	lv_obj_t *obj = uclv_widget_obj(vm);
	uc_value_t *props = uc_fn_arg(0);
	uc_value_t *val;

	if (!obj)
		return NULL;

	if (ucv_type(props) != UC_OBJECT) {
		uc_vm_raise_exception(vm, EXCEPTION_TYPE,
				      "expecting an object of geometry properties");
		return NULL;
	}

	val = ucv_object_get(props, "w", NULL);
	if (val)
		lv_obj_set_width(obj, (int32_t)ucv_to_integer(val));

	val = ucv_object_get(props, "h", NULL);
	if (val)
		lv_obj_set_height(obj, (int32_t)ucv_to_integer(val));

	val = ucv_object_get(props, "x", NULL);
	if (val)
		lv_obj_set_x(obj, (int32_t)ucv_to_integer(val));

	val = ucv_object_get(props, "y", NULL);
	if (val)
		lv_obj_set_y(obj, (int32_t)ucv_to_integer(val));

	val = ucv_object_get(props, "align", NULL);
	if (val) {
		uc_value_t *ox = ucv_object_get(props, "align_x", NULL);
		uc_value_t *oy = ucv_object_get(props, "align_y", NULL);

		lv_obj_align(obj, (lv_align_t)ucv_to_integer(val),
			     ox ? ucv_to_integer(ox) : 0,
			     oy ? ucv_to_integer(oy) : 0);
	}

	val = ucv_object_get(props, "align_to", NULL);
	if (val) {
		uclv_widget_t *ref = ucv_resource_data(val, UCLV_WIDGET_TYPE);
		uc_value_t *mode = ucv_object_get(props, "align_mode", NULL);
		uc_value_t *ox = ucv_object_get(props, "align_x", NULL);
		uc_value_t *oy = ucv_object_get(props, "align_y", NULL);

		if (!ref || !ref->obj) {
			uc_vm_raise_exception(vm, EXCEPTION_TYPE,
					      "align_to expects a live widget");
			return NULL;
		}

		if (!mode) {
			uc_vm_raise_exception(vm, EXCEPTION_TYPE,
					      "align_to needs an align_mode");
			return NULL;
		}

		lv_obj_align_to(obj, ref->obj, (lv_align_t)ucv_to_integer(mode),
				ox ? ucv_to_integer(ox) : 0,
				oy ? ucv_to_integer(oy) : 0);
	}

	return ucv_boolean_new(true);
}

static const struct {
	const char *name;
	lv_layout_t layout;
} obj_layouts[] = {
	{ "none",		LV_LAYOUT_NONE },
	{ "flex",		LV_LAYOUT_FLEX },
	{ "grid",		LV_LAYOUT_GRID },
};

/**
 * uc_lv_layout() - w.layout(name)
 * @name: none, flex or grid
 *
 * By name and not by number: lv_layout_t only carries flex and grid when
 * LV_USE_FLEX and LV_USE_GRID are on, so the numbers behind them belong to the
 * build rather than to the interface. none is the way back to placing children
 * by hand after w.flex_flow() or w.grid_dsc().
 *
 * Return: true, or null for an unknown name.
 */
static uc_value_t *
uc_lv_layout(uc_vm_t *vm, size_t nargs)
{
	lv_obj_t *obj = uclv_widget_obj(vm);
	uc_value_t *name = uc_fn_arg(0);
	size_t i;

	if (!obj)
		return NULL;

	if (ucv_type(name) != UC_STRING) {
		uc_vm_raise_exception(vm, EXCEPTION_TYPE,
				      "expecting a layout name");
		return NULL;
	}

	for (i = 0; i < ARRAY_SIZE(obj_layouts); i++) {
		if (strcmp(obj_layouts[i].name, ucv_string_get(name)))
			continue;

		lv_obj_set_layout(obj, obj_layouts[i].layout);

		return ucv_boolean_new(true);
	}

	uc_vm_raise_exception(vm, EXCEPTION_TYPE, "unknown layout: %s",
			      ucv_string_get(name));

	return NULL;
}

/**
 * uc_lv_flex_flow() - w.flex_flow(flow)
 * @flow: an lv.FLEX_FLOW_ constant
 *
 * Puts the widget under flex layout as well, so from here on its children are
 * placed by the flow and x, y, align and align_to on them are ignored.
 *
 * Return: true.
 */
static uc_value_t *
uc_lv_flex_flow(uc_vm_t *vm, size_t nargs)
{
	lv_obj_t *obj = uclv_widget_obj(vm);

	if (!obj)
		return NULL;

	lv_obj_set_flex_flow(obj, (lv_flex_flow_t)ucv_to_integer(uc_fn_arg(0)));

	return ucv_boolean_new(true);
}

/**
 * uc_lv_flex_align() - w.flex_align(main, [cross], [track])
 * @main: where the items sit along the flow, an lv.FLEX_ALIGN_ constant
 * @cross: where an item sits across its track, default START
 * @track: where the tracks sit when the flow wraps, default START
 *
 * Puts the widget under flex layout as well.
 *
 * Return: true.
 */
static uc_value_t *
uc_lv_flex_align(uc_vm_t *vm, size_t nargs)
{
	lv_obj_t *obj = uclv_widget_obj(vm);
	uc_value_t *cross = uc_fn_arg(1);
	uc_value_t *track = uc_fn_arg(2);

	if (!obj)
		return NULL;

	lv_obj_set_flex_align(obj,
			      (lv_flex_align_t)ucv_to_integer(uc_fn_arg(0)),
			      cross ? (lv_flex_align_t)ucv_to_integer(cross)
				    : LV_FLEX_ALIGN_START,
			      track ? (lv_flex_align_t)ucv_to_integer(track)
				    : LV_FLEX_ALIGN_START);

	return ucv_boolean_new(true);
}

/**
 * uc_lv_flex_grow() - w.flex_grow(share)
 * @share: 0 to 255, a share of the free space along the parent's flow, or 0 to
 *         keep the size the widget was given
 *
 * A property of the child rather than of the container, and the only one here
 * that does not turn a layout on: the parent must already be a flex one.
 *
 * Return: true, or null on a widget with no parent.
 */
static uc_value_t *
uc_lv_flex_grow(uc_vm_t *vm, size_t nargs)
{
	lv_obj_t *obj = uclv_widget_obj(vm);
	int64_t grow;

	if (!obj)
		return NULL;

	/* lv_obj_set_flex_grow() marks the parent dirty without looking for one
	   first, and a screen has none. */
	if (!lv_obj_get_parent(obj)) {
		uc_vm_raise_exception(vm, EXCEPTION_TYPE,
				      "flex_grow() needs a widget with a parent");
		return NULL;
	}

	grow = ucv_to_integer(uc_fn_arg(0));

	if (grow < 0)
		grow = 0;
	else if (grow > UINT8_MAX)
		grow = UINT8_MAX;

	lv_obj_set_flex_grow(obj, (uint8_t)grow);

	return ucv_boolean_new(true);
}

static int32_t *
uclv_track_array(uc_vm_t *vm, uc_value_t *arr, const char *what)
{
	int32_t *track;
	size_t i, len;

	if (ucv_type(arr) != UC_ARRAY) {
		uc_vm_raise_exception(vm, EXCEPTION_TYPE,
				      "expecting an array of %s sizes", what);
		return NULL;
	}

	len = ucv_array_length(arr);

	if (!len) {
		uc_vm_raise_exception(vm, EXCEPTION_TYPE,
				      "expecting at least one %s", what);
		return NULL;
	}

	track = calloc(len + 1, sizeof(*track));

	if (!track)
		return NULL;

	for (i = 0; i < len; i++)
		track[i] = (int32_t)ucv_to_integer(ucv_array_get(arr, i));

	track[len] = LV_GRID_TEMPLATE_LAST;

	return track;
}

static void
uclv_grid_deleted(lv_event_t *e)
{
	uclv_grid_t *grid = lv_event_get_user_data(e);

	free(grid->col);
	free(grid->row);
	free(grid);
}

/**
 * uc_lv_grid_dsc() - w.grid_dsc(cols, rows)
 * @cols: column sizes in pixels, or lv.grid_fr(n), lv.GRID_CONTENT, lv.pct(n)
 * @rows: row sizes, the same
 *
 * Puts the widget under grid layout. A child of it is placed only once it has
 * been given w.grid_cell().
 *
 * Return: true, or null when either argument is not a non-empty array.
 */
static uc_value_t *
uc_lv_grid_dsc(uc_vm_t *vm, size_t nargs)
{
	uclv_widget_t *w = uclv_widget_this(vm);
	int32_t *col, *row, *stale_col, *stale_row;

	if (!w)
		return NULL;

	col = uclv_track_array(vm, uc_fn_arg(0), "column");

	if (!col)
		return NULL;

	row = uclv_track_array(vm, uc_fn_arg(1), "row");

	if (!row) {
		free(col);

		return NULL;
	}

	if (!w->grid) {
		w->grid = calloc(1, sizeof(*w->grid));

		if (!w->grid) {
			free(col);
			free(row);

			return NULL;
		}

		/* The object owns the arrays, not the resource: LVGL keeps both
		   pointers, uclv_widget_free() strips every callback carrying
		   w, and this one carries the arrays instead, so it survives a
		   resource that dies before the object does. */
		lv_obj_add_event_cb(w->obj, uclv_grid_deleted, LV_EVENT_DELETE,
				    w->grid);
	}

	stale_col = w->grid->col;
	stale_row = w->grid->row;

	w->grid->col = col;
	w->grid->row = row;

	/* Install before freeing, so no layout pass can run against a pointer
	   that has already gone. */
	lv_obj_set_grid_dsc_array(w->obj, col, row);

	free(stale_col);
	free(stale_row);

	return ucv_boolean_new(true);
}

/**
 * uc_lv_grid_align() - w.grid_align(col, row)
 * @col: where the columns sit when they do not fill the widget, an
 *       lv.GRID_ALIGN_ constant
 * @row: where the rows sit
 *
 * Only visible when a track is sized in pixels or by content. A grid of
 * lv.grid_fr() tracks leaves nothing over to distribute.
 *
 * Return: true.
 */
static uc_value_t *
uc_lv_grid_align(uc_vm_t *vm, size_t nargs)
{
	lv_obj_t *obj = uclv_widget_obj(vm);

	if (!obj)
		return NULL;

	lv_obj_set_grid_align(obj,
			      (lv_grid_align_t)ucv_to_integer(uc_fn_arg(0)),
			      (lv_grid_align_t)ucv_to_integer(uc_fn_arg(1)));

	return ucv_boolean_new(true);
}

/**
 * uc_lv_grid_cell() - w.grid_cell(col, row, [opts])
 * @col: column, counted from zero
 * @row: row
 * @opts: span_x and span_y, 1 by default; align_x and align_y, lv.GRID_ALIGN_
 *        constants, STRETCH by default, which is the one case where the grid
 *        writes the child's size
 *
 * The spans default to 1 rather than to LVGL's 0, which places nothing at all:
 * lv_grid drops an item of zero span without a word, so a child of a grid that
 * was never given a cell simply never appears.
 *
 * Return: true, or null on a widget with no parent.
 */
static uc_value_t *
uc_lv_grid_cell(uc_vm_t *vm, size_t nargs)
{
	lv_obj_t *obj = uclv_widget_obj(vm);
	uc_value_t *opts = uc_fn_arg(2);
	uc_value_t *span_x = NULL, *span_y = NULL;
	uc_value_t *align_x = NULL, *align_y = NULL;

	if (!obj)
		return NULL;

	/* lv_obj_set_grid_cell() marks the parent dirty without looking for one
	   first, and a screen has none. */
	if (!lv_obj_get_parent(obj)) {
		uc_vm_raise_exception(vm, EXCEPTION_TYPE,
				      "grid_cell() needs a widget with a parent");
		return NULL;
	}

	if (opts && ucv_type(opts) != UC_OBJECT) {
		uc_vm_raise_exception(vm, EXCEPTION_TYPE,
				      "expecting an object of cell options");
		return NULL;
	}

	if (opts) {
		span_x = ucv_object_get(opts, "span_x", NULL);
		span_y = ucv_object_get(opts, "span_y", NULL);
		align_x = ucv_object_get(opts, "align_x", NULL);
		align_y = ucv_object_get(opts, "align_y", NULL);
	}

	lv_obj_set_grid_cell(obj,
			     align_x ? (lv_grid_align_t)ucv_to_integer(align_x)
				     : LV_GRID_ALIGN_STRETCH,
			     (int32_t)ucv_to_integer(uc_fn_arg(0)),
			     span_x ? (int32_t)ucv_to_integer(span_x) : 1,
			     align_y ? (lv_grid_align_t)ucv_to_integer(align_y)
				     : LV_GRID_ALIGN_STRETCH,
			     (int32_t)ucv_to_integer(uc_fn_arg(1)),
			     span_y ? (int32_t)ucv_to_integer(span_y) : 1);

	return ucv_boolean_new(true);
}

/**
 * uc_lv_text() - w.text(string)
 * @string: what the label reads
 *
 * Return: true, or null on a widget that is not a label.
 */
static uc_value_t *
uc_lv_text(uc_vm_t *vm, size_t nargs)
{
	lv_obj_t *obj = uclv_widget_typed(vm, &lv_label_class, "a label");
	uc_value_t *arg = uc_fn_arg(0);
	char *str;

	if (!obj)
		return NULL;

	str = ucv_to_string(vm, arg);

	if (!str)
		return NULL;

	lv_label_set_text(obj, str);
	free(str);

	return ucv_boolean_new(true);
}

/**
 * uc_lv_long_mode() - w.long_mode(mode)
 * @mode: an lv.LABEL_LONG_ constant
 *
 * DOTS needs a bounded height as well as a width, or the label wraps.
 *
 * Return: true, or null on a widget that is not a label.
 */
static uc_value_t *
uc_lv_long_mode(uc_vm_t *vm, size_t nargs)
{
	lv_obj_t *obj = uclv_widget_typed(vm, &lv_label_class, "a label");

	if (!obj)
		return NULL;

	lv_label_set_long_mode(obj,
			       (lv_label_long_mode_t)ucv_to_integer(uc_fn_arg(0)));

	return ucv_boolean_new(true);
}

/**
 * uc_lv_src() - w.src(index)
 * @index: an index from lv.image_load()
 *
 * Return: true, or null on a widget that is not an image.
 */
static uc_value_t *
uc_lv_src(uc_vm_t *vm, size_t nargs)
{
	lv_obj_t *obj = uclv_widget_typed(vm, &lv_image_class, "an image");
	const lv_image_dsc_t *dsc;
	size_t index;

	if (!obj)
		return NULL;

	index = (size_t)ucv_to_integer(uc_fn_arg(0));
	dsc = uclv_image(index);

	if (!dsc) {
		uc_vm_raise_exception(vm, EXCEPTION_RUNTIME,
				      "no such image: %zu", index);
		return NULL;
	}

	lv_image_set_src(obj, dsc);

	return ucv_boolean_new(true);
}

/**
 * uc_lv_value() - w.value(value)
 * @value: the reading, within the range set by w.range()
 *
 * Return: true, or null on a widget that is neither an arc nor a bar.
 */
static uc_value_t *
uc_lv_value(uc_vm_t *vm, size_t nargs)
{
	lv_obj_t *obj = uclv_widget_obj(vm);
	int32_t value;

	if (!obj)
		return NULL;

	value = (int32_t)ucv_to_integer(uc_fn_arg(0));

	/* lv_arc is not an lv_bar subclass, and LV_USE_ASSERT_OBJ is off, so a
	   bar setter on an arc casts an unrelated struct in silence. */
	if (lv_obj_check_type(obj, &lv_arc_class))
		lv_arc_set_value(obj, value);
	else if (lv_obj_check_type(obj, &lv_bar_class))
		lv_bar_set_value(obj, value, LV_ANIM_OFF);
	else {
		uc_vm_raise_exception(vm, EXCEPTION_TYPE,
				      "value() needs a bar or an arc");
		return NULL;
	}

	return ucv_boolean_new(true);
}

/**
 * uc_lv_range() - w.range(min, max)
 * @min: the value at an empty arc or bar
 * @max: the value at a full one
 *
 * Return: true, or null on a widget that is neither an arc nor a bar.
 */
static uc_value_t *
uc_lv_range(uc_vm_t *vm, size_t nargs)
{
	lv_obj_t *obj = uclv_widget_obj(vm);
	int32_t min, max;

	if (!obj)
		return NULL;

	min = (int32_t)ucv_to_integer(uc_fn_arg(0));
	max = (int32_t)ucv_to_integer(uc_fn_arg(1));

	if (lv_obj_check_type(obj, &lv_arc_class))
		lv_arc_set_range(obj, min, max);
	else if (lv_obj_check_type(obj, &lv_bar_class))
		lv_bar_set_range(obj, min, max);
	else {
		uc_vm_raise_exception(vm, EXCEPTION_TYPE,
				      "range() needs a bar or an arc");
		return NULL;
	}

	return ucv_boolean_new(true);
}

/**
 * uc_lv_rotation() - w.rotation(degrees)
 * @degrees: where an arc starts, clockwise from three o'clock
 *
 * Return: true, or null on a widget that is not an arc.
 */
static uc_value_t *
uc_lv_rotation(uc_vm_t *vm, size_t nargs)
{
	lv_obj_t *obj = uclv_widget_typed(vm, &lv_arc_class, "an arc");

	if (!obj)
		return NULL;

	lv_arc_set_rotation(obj, (int32_t)ucv_to_integer(uc_fn_arg(0)));

	return ucv_boolean_new(true);
}

/**
 * uc_lv_bg_angles() - w.bg_angles(start, end)
 * @start: where the unfilled track begins, in degrees
 * @end: where it ends
 *
 * Return: true, or null on a widget that is not an arc.
 */
static uc_value_t *
uc_lv_bg_angles(uc_vm_t *vm, size_t nargs)
{
	lv_obj_t *obj = uclv_widget_typed(vm, &lv_arc_class, "an arc");

	if (!obj)
		return NULL;

	lv_arc_set_bg_angles(obj,
			     (lv_value_precise_t)ucv_to_integer(uc_fn_arg(0)),
			     (lv_value_precise_t)ucv_to_integer(uc_fn_arg(1)));

	return ucv_boolean_new(true);
}

/**
 * uc_lv_anim() - w.anim(to, ms)
 * @to: the value to travel to
 * @ms: how long it takes
 *
 * Eased out. The binding offers no other animation.
 *
 * Return: true, or null on a widget that is not an arc.
 */
static uc_value_t *
uc_lv_anim(uc_vm_t *vm, size_t nargs)
{
	lv_obj_t *obj = uclv_widget_typed(vm, &lv_arc_class, "an arc");
	lv_anim_t a;
	int32_t to;

	if (!obj)
		return NULL;

	to = (int32_t)ucv_to_integer(uc_fn_arg(0));

	lv_anim_init(&a);
	lv_anim_set_var(&a, obj);
	lv_anim_set_exec_cb(&a, (lv_anim_exec_xcb_t)lv_arc_set_value);
	lv_anim_set_values(&a, lv_arc_get_value(obj), to);
	lv_anim_set_duration(&a, (uint32_t)ucv_to_unsigned(uc_fn_arg(1)));
	lv_anim_set_path_cb(&a, lv_anim_path_ease_out);
	lv_anim_start(&a);

	return ucv_boolean_new(true);
}

/**
 * uc_lv_on() - w.on(code, handler)
 * @code: an lv.EVENT_ constant
 * @handler: called with the event code when the event fires
 *
 * Several handlers may share one code. The widget stays anchored while it is on
 * screen, because LVGL can fire after ucode drops its last reference. A handler
 * that deletes part of the tree must defer that work.
 *
 * Return: true.
 */
static uc_value_t *
uc_lv_on(uc_vm_t *vm, size_t nargs)
{
	uclv_widget_t *w = uclv_widget_this(vm);
	uc_value_t *cb = uc_fn_arg(1);
	uc_value_t *handlers;
	lv_event_code_t code;
	size_t i;

	if (!w)
		return NULL;

	if (!ucv_is_callable(cb)) {
		uc_vm_raise_exception(vm, EXCEPTION_TYPE,
				      "expecting a callable handler");
		return NULL;
	}

	handlers = ucv_resource_value_get(w->res, WIDGET_UV_ON_EVENT);

	if (ucv_type(handlers) != UC_ARRAY) {
		handlers = ucv_array_new(vm);
		ucv_resource_value_set(w->res, WIDGET_UV_ON_EVENT, handlers);
	}

	code = (lv_event_code_t)ucv_to_integer(uc_fn_arg(0));

	ucv_array_push(handlers, ucv_int64_new(code));
	ucv_array_push(handlers, ucv_get(cb));

	if (!w->anchored) {
		w->anchored = true;
		ucv_get(w->res);
		ucv_resource_persistent_set(w->res, true);
	}

	for (i = 0; i < w->code_count; i++)
		if (w->codes[i] == code)
			return ucv_boolean_new(true);

	if (w->code_count >= UCLV_EVENT_MAX) {
		uc_vm_raise_exception(vm, EXCEPTION_RUNTIME,
				      "at most %d event codes are supported",
				      UCLV_EVENT_MAX);
		return NULL;
	}

	w->codes[w->code_count++] = code;
	lv_obj_add_event_cb(w->obj, uclv_widget_event, code, w);

	return ucv_boolean_new(true);
}

/**
 * uc_lv_delete() - w.delete()
 *
 * Never call this from the widget's own handler. Defer it.
 *
 * Return: true.
 */
static uc_value_t *
uc_lv_delete(uc_vm_t *vm, size_t nargs)
{
	lv_obj_t *obj = uclv_widget_obj(vm);

	if (!obj)
		return NULL;

	lv_obj_delete(obj);

	return ucv_boolean_new(true);
}

/**
 * uc_lv_clean() - w.clean()
 *
 * Deletes every child and keeps the widget.
 *
 * Return: true.
 */
static uc_value_t *
uc_lv_clean(uc_vm_t *vm, size_t nargs)
{
	lv_obj_t *obj = uclv_widget_obj(vm);

	if (!obj)
		return NULL;

	lv_obj_clean(obj);

	return ucv_boolean_new(true);
}

/**
 * uc_lv_hidden() - w.hidden(on)
 * @on: whether the widget is drawn
 *
 * Return: true.
 */
static uc_value_t *
uc_lv_hidden(uc_vm_t *vm, size_t nargs)
{
	lv_obj_t *obj = uclv_widget_obj(vm);

	if (!obj)
		return NULL;

	if (ucv_is_truish(uc_fn_arg(0)))
		lv_obj_add_flag(obj, LV_OBJ_FLAG_HIDDEN);
	else
		lv_obj_remove_flag(obj, LV_OBJ_FLAG_HIDDEN);

	return ucv_boolean_new(true);
}

/**
 * uc_lv_clickable() - w.clickable(on)
 * @on: whether the widget takes a press
 *
 * A scroll area needs this, or lv_obj_hit_test() rejects the press and nothing
 * scrolls.
 *
 * Return: true.
 */
static uc_value_t *
uc_lv_clickable(uc_vm_t *vm, size_t nargs)
{
	lv_obj_t *obj = uclv_widget_obj(vm);

	if (!obj)
		return NULL;

	if (ucv_is_truish(uc_fn_arg(0)))
		lv_obj_add_flag(obj, LV_OBJ_FLAG_CLICKABLE);
	else
		lv_obj_remove_flag(obj, LV_OBJ_FLAG_CLICKABLE);

	return ucv_boolean_new(true);
}

static const struct {
	const char *name;
	lv_obj_flag_t flag;
} obj_flags[] = {
	{ "clickable",		LV_OBJ_FLAG_CLICKABLE },
	{ "hidden",		LV_OBJ_FLAG_HIDDEN },
	{ "scrollable",		LV_OBJ_FLAG_SCROLLABLE },
	{ "scroll_elastic",	LV_OBJ_FLAG_SCROLL_ELASTIC },
	{ "scroll_momentum",	LV_OBJ_FLAG_SCROLL_MOMENTUM },
	{ "scroll_one",		LV_OBJ_FLAG_SCROLL_ONE },
	{ "scroll_chain_hor",	LV_OBJ_FLAG_SCROLL_CHAIN_HOR },
	{ "scroll_chain_ver",	LV_OBJ_FLAG_SCROLL_CHAIN_VER },
	{ "snappable",		LV_OBJ_FLAG_SNAPPABLE },
	{ "press_lock",		LV_OBJ_FLAG_PRESS_LOCK },
	{ "event_bubble",	LV_OBJ_FLAG_EVENT_BUBBLE },
	{ "gesture_bubble",	LV_OBJ_FLAG_GESTURE_BUBBLE },
	{ "adv_hittest",	LV_OBJ_FLAG_ADV_HITTEST },
	{ "ignore_layout",	LV_OBJ_FLAG_IGNORE_LAYOUT },
	{ "floating",		LV_OBJ_FLAG_FLOATING },
	{ "overflow_visible",	LV_OBJ_FLAG_OVERFLOW_VISIBLE },
};

/**
 * uc_lv_flag() - w.flag(name, on)
 * @name: a flag by name, such as scroll_chain_hor, scroll_elastic
 * @on: whether to set it
 *
 * Return: true, or null for an unknown name.
 */
static uc_value_t *
uc_lv_flag(uc_vm_t *vm, size_t nargs)
{
	lv_obj_t *obj = uclv_widget_obj(vm);
	uc_value_t *name = uc_fn_arg(0);
	size_t i;

	if (!obj)
		return NULL;

	if (ucv_type(name) != UC_STRING) {
		uc_vm_raise_exception(vm, EXCEPTION_TYPE,
				      "expecting a flag name");
		return NULL;
	}

	for (i = 0; i < ARRAY_SIZE(obj_flags); i++) {
		if (strcmp(obj_flags[i].name, ucv_string_get(name)))
			continue;

		if (ucv_is_truish(uc_fn_arg(1)))
			lv_obj_add_flag(obj, obj_flags[i].flag);
		else
			lv_obj_remove_flag(obj, obj_flags[i].flag);

		return ucv_boolean_new(true);
	}

	uc_vm_raise_exception(vm, EXCEPTION_TYPE, "unknown flag: %s",
			      ucv_string_get(name));

	return NULL;
}

/**
 * uc_lv_scrollable() - w.scrollable(on)
 * @on: whether a drag moves the widget's content
 *
 * Return: true.
 */
static uc_value_t *
uc_lv_scrollable(uc_vm_t *vm, size_t nargs)
{
	lv_obj_t *obj = uclv_widget_obj(vm);

	if (!obj)
		return NULL;

	if (ucv_is_truish(uc_fn_arg(0)))
		lv_obj_add_flag(obj, LV_OBJ_FLAG_SCROLLABLE);
	else
		lv_obj_remove_flag(obj, LV_OBJ_FLAG_SCROLLABLE);

	return ucv_boolean_new(true);
}

/**
 * uc_lv_scroll_dir() - w.scroll_dir(dir)
 * @dir: an lv.DIR_ constant
 *
 * lv_indev walks up for an ancestor that scrolls in the drag direction, so an
 * area held to one axis passes the other axis through to the tileview.
 *
 * Return: true.
 */
static uc_value_t *
uc_lv_scroll_dir(uc_vm_t *vm, size_t nargs)
{
	lv_obj_t *obj = uclv_widget_obj(vm);

	if (!obj)
		return NULL;

	lv_obj_set_scroll_dir(obj, (lv_dir_t)ucv_to_integer(uc_fn_arg(0)));

	return ucv_boolean_new(true);
}

/**
 * UCLV_WIDGET_GET() - define w.<name>(), which reads a laid out measurement
 * @name: the ucode name
 * @getter: the LVGL call behind it
 *
 * The laid out number, which is not always the one that was set: a widget can
 * take its size from its content, or from a flex or grid parent. A size only
 * marks the tree dirty, so the layout runs first.
 */
#define UCLV_WIDGET_GET(name, getter)					\
	static uc_value_t *						\
	uc_lv_##name(uc_vm_t *vm, size_t nargs)				\
	{								\
		lv_obj_t *obj = uclv_widget_obj(vm);			\
									\
		if (!obj)						\
			return NULL;					\
									\
		lv_obj_update_layout(obj);				\
									\
		return ucv_int64_new(getter(obj));			\
	}

UCLV_WIDGET_GET(width,		lv_obj_get_width)
UCLV_WIDGET_GET(height,		lv_obj_get_height)
UCLV_WIDGET_GET(x,		lv_obj_get_x)
UCLV_WIDGET_GET(y,		lv_obj_get_y)
UCLV_WIDGET_GET(content_width,	lv_obj_get_content_width)
UCLV_WIDGET_GET(content_height,	lv_obj_get_content_height)
UCLV_WIDGET_GET(self_width,	lv_obj_get_self_width)
UCLV_WIDGET_GET(self_height,	lv_obj_get_self_height)

/**
 * uc_lv_coords() - w.coords()
 *
 * Screen coordinates, not the parent relative ones w.x() answers, so two
 * widgets on different parents can be compared.
 *
 * Return: x1, y1, x2, y2, w and h.
 */
static uc_value_t *
uc_lv_coords(uc_vm_t *vm, size_t nargs)
{
	lv_obj_t *obj = uclv_widget_obj(vm);
	uc_value_t *rv;
	lv_area_t area;

	if (!obj)
		return NULL;

	lv_obj_update_layout(obj);
	lv_obj_get_coords(obj, &area);

	rv = ucv_object_new(vm);

	ucv_object_add(rv, "x1", ucv_int64_new(area.x1));
	ucv_object_add(rv, "y1", ucv_int64_new(area.y1));
	ucv_object_add(rv, "x2", ucv_int64_new(area.x2));
	ucv_object_add(rv, "y2", ucv_int64_new(area.y2));
	ucv_object_add(rv, "w", ucv_int64_new(lv_area_get_width(&area)));
	ucv_object_add(rv, "h", ucv_int64_new(lv_area_get_height(&area)));

	return rv;
}

/**
 * uc_lv_update_layout() - w.update_layout()
 *
 * Runs every pending layout on the screen the widget is on, so a run of
 * measurements pays for it once rather than once per getter.
 *
 * LVGL drops a re-entrant call, so a measurement taken from inside a layout
 * driven handler answers the sizes from before that pass.
 *
 * Return: true.
 */
static uc_value_t *
uc_lv_update_layout(uc_vm_t *vm, size_t nargs)
{
	lv_obj_t *obj = uclv_widget_obj(vm);

	if (!obj)
		return NULL;

	lv_obj_update_layout(obj);

	return ucv_boolean_new(true);
}

/**
 * uc_lv_scrollbar() - w.scrollbar(mode)
 * @mode: an lv.SCROLLBAR_ constant
 *
 * Return: true.
 */
static uc_value_t *
uc_lv_scrollbar(uc_vm_t *vm, size_t nargs)
{
	lv_obj_t *obj = uclv_widget_obj(vm);

	if (!obj)
		return NULL;

	lv_obj_set_scrollbar_mode(obj,
				  (lv_scrollbar_mode_t)ucv_to_integer(uc_fn_arg(0)));

	return ucv_boolean_new(true);
}

/**
 * uc_lv_remove_style() - w.remove_style(selector)
 * @selector: a part ORed with a state
 *
 * Return: true.
 */
static uc_value_t *
uc_lv_remove_style(uc_vm_t *vm, size_t nargs)
{
	lv_obj_t *obj = uclv_widget_obj(vm);

	if (!obj)
		return NULL;

	lv_obj_remove_style(obj, NULL,
			    (lv_style_selector_t)ucv_to_unsigned(uc_fn_arg(0)));

	return ucv_boolean_new(true);
}

/**
 * uc_lv_series() - w.series(colour)
 * @colour: the colour the series is drawn in
 *
 * Return: the series index for w.push() and w.points(), or null past the
 * series limit.
 */
static uc_value_t *
uc_lv_series(uc_vm_t *vm, size_t nargs)
{
	uclv_widget_t *w = uclv_widget_this(vm);
	lv_chart_series_t *ser;

	if (!w)
		return NULL;

	if (!lv_obj_check_type(w->obj, &lv_chart_class)) {
		uc_vm_raise_exception(vm, EXCEPTION_TYPE,
				      "series() needs a chart");
		return NULL;
	}

	if (w->series_count >= UCLV_SERIES_MAX) {
		uc_vm_raise_exception(vm, EXCEPTION_RUNTIME,
				      "at most %d chart series are supported",
				      UCLV_SERIES_MAX);
		return NULL;
	}

	ser = lv_chart_add_series(w->obj,
				  lv_color_hex((uint32_t)ucv_to_unsigned(uc_fn_arg(0))),
				  LV_CHART_AXIS_PRIMARY_Y);

	if (!ser)
		return NULL;

	w->series[w->series_count] = ser;

	return ucv_int64_new(w->series_count++);
}

/**
 * uc_lv_push() - w.push(series, value)
 * @series: an index from w.series()
 * @value: the sample, within the chart range
 *
 * Return: true.
 */
static uc_value_t *
uc_lv_push(uc_vm_t *vm, size_t nargs)
{
	uclv_widget_t *w = uclv_widget_this(vm);
	size_t idx;

	if (!w)
		return NULL;

	idx = (size_t)ucv_to_integer(uc_fn_arg(0));

	if (idx >= w->series_count) {
		uc_vm_raise_exception(vm, EXCEPTION_RUNTIME,
				      "no such chart series: %zu", idx);
		return NULL;
	}

	lv_chart_set_next_value(w->obj, w->series[idx],
				(int32_t)ucv_to_integer(uc_fn_arg(1)));

	return ucv_boolean_new(true);
}

/**
 * uc_lv_points() - w.points(series, values)
 * @series: an index from w.series()
 * @values: the whole series at once, oldest first
 *
 * Return: true.
 */
static uc_value_t *
uc_lv_points(uc_vm_t *vm, size_t nargs)
{
	uclv_widget_t *w = uclv_widget_this(vm);
	uc_value_t *arr = uc_fn_arg(1);
	size_t idx, i, len;

	if (!w)
		return NULL;

	idx = (size_t)ucv_to_integer(uc_fn_arg(0));

	if (idx >= w->series_count) {
		uc_vm_raise_exception(vm, EXCEPTION_RUNTIME,
				      "no such chart series: %zu", idx);
		return NULL;
	}

	if (ucv_type(arr) != UC_ARRAY) {
		uc_vm_raise_exception(vm, EXCEPTION_TYPE,
				      "expecting an array of values");
		return NULL;
	}

	len = ucv_array_length(arr);

	for (i = 0; i < len; i++)
		lv_chart_set_next_value(w->obj, w->series[idx],
					(int32_t)ucv_to_integer(ucv_array_get(arr, i)));

	return ucv_boolean_new(true);
}

/**
 * uc_lv_chart_range() - w.chart_range(min, max)
 * @min: the value at the baseline
 * @max: the value at full height
 *
 * Keep the range small: LVGL maps a point with an int32 multiply.
 *
 * Return: true, or null on a widget that is not a chart.
 */
static uc_value_t *
uc_lv_chart_range(uc_vm_t *vm, size_t nargs)
{
	lv_obj_t *obj = uclv_widget_typed(vm, &lv_chart_class, "a chart");

	if (!obj)
		return NULL;

	lv_chart_set_axis_range(obj, LV_CHART_AXIS_PRIMARY_Y,
				(int32_t)ucv_to_integer(uc_fn_arg(0)),
				(int32_t)ucv_to_integer(uc_fn_arg(1)));

	return ucv_boolean_new(true);
}

/**
 * uc_lv_point_count() - w.point_count(count)
 * @count: how many samples the chart holds
 *
 * Return: true, or null on a widget that is not a chart.
 */
static uc_value_t *
uc_lv_point_count(uc_vm_t *vm, size_t nargs)
{
	lv_obj_t *obj = uclv_widget_typed(vm, &lv_chart_class, "a chart");

	if (!obj)
		return NULL;

	lv_chart_set_point_count(obj, (uint32_t)ucv_to_unsigned(uc_fn_arg(0)));

	return ucv_boolean_new(true);
}

/**
 * uc_lv_chart_type() - w.chart_type(type)
 * @type: an lv.CHART_TYPE_ constant
 *
 * Return: true, or null on a widget that is not a chart.
 */
static uc_value_t *
uc_lv_chart_type(uc_vm_t *vm, size_t nargs)
{
	lv_obj_t *obj = uclv_widget_typed(vm, &lv_chart_class, "a chart");

	if (!obj)
		return NULL;

	lv_chart_set_type(obj, (lv_chart_type_t)ucv_to_integer(uc_fn_arg(0)));

	return ucv_boolean_new(true);
}

/**
 * uc_lv_update_mode() - w.update_mode(mode)
 * @mode: an lv.CHART_UPDATE_ constant
 *
 * Return: true, or null on a widget that is not a chart.
 */
static uc_value_t *
uc_lv_update_mode(uc_vm_t *vm, size_t nargs)
{
	lv_obj_t *obj = uclv_widget_typed(vm, &lv_chart_class, "a chart");

	if (!obj)
		return NULL;

	lv_chart_set_update_mode(obj,
				 (lv_chart_update_mode_t)ucv_to_integer(uc_fn_arg(0)));

	return ucv_boolean_new(true);
}

/**
 * uc_lv_div_lines() - w.div_lines(horizontal, vertical)
 * @horizontal: how many lines across
 * @vertical: how many down
 *
 * Return: true, or null on a widget that is not a chart.
 */
static uc_value_t *
uc_lv_div_lines(uc_vm_t *vm, size_t nargs)
{
	lv_obj_t *obj = uclv_widget_typed(vm, &lv_chart_class, "a chart");

	if (!obj)
		return NULL;

	lv_chart_set_div_line_count(obj,
				    (uint8_t)ucv_to_unsigned(uc_fn_arg(0)),
				    (uint8_t)ucv_to_unsigned(uc_fn_arg(1)));

	return ucv_boolean_new(true);
}

/**
 * uc_lv_tile_add() - w.tile_add(col, row, dir)
 * @col: column in the tileview
 * @row: row in it
 * @dir: the lv.DIR_ constants a drag from this tile may travel in
 *
 * Return: the tile, to build a page on.
 */
static uc_value_t *
uc_lv_tile_add(uc_vm_t *vm, size_t nargs)
{
	lv_obj_t *obj = uclv_widget_typed(vm, &lv_tileview_class, "a tileview");
	uclv_widget_t *tile;
	uc_value_t *res;
	int col, row;

	if (!obj)
		return NULL;

	col = (int)ucv_to_integer(uc_fn_arg(0));
	row = (int)ucv_to_integer(uc_fn_arg(1));

	res = uclv_widget_wrap(vm,
			       lv_tileview_add_tile(obj, col, row,
						    (lv_dir_t)ucv_to_integer(uc_fn_arg(2))));

	if (!res)
		return NULL;

	tile = ucv_resource_data(res, UCLV_WIDGET_TYPE);
	tile->tile_col = col;
	tile->tile_row = row;

	return res;
}

/**
 * uc_lv_tile_set() - w.tile_set(col, row, animate)
 * @col: column to travel to
 * @row: row to travel to
 * @animate: whether to slide, or arrive at once
 *
 * Works whether or not the tileview is scrollable.
 *
 * Return: true.
 */
static uc_value_t *
uc_lv_tile_set(uc_vm_t *vm, size_t nargs)
{
	lv_obj_t *obj = uclv_widget_typed(vm, &lv_tileview_class, "a tileview");

	if (!obj)
		return NULL;

	lv_tileview_set_tile_by_index(obj,
				      (uint32_t)ucv_to_unsigned(uc_fn_arg(0)),
				      (uint32_t)ucv_to_unsigned(uc_fn_arg(1)),
				      ucv_is_truish(uc_fn_arg(2)) ? LV_ANIM_ON
								  : LV_ANIM_OFF);

	return ucv_boolean_new(true);
}

/**
 * uc_lv_tile_active() - w.tile_active()
 *
 * Return: col and row of the tile the view has settled on, or null.
 */
static uc_value_t *
uc_lv_tile_active(uc_vm_t *vm, size_t nargs)
{
	lv_obj_t *obj = uclv_widget_typed(vm, &lv_tileview_class, "a tileview");
	uclv_widget_t *tile;
	lv_obj_t *active;
	uc_value_t *rv;

	if (!obj)
		return NULL;

	active = lv_tileview_get_tile_active(obj);
	tile = active ? lv_obj_get_user_data(active) : NULL;

	if (!tile)
		return NULL;

	rv = ucv_object_new(vm);

	ucv_object_add(rv, "col", ucv_int64_new(tile->tile_col));
	ucv_object_add(rv, "row", ucv_int64_new(tile->tile_row));

	return rv;
}

/**
 * uc_lv_qr_size() - w.qr_size(px)
 * @px: the side of the square
 *
 * Set this before the payload.
 *
 * Return: true, or null on a widget that is not a qrcode.
 */
static uc_value_t *
uc_lv_qr_size(uc_vm_t *vm, size_t nargs)
{
	lv_obj_t *obj = uclv_widget_typed(vm, &lv_qrcode_class, "a qrcode");

	if (!obj)
		return NULL;

	lv_qrcode_set_size(obj, (int32_t)ucv_to_integer(uc_fn_arg(0)));

	return ucv_boolean_new(true);
}

/**
 * uc_lv_qr_colors() - w.qr_colors(dark, light)
 * @dark: the module colour
 * @light: the background
 *
 * Set these before the payload.
 *
 * Return: true, or null on a widget that is not a qrcode.
 */
static uc_value_t *
uc_lv_qr_colors(uc_vm_t *vm, size_t nargs)
{
	lv_obj_t *obj = uclv_widget_typed(vm, &lv_qrcode_class, "a qrcode");

	if (!obj)
		return NULL;

	lv_qrcode_set_dark_color(obj,
				 lv_color_hex((uint32_t)ucv_to_unsigned(uc_fn_arg(0))));
	lv_qrcode_set_light_color(obj,
				  lv_color_hex((uint32_t)ucv_to_unsigned(uc_fn_arg(1))));

	return ucv_boolean_new(true);
}

/**
 * uc_lv_qr_update() - w.qr_update(payload)
 * @payload: the string to encode, taken as bytes so a key goes over verbatim
 *
 * This call allocates the draw buffer and renders, so the size and the colours
 * must already be set.
 *
 * Return: true, or null on a widget that is not a qrcode.
 */
static uc_value_t *
uc_lv_qr_update(uc_vm_t *vm, size_t nargs)
{
	lv_obj_t *obj = uclv_widget_typed(vm, &lv_qrcode_class, "a qrcode");
	uc_value_t *arg = uc_fn_arg(0);

	if (!obj)
		return NULL;

	if (ucv_type(arg) != UC_STRING) {
		uc_vm_raise_exception(vm, EXCEPTION_TYPE,
				      "expecting a payload string");
		return NULL;
	}

	return ucv_boolean_new(lv_qrcode_update(obj, ucv_string_get(arg),
						ucv_string_length(arg)) ==
			       LV_RESULT_OK);
}

static const uc_function_list_t widget_fns[] = {
	{ "style",		uc_lv_style },
	{ "set",		uc_lv_set },
	{ "layout",		uc_lv_layout },
	{ "flex_flow",		uc_lv_flex_flow },
	{ "flex_align",		uc_lv_flex_align },
	{ "flex_grow",		uc_lv_flex_grow },
	{ "grid_dsc",		uc_lv_grid_dsc },
	{ "grid_align",		uc_lv_grid_align },
	{ "grid_cell",		uc_lv_grid_cell },
	{ "text",		uc_lv_text },
	{ "long_mode",		uc_lv_long_mode },
	{ "src",		uc_lv_src },
	{ "value",		uc_lv_value },
	{ "range",		uc_lv_range },
	{ "rotation",		uc_lv_rotation },
	{ "bg_angles",		uc_lv_bg_angles },
	{ "anim",		uc_lv_anim },
	{ "on",			uc_lv_on },
	{ "delete",		uc_lv_delete },
	{ "clean",		uc_lv_clean },
	{ "hidden",		uc_lv_hidden },
	{ "clickable",		uc_lv_clickable },
	{ "scrollable",		uc_lv_scrollable },
	{ "flag",		uc_lv_flag },
	{ "scrollbar",		uc_lv_scrollbar },
	{ "scroll_dir",		uc_lv_scroll_dir },
	{ "width",		uc_lv_width },
	{ "height",		uc_lv_height },
	{ "x",			uc_lv_x },
	{ "y",			uc_lv_y },
	{ "content_width",	uc_lv_content_width },
	{ "content_height",	uc_lv_content_height },
	{ "self_width",		uc_lv_self_width },
	{ "self_height",	uc_lv_self_height },
	{ "coords",		uc_lv_coords },
	{ "update_layout",	uc_lv_update_layout },
	{ "remove_style",	uc_lv_remove_style },
	{ "series",		uc_lv_series },
	{ "push",		uc_lv_push },
	{ "points",		uc_lv_points },
	{ "chart_range",	uc_lv_chart_range },
	{ "point_count",	uc_lv_point_count },
	{ "chart_type",		uc_lv_chart_type },
	{ "update_mode",	uc_lv_update_mode },
	{ "div_lines",		uc_lv_div_lines },
	{ "tile_add",		uc_lv_tile_add },
	{ "tile_set",		uc_lv_tile_set },
	{ "tile_active",	uc_lv_tile_active },
	{ "qr_size",		uc_lv_qr_size },
	{ "qr_colors",		uc_lv_qr_colors },
	{ "qr_update",		uc_lv_qr_update },
};

void
uclv_widget_register(uc_vm_t *vm)
{
	uc_type_declare(vm, UCLV_WIDGET_TYPE, widget_fns, uclv_widget_free);
}
