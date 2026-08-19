/*
 * SPDX-License-Identifier: GPL-2.0-only
 *
 * Copyright (C) 2026 John Crispin <john@phrozen.org>
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>

#include <xf86drm.h>

#include <ucode/module.h>

#include "ucode-lv.h"

/* Included by path, because lvgl.h does not pull in the bundled libraries. */
#include "src/libs/lodepng/lodepng.h"
#include "src/display/lv_display_private.h"

static bool lv_ready;
static lv_display_t *display;
static lv_indev_t *indev;

static uint32_t
uclv_tick(void)
{
	struct timespec ts;

	clock_gettime(CLOCK_MONOTONIC, &ts);

	return (uint32_t)(ts.tv_sec * 1000 + ts.tv_nsec / 1000000);
}

static lv_obj_t *
uclv_parent(uc_vm_t *vm, uc_value_t *val)
{
	uclv_widget_t *w;

	if (!val)
		return lv_screen_active();

	w = ucv_resource_data(val, UCLV_WIDGET_TYPE);

	if (!w || !w->obj) {
		uc_vm_raise_exception(vm, EXCEPTION_TYPE,
				      "expecting a live widget as parent");
		return NULL;
	}

	return w->obj;
}

/**
 * uc_lv_init() - lv.init()
 *
 * Starts LVGL and installs a monotonic tick source, so uloop needs no 1 ms
 * timer beside it. Idempotent.
 *
 * Return: true.
 */
static uc_value_t *
uc_lv_init(uc_vm_t *vm, size_t nargs)
{
	if (lv_ready)
		return ucv_boolean_new(true);

	lv_tick_set_cb(uclv_tick);
	lv_init();

	lv_ready = true;

	return ucv_boolean_new(true);
}

/**
 * uc_lv_display_drm() - lv.display_drm([path], [connector])
 * @path: DRM device, default /dev/dri/card0
 * @connector: connector id, or null for the first
 *
 * Return: true, also when the display is already open.
 */
static uc_value_t *
uc_lv_display_drm(uc_vm_t *vm, size_t nargs)
{
	uc_value_t *path = uc_fn_arg(0);
	uc_value_t *conn = uc_fn_arg(1);
	const char *dev;

	if (!lv_ready) {
		uc_vm_raise_exception(vm, EXCEPTION_RUNTIME,
				      "lv.init() has not been called");
		return NULL;
	}

	if (display)
		return ucv_boolean_new(true);

	if (path && ucv_type(path) != UC_STRING) {
		uc_vm_raise_exception(vm, EXCEPTION_TYPE,
				      "expecting a display device path");
		return NULL;
	}

	dev = path ? ucv_string_get(path) : "/dev/dri/card0";

	display = lv_linux_drm_create();

	if (!display)
		return ucv_boolean_new(false);

	lv_linux_drm_set_file(display, dev,
			      conn ? (int64_t)ucv_to_integer(conn) : -1);

	/* set_file() returns void and reports failure by leaving the fd at -1. */
	if (lv_linux_drm_get_fd(display) < 0) {
		display = NULL;

		return ucv_boolean_new(false);
	}

	return ucv_boolean_new(true);
}

/**
 * uc_lv_indev_evdev() - lv.indev_evdev(path)
 * @path: evdev node with absolute pointer events
 *
 * Return: whether the device opened.
 */
static uc_value_t *
uc_lv_indev_evdev(uc_vm_t *vm, size_t nargs)
{
	uc_value_t *path = uc_fn_arg(0);
	const char *dev;

	if (!lv_ready) {
		uc_vm_raise_exception(vm, EXCEPTION_RUNTIME,
				      "lv.init() has not been called");
		return NULL;
	}

	if (!path || ucv_type(path) != UC_STRING) {
		uc_vm_raise_exception(vm, EXCEPTION_TYPE,
				      "expecting an input device path");
		return NULL;
	}

	dev = ucv_string_get(path);
	indev = lv_evdev_create(LV_INDEV_TYPE_POINTER, dev);

	return ucv_boolean_new(indev != NULL);
}

/**
 * uc_lv_touch_calibrate() - lv.touch_calibrate(x_min, y_min, x_max, y_max)
 * @x_min: raw value at the left edge
 * @y_min: raw value at the top edge
 * @x_max: raw value at the right edge
 * @y_max: raw value at the bottom edge
 *
 * Return: false when no input device is open.
 */
static uc_value_t *
uc_lv_touch_calibrate(uc_vm_t *vm, size_t nargs)
{
	if (!indev)
		return ucv_boolean_new(false);

	lv_evdev_set_calibration(indev,
				 (int)ucv_to_integer(uc_fn_arg(0)),
				 (int)ucv_to_integer(uc_fn_arg(1)),
				 (int)ucv_to_integer(uc_fn_arg(2)),
				 (int)ucv_to_integer(uc_fn_arg(3)));

	return ucv_boolean_new(true);
}

/**
 * uc_lv_touch_swap_axes() - lv.touch_swap_axes(on)
 * @on: whether the device reports y where x is expected
 *
 * Return: false when no input device is open.
 */
static uc_value_t *
uc_lv_touch_swap_axes(uc_vm_t *vm, size_t nargs)
{
	if (!indev)
		return ucv_boolean_new(false);

	lv_evdev_set_swap_axes(indev, ucv_is_truish(uc_fn_arg(0)));

	return ucv_boolean_new(true);
}

/**
 * uc_lv_touch_point() - lv.touch_point()
 *
 * Return: x, y and pressed, or null when no input device is open.
 */
static uc_value_t *
uc_lv_touch_point(uc_vm_t *vm, size_t nargs)
{
	uc_value_t *rv;
	lv_point_t point;

	if (!indev)
		return NULL;

	lv_indev_get_point(indev, &point);

	rv = ucv_object_new(vm);

	ucv_object_add(rv, "x", ucv_int64_new(point.x));
	ucv_object_add(rv, "y", ucv_int64_new(point.y));
	ucv_object_add(rv, "pressed",
		       ucv_boolean_new(lv_indev_get_state(indev) ==
				       LV_INDEV_STATE_PRESSED));

	return rv;
}

/**
 * uc_lv_gesture_dir() - lv.gesture_dir()
 *
 * LVGL detects a gesture only while nothing scrolls, and bubbles it up to the
 * screen, so one handler there sees a swipe that starts anywhere on it.
 *
 * Return: an lv.DIR_ constant, or null.
 */
static uc_value_t *
uc_lv_gesture_dir(uc_vm_t *vm, size_t nargs)
{
	if (!indev)
		return NULL;

	return ucv_int64_new(lv_indev_get_gesture_dir(indev));
}

/**
 * uc_lv_touch_drop() - lv.touch_drop()
 *
 * LVGL ignores the finger until it comes off, and gives nothing a press, a
 * drag or a click from it. Call before a screen load, or the new screen takes
 * over the finger that is still down.
 *
 * Return: whether there was an input device to tell.
 */
static uc_value_t *
uc_lv_touch_drop(uc_vm_t *vm, size_t nargs)
{
	if (!indev)
		return ucv_boolean_new(false);

	lv_indev_wait_release(indev);

	return ucv_boolean_new(true);
}

static uint8_t *
file_slurp(const char *path, size_t *size)
{
	FILE *fp = fopen(path, "rb");
	uint8_t *buf;
	long len;

	if (!fp)
		return NULL;

	if (fseek(fp, 0, SEEK_END) != 0 || (len = ftell(fp)) <= 0) {
		fclose(fp);

		return NULL;
	}

	rewind(fp);
	buf = malloc((size_t)len);

	if (!buf || fread(buf, 1, (size_t)len, fp) != (size_t)len) {
		free(buf);
		fclose(fp);

		return NULL;
	}

	fclose(fp);
	*size = (size_t)len;

	return buf;
}

typedef struct {
	char *path;
	size_t index;
} uclv_asset_t;

static uclv_asset_t *assets;
static size_t asset_count;

static bool
uclv_asset_get(const char *path, size_t *index)
{
	size_t i;

	for (i = 0; i < asset_count; i++) {
		if (strcmp(assets[i].path, path))
			continue;

		*index = assets[i].index;

		return true;
	}

	return false;
}

static void
uclv_asset_put(const char *path, size_t index)
{
	uclv_asset_t *grown;
	char *copy = strdup(path);

	if (!copy)
		return;

	grown = realloc(assets, (asset_count + 1) * sizeof(*assets));

	if (!grown) {
		free(copy);

		return;
	}

	assets = grown;
	assets[asset_count].path = copy;
	assets[asset_count].index = index;
	asset_count++;
}

/**
 * uc_lv_font_load() - lv.font_load(path)
 * @path: an LVGL binary font file
 *
 * The bytes go over as memory, not as a path: lv_binfont_create() reads through
 * lv_fs and no driver is registered for one, so LV_USE_FS_MEMFS must stay on. A
 * path already loaded answers the same index.
 *
 * Return: an index for a widget's text_font style, or null.
 */
static uc_value_t *
uc_lv_font_load(uc_vm_t *vm, size_t nargs)
{
	uc_value_t *arg = uc_fn_arg(0);
	const char *path;
	lv_font_t *font;
	uint8_t *blob;
	size_t size, index;

	if (ucv_type(arg) != UC_STRING) {
		uc_vm_raise_exception(vm, EXCEPTION_TYPE,
				      "expecting a font path");
		return NULL;
	}

	path = ucv_string_get(arg);

	if (uclv_asset_get(path, &index))
		return ucv_uint64_new(index);

	blob = file_slurp(path, &size);

	if (!blob) {
		fprintf(stderr, "lv: %s: cannot be read\n", path);

		return NULL;
	}

	font = lv_binfont_create_from_buffer(blob, size);

	free(blob);

	if (!font) {
		fprintf(stderr, "lv: %s: not a usable font\n", path);

		return NULL;
	}

	index = uclv_font_add(font);

	if (index == SIZE_MAX) {
		lv_binfont_destroy(font);

		return NULL;
	}

	uclv_asset_put(path, index);

	return ucv_uint64_new(index);
}

/**
 * uc_lv_image_load() - lv.image_load(path, [alpha])
 * @path: a PNG file
 * @alpha: true for an A8 coverage mask, false or absent for RGB565
 *
 * Decoded once and kept as a descriptor. LV_CACHE_DEF_SIZE is 0, so LVGL frees
 * a decoded image once it has drawn it and decodes the PNG again every frame. A
 * path already loaded answers the same index.
 *
 * Return: an index for w.src(), or null when the file cannot be used.
 */
static uc_value_t *
uc_lv_image_load(uc_vm_t *vm, size_t nargs)
{
	uc_value_t *arg = uc_fn_arg(0);
	bool alpha = ucv_is_truish(uc_fn_arg(1));
	lv_draw_buf_t *decoded = NULL;
	lv_image_dsc_t *dsc;
	uint8_t *png, *bytes;
	unsigned w = 0, h = 0, err;
	uint32_t stride, x, y;
	size_t count, index, png_size, pixel;
	const char *path;

	if (ucv_type(arg) != UC_STRING) {
		uc_vm_raise_exception(vm, EXCEPTION_TYPE,
				      "expecting an image path");
		return NULL;
	}

	path = ucv_string_get(arg);

	if (uclv_asset_get(path, &index))
		return ucv_uint64_new(index);

	png = file_slurp(path, &png_size);

	if (!png) {
		fprintf(stderr, "lv: %s: cannot be read\n", ucv_string_get(arg));

		return NULL;
	}

	/* LVGL patches lodepng to answer an ARGB8888 lv_draw_buf_t rather than
	   a pixel buffer. */
	err = lodepng_decode32((unsigned char **)&decoded, &w, &h, png, png_size);

	free(png);

	if (err || !decoded) {
		if (decoded)
			lv_draw_buf_destroy(decoded);

		fprintf(stderr, "lv: %s: png decode failed: %u %s\n",
			ucv_string_get(arg), err, lodepng_error_text(err));

		return NULL;
	}

	pixel = alpha ? 1 : 2;
	count = (size_t)w * h;
	stride = decoded->header.stride;
	bytes = malloc(count * pixel);
	dsc = calloc(1, sizeof(*dsc));

	if (!count || !bytes || !dsc) {
		lv_draw_buf_destroy(decoded);
		free(bytes);
		free(dsc);

		return NULL;
	}

	for (y = 0; y < h; y++) {
		const uint8_t *row = decoded->data + (size_t)y * stride;

		for (x = 0; x < w; x++) {
			size_t at = (size_t)y * w + x;

			if (alpha) {
				bytes[at] = row[x * 4 + 3];

				continue;
			}

			((uint16_t *)bytes)[at] =
				(uint16_t)(((row[x * 4 + 0] >> 3) << 11) |
					   ((row[x * 4 + 1] >> 2) << 5) |
					    (row[x * 4 + 2] >> 3));
		}
	}

	lv_draw_buf_destroy(decoded);

	dsc->header.magic = LV_IMAGE_HEADER_MAGIC;
	dsc->header.cf = alpha ? LV_COLOR_FORMAT_A8 : LV_COLOR_FORMAT_RGB565;
	dsc->header.w = w;
	dsc->header.h = h;
	dsc->header.stride = w * pixel;
	dsc->data_size = count * pixel;
	dsc->data = bytes;

	index = uclv_image_add(dsc);

	if (index == SIZE_MAX) {
		free(bytes);
		free(dsc);

		return NULL;
	}

	uclv_asset_put(path, index);

	return ucv_uint64_new(index);
}

/**
 * uc_lv_screen() - lv.screen()
 *
 * Return: the active screen, or null before a display exists.
 */
static uc_value_t *
uc_lv_screen(uc_vm_t *vm, size_t nargs)
{
	if (!lv_ready) {
		uc_vm_raise_exception(vm, EXCEPTION_RUNTIME,
				      "lv.init() has not been called");
		return NULL;
	}

	return uclv_widget_wrap(vm, lv_screen_active());
}

/**
 * uc_lv_screen_create() - lv.screen_create()
 *
 * Return: a screen with no parent, to be shown with lv.screen_load().
 */
static uc_value_t *
uc_lv_screen_create(uc_vm_t *vm, size_t nargs)
{
	if (!lv_ready) {
		uc_vm_raise_exception(vm, EXCEPTION_RUNTIME,
				      "lv.init() has not been called");
		return NULL;
	}

	return uclv_widget_wrap(vm, lv_obj_create(NULL));
}

/**
 * uc_lv_screen_load() - lv.screen_load(screen)
 * @screen: a screen from lv.screen_create()
 *
 * Return: false when the argument is not a parentless widget.
 */
static uc_value_t *
uc_lv_screen_load(uc_vm_t *vm, size_t nargs)
{
	uclv_widget_t *w = ucv_resource_data(uc_fn_arg(0), UCLV_WIDGET_TYPE);

	if (!w || !w->obj) {
		uc_vm_raise_exception(vm, EXCEPTION_TYPE,
				      "expecting a live screen widget");
		return NULL;
	}

	if (lv_obj_get_parent(w->obj)) {
		uc_vm_raise_exception(vm, EXCEPTION_TYPE,
				      "expecting a screen, not a child object");
		return NULL;
	}

	lv_screen_load(w->obj);

	return ucv_boolean_new(true);
}

/**
 * uc_lv_timer_handler() - lv.timer_handler()
 *
 * Runs LVGL's timers, the input read and the redraw. Call from a uloop timer.
 *
 * Return: milliseconds until the next call is due, or null when a handler
 * raised an exception.
 */
static uc_value_t *
uc_lv_timer_handler(uc_vm_t *vm, size_t nargs)
{
	uint32_t delay;

	if (!lv_ready)
		return ucv_int64_new(1000);

	delay = lv_timer_handler();

	if (vm->exception.type != EXCEPTION_NONE)
		return NULL;

	return ucv_int64_new(delay);
}

/**
 * uc_lv_drm_drop_master() - lv.drm_drop_master()
 *
 * Hands DRM master to the next process to open the device, without closing it.
 * The framebuffer stays on the plane, so the panel keeps the last frame until
 * the new master commits its own. A close destroys a framebuffer that is in
 * use, and the kernel then disables the CRTC.
 *
 * Return: whether there was a display to drop.
 */
static uc_value_t *
uc_lv_drm_drop_master(uc_vm_t *vm, size_t nargs)
{
	int fd;

	if (!display)
		return ucv_boolean_new(false);

	fd = lv_linux_drm_get_fd(display);

	if (fd < 0)
		return ucv_boolean_new(false);

	return ucv_boolean_new(drmDropMaster(fd) == 0);
}

/**
 * uc_lv_refresh() - lv.refresh()
 *
 * Draws every pending invalidation and waits for the panel to take the frame,
 * so a caller can raise the backlight or hand over the display with the frame
 * on the glass.
 *
 * lv_refr_now() returns once the flush callback has queued the commit, while
 * the CRTC still scans the previous buffer. LVGL waits at the top of its next
 * flush, which is too late, so the wait happens here through
 * lv_display_private.h.
 *
 * Return: true, or false when no display is open.
 */
static uc_value_t *
uc_lv_refresh(uc_vm_t *vm, size_t nargs)
{
	if (!display)
		return ucv_boolean_new(false);

	lv_refr_now(display);

	if (display->flushing && display->flush_wait_cb) {
		display->flush_wait_cb(display);
		display->flushing = 0;
	}

	return ucv_boolean_new(true);
}

/**
 * uc_lv_screenshot() - lv.screenshot(path)
 * @path: where to write a PNG of what is on the panel
 *
 * Re-renders the tree into its own buffer rather than reading the DRM
 * framebuffer back, so it cannot catch a half drawn frame.
 *
 * Return: whether the file was written.
 */
static uc_value_t *
uc_lv_screenshot(uc_vm_t *vm, size_t nargs)
{
	uc_value_t *arg = uc_fn_arg(0);
	lv_draw_buf_t *snap;
	uint8_t *rgb, *png = NULL;
	size_t size = 0, x, y;
	uint32_t w, h, stride;
	unsigned err;
	FILE *fp;

	if (!lv_ready) {
		uc_vm_raise_exception(vm, EXCEPTION_RUNTIME,
				      "lv.init() has not been called");
		return NULL;
	}

	if (ucv_type(arg) != UC_STRING) {
		uc_vm_raise_exception(vm, EXCEPTION_TYPE,
				      "expecting an output path");
		return NULL;
	}

	snap = lv_snapshot_take(lv_screen_active(), LV_COLOR_FORMAT_RGB888);

	if (!snap)
		return ucv_boolean_new(false);

	w = snap->header.w;
	h = snap->header.h;
	stride = snap->header.stride;
	rgb = malloc((size_t)w * h * 3);

	if (!rgb) {
		lv_draw_buf_destroy(snap);

		return ucv_boolean_new(false);
	}

	for (y = 0; y < h; y++) {
		const uint8_t *src = snap->data + y * stride;
		uint8_t *dst = rgb + y * w * 3;

		for (x = 0; x < w; x++) {
			dst[x * 3 + 0] = src[x * 3 + 2];
			dst[x * 3 + 1] = src[x * 3 + 1];
			dst[x * 3 + 2] = src[x * 3 + 0];
		}
	}

	err = lodepng_encode24(&png, &size, rgb, w, h);

	free(rgb);
	lv_draw_buf_destroy(snap);

	if (err) {
		free(png);

		uc_vm_raise_exception(vm, EXCEPTION_RUNTIME,
				      "png encode failed: %u", err);
		return NULL;
	}

	fp = fopen(ucv_string_get(arg), "wb");

	if (fp) {
		if (fwrite(png, 1, size, fp) != size)
			err = 1;

		fclose(fp);
	}

	free(png);

	return ucv_boolean_new(fp != NULL && !err);
}

/**
 * uc_lv_text_width() - lv.text_width(font, text, [tracking])
 * @font: an index from lv.font_load()
 * @text: the string, measured in bytes so a multibyte name measures as drawn
 * @tracking: letter spacing, default 0
 *
 * Return: the rendered width in pixels.
 */
static uc_value_t *
uc_lv_text_width(uc_vm_t *vm, size_t nargs)
{
	const lv_font_t *font = uclv_font((size_t)ucv_to_integer(uc_fn_arg(0)));
	uc_value_t *text = uc_fn_arg(1);
	uc_value_t *space = uc_fn_arg(2);

	if (!font) {
		uc_vm_raise_exception(vm, EXCEPTION_RUNTIME,
				      "no such font");
		return NULL;
	}

	if (ucv_type(text) != UC_STRING) {
		uc_vm_raise_exception(vm, EXCEPTION_TYPE,
				      "expecting a string to measure");
		return NULL;
	}

	return ucv_int64_new(lv_text_get_width(ucv_string_get(text),
					       ucv_string_length(text), font,
					       space ? (int32_t)ucv_to_integer(space)
						     : 0));
}

/**
 * uc_lv_font_line_height() - lv.font_line_height(font)
 * @font: an index from lv.font_load()
 *
 * Read from the face the panel loaded rather than from a table beside it, so it
 * cannot drift from the bytes on the target. Line height is not 1.2 times the
 * size: a digit only face has no ascenders, so a 27 px face is 21 tall.
 *
 * Return: the line height in pixels.
 */
static uc_value_t *
uc_lv_font_line_height(uc_vm_t *vm, size_t nargs)
{
	const lv_font_t *font = uclv_font((size_t)ucv_to_integer(uc_fn_arg(0)));

	if (!font) {
		uc_vm_raise_exception(vm, EXCEPTION_RUNTIME, "no such font");
		return NULL;
	}

	return ucv_int64_new(lv_font_get_line_height(font));
}

/**
 * uc_lv_pct() - lv.pct(percent)
 * @percent: -1000 to 1000, where a negative one measures back from the far edge
 *
 * A size or a position as a share of the parent, for w.set() and w.style().
 * LVGL encodes one as a number inside a reserved range rather than as a flag,
 * so it cannot be worked out in ucode.
 *
 * Return: the encoded coordinate.
 */
static uc_value_t *
uc_lv_pct(uc_vm_t *vm, size_t nargs)
{
	/* LV_PCT() clamps through LV_MIN and LV_MAX, so it reads its argument
	   several times. */
	int32_t percent = (int32_t)ucv_to_integer(uc_fn_arg(0));

	return ucv_int64_new(LV_PCT(percent));
}

/**
 * uc_lv_grid_fr() - lv.grid_fr(share)
 * @share: 0 to 255, a share of whatever the fixed tracks leave over
 *
 * A track size for w.grid_dsc().
 *
 * Return: the encoded track size.
 */
static uc_value_t *
uc_lv_grid_fr(uc_vm_t *vm, size_t nargs)
{
	return ucv_int64_new(lv_grid_fr((uint8_t)ucv_to_unsigned(uc_fn_arg(0))));
}

/**
 * UCLV_WIDGET_CTOR() - define lv.<name>(parent), which builds a widget
 * @name: the ucode name
 * @ctor: the LVGL constructor behind it
 */
#define UCLV_WIDGET_CTOR(name, ctor)					\
	static uc_value_t *						\
	uc_lv_##name(uc_vm_t *vm, size_t nargs)				\
	{								\
		lv_obj_t *parent = uclv_parent(vm, uc_fn_arg(0));	\
									\
		if (!parent)						\
			return NULL;					\
									\
		return uclv_widget_wrap(vm, ctor(parent));		\
	}

UCLV_WIDGET_CTOR(obj,      lv_obj_create)
UCLV_WIDGET_CTOR(label,    lv_label_create)
UCLV_WIDGET_CTOR(bar,      lv_bar_create)
UCLV_WIDGET_CTOR(chart,    lv_chart_create)
UCLV_WIDGET_CTOR(arc,      lv_arc_create)
UCLV_WIDGET_CTOR(line,     lv_line_create)
UCLV_WIDGET_CTOR(tileview, lv_tileview_create)
UCLV_WIDGET_CTOR(image,    lv_image_create)
UCLV_WIDGET_CTOR(qrcode,   lv_qrcode_create)

static const uc_function_list_t global_fns[] = {
	{ "init",		uc_lv_init },
	{ "display_drm",	uc_lv_display_drm },
	{ "indev_evdev",	uc_lv_indev_evdev },
	{ "touch_calibrate",	uc_lv_touch_calibrate },
	{ "touch_swap_axes",	uc_lv_touch_swap_axes },
	{ "touch_point",	uc_lv_touch_point },
	{ "touch_drop",		uc_lv_touch_drop },
	{ "gesture_dir",	uc_lv_gesture_dir },
	{ "screen",		uc_lv_screen },
	{ "screen_create",	uc_lv_screen_create },
	{ "screen_load",	uc_lv_screen_load },
	{ "timer_handler",	uc_lv_timer_handler },
	{ "refresh",		uc_lv_refresh },
	{ "drm_drop_master",	uc_lv_drm_drop_master },
	{ "text_width",		uc_lv_text_width },
	{ "font_line_height",	uc_lv_font_line_height },
	{ "pct",		uc_lv_pct },
	{ "grid_fr",		uc_lv_grid_fr },
	{ "screenshot",		uc_lv_screenshot },
	{ "image_load",		uc_lv_image_load },
	{ "font_load",		uc_lv_font_load },
	{ "obj",		uc_lv_obj },
	{ "label",		uc_lv_label },
	{ "bar",		uc_lv_bar },
	{ "chart",		uc_lv_chart },
	{ "arc",		uc_lv_arc },
	{ "line",		uc_lv_line },
	{ "tileview",		uc_lv_tileview },
	{ "image",		uc_lv_image },
	{ "qrcode",		uc_lv_qrcode },
};

static const struct {
	const char *name;
	int64_t value;
} constants[] = {

	{ "ALIGN_TOP_LEFT",		LV_ALIGN_TOP_LEFT },
	{ "ALIGN_TOP_MID",		LV_ALIGN_TOP_MID },
	{ "ALIGN_TOP_RIGHT",		LV_ALIGN_TOP_RIGHT },
	{ "ALIGN_LEFT_MID",		LV_ALIGN_LEFT_MID },
	{ "ALIGN_CENTER",		LV_ALIGN_CENTER },
	{ "ALIGN_RIGHT_MID",		LV_ALIGN_RIGHT_MID },
	{ "ALIGN_BOTTOM_LEFT",		LV_ALIGN_BOTTOM_LEFT },
	{ "ALIGN_BOTTOM_MID",		LV_ALIGN_BOTTOM_MID },
	{ "ALIGN_BOTTOM_RIGHT",		LV_ALIGN_BOTTOM_RIGHT },
	{ "ALIGN_OUT_RIGHT_BOTTOM",	LV_ALIGN_OUT_RIGHT_BOTTOM },
	{ "ALIGN_OUT_RIGHT_MID",	LV_ALIGN_OUT_RIGHT_MID },
	{ "ALIGN_OUT_BOTTOM_MID",	LV_ALIGN_OUT_BOTTOM_MID },

	{ "EVENT_PRESSED",		LV_EVENT_PRESSED },
	{ "EVENT_CLICKED",		LV_EVENT_CLICKED },
	{ "EVENT_RELEASED",		LV_EVENT_RELEASED },
	{ "EVENT_LONG_PRESSED",		LV_EVENT_LONG_PRESSED },
	{ "EVENT_PRESSING",		LV_EVENT_PRESSING },
	{ "EVENT_GESTURE",		LV_EVENT_GESTURE },
	{ "EVENT_VALUE_CHANGED",	LV_EVENT_VALUE_CHANGED },

	{ "STATE_DEFAULT",		LV_STATE_DEFAULT },
	{ "STATE_PRESSED",		LV_STATE_PRESSED },
	{ "STATE_CHECKED",		LV_STATE_CHECKED },
	{ "STATE_DISABLED",		LV_STATE_DISABLED },

	{ "PART_MAIN",			LV_PART_MAIN },
	{ "PART_INDICATOR",		LV_PART_INDICATOR },
	{ "PART_KNOB",			LV_PART_KNOB },
	{ "PART_ITEMS",			LV_PART_ITEMS },
	{ "PART_ANY",			LV_PART_ANY },

	{ "DIR_NONE",			LV_DIR_NONE },
	{ "DIR_LEFT",			LV_DIR_LEFT },
	{ "DIR_RIGHT",			LV_DIR_RIGHT },
	{ "DIR_TOP",			LV_DIR_TOP },
	{ "DIR_BOTTOM",			LV_DIR_BOTTOM },
	{ "DIR_HOR",			LV_DIR_HOR },
	{ "DIR_VER",			LV_DIR_VER },

	{ "CHART_TYPE_LINE",		LV_CHART_TYPE_LINE },
	{ "CHART_TYPE_BAR",		LV_CHART_TYPE_BAR },
	{ "CHART_UPDATE_SHIFT",		LV_CHART_UPDATE_MODE_SHIFT },
	{ "CHART_UPDATE_CIRCULAR",	LV_CHART_UPDATE_MODE_CIRCULAR },

	{ "GRAD_DIR_NONE",		LV_GRAD_DIR_NONE },
	{ "GRAD_DIR_VER",		LV_GRAD_DIR_VER },
	{ "GRAD_DIR_HOR",		LV_GRAD_DIR_HOR },

	{ "SCROLLBAR_OFF",		LV_SCROLLBAR_MODE_OFF },
	{ "SCROLLBAR_AUTO",		LV_SCROLLBAR_MODE_AUTO },

	{ "TEXT_ALIGN_LEFT",		LV_TEXT_ALIGN_LEFT },
	{ "TEXT_ALIGN_CENTER",		LV_TEXT_ALIGN_CENTER },
	{ "TEXT_ALIGN_RIGHT",		LV_TEXT_ALIGN_RIGHT },

	{ "LABEL_LONG_WRAP",		LV_LABEL_LONG_MODE_WRAP },
	{ "LABEL_LONG_DOTS",		LV_LABEL_LONG_MODE_DOTS },
	{ "LABEL_LONG_SCROLL",		LV_LABEL_LONG_MODE_SCROLL },
	{ "LABEL_LONG_SCROLL_CIRCULAR",	LV_LABEL_LONG_MODE_SCROLL_CIRCULAR },
	{ "LABEL_LONG_CLIP",		LV_LABEL_LONG_MODE_CLIP },

	{ "OPA_TRANSP",			LV_OPA_TRANSP },
	{ "OPA_20",			LV_OPA_20 },
	{ "OPA_30",			LV_OPA_30 },
	{ "OPA_50",			LV_OPA_50 },
	{ "OPA_COVER",			LV_OPA_COVER },

	{ "SIZE_CONTENT",		LV_SIZE_CONTENT },

	{ "FLEX_FLOW_ROW",		LV_FLEX_FLOW_ROW },
	{ "FLEX_FLOW_ROW_WRAP",		LV_FLEX_FLOW_ROW_WRAP },
	{ "FLEX_FLOW_ROW_REVERSE",	LV_FLEX_FLOW_ROW_REVERSE },
	{ "FLEX_FLOW_ROW_WRAP_REVERSE",	LV_FLEX_FLOW_ROW_WRAP_REVERSE },
	{ "FLEX_FLOW_COLUMN",		LV_FLEX_FLOW_COLUMN },
	{ "FLEX_FLOW_COLUMN_WRAP",	LV_FLEX_FLOW_COLUMN_WRAP },
	{ "FLEX_FLOW_COLUMN_REVERSE",	LV_FLEX_FLOW_COLUMN_REVERSE },
	{ "FLEX_FLOW_COLUMN_WRAP_REVERSE", LV_FLEX_FLOW_COLUMN_WRAP_REVERSE },

	{ "FLEX_ALIGN_START",		LV_FLEX_ALIGN_START },
	{ "FLEX_ALIGN_END",		LV_FLEX_ALIGN_END },
	{ "FLEX_ALIGN_CENTER",		LV_FLEX_ALIGN_CENTER },
	{ "FLEX_ALIGN_SPACE_EVENLY",	LV_FLEX_ALIGN_SPACE_EVENLY },
	{ "FLEX_ALIGN_SPACE_AROUND",	LV_FLEX_ALIGN_SPACE_AROUND },
	{ "FLEX_ALIGN_SPACE_BETWEEN",	LV_FLEX_ALIGN_SPACE_BETWEEN },

	{ "GRID_ALIGN_START",		LV_GRID_ALIGN_START },
	{ "GRID_ALIGN_CENTER",		LV_GRID_ALIGN_CENTER },
	{ "GRID_ALIGN_END",		LV_GRID_ALIGN_END },
	{ "GRID_ALIGN_STRETCH",		LV_GRID_ALIGN_STRETCH },
	{ "GRID_ALIGN_SPACE_EVENLY",	LV_GRID_ALIGN_SPACE_EVENLY },
	{ "GRID_ALIGN_SPACE_AROUND",	LV_GRID_ALIGN_SPACE_AROUND },
	{ "GRID_ALIGN_SPACE_BETWEEN",	LV_GRID_ALIGN_SPACE_BETWEEN },

	{ "GRID_CONTENT",		LV_GRID_CONTENT },
};

void
uc_module_init(uc_vm_t *vm, uc_value_t *scope)
{
	size_t i;

	uc_function_list_register(scope, global_fns);
	uclv_widget_register(vm);

	for (i = 0; i < ARRAY_SIZE(constants); i++)
		ucv_object_add(scope, constants[i].name,
			       ucv_int64_new(constants[i].value));
}
