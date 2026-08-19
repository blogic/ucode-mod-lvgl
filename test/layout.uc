/*
 * SPDX-License-Identifier: GPL-2.0-only
 *
 * Copyright (C) 2026 John Crispin <john@phrozen.org>
 */

'use strict';

/*
 * Layout smoke test for the lv module.
 *
 * Exercises flex, grid, the geometry read-back and both grid teardown paths.
 * Takes DRM master, so stop whatever owns the panel first:
 *
 *	/etc/init.d/glinet-panel-ui stop
 *	ucode /tmp/layout.uc
 *	/etc/init.d/glinet-panel-ui start
 *
 * Leaves flex.png, grid.png, grid-redsc.png and orphan.png in /tmp.
 */

import * as lv from 'lv';

const W = 320;
const H = 240;

const BG = 0x101010;
const COLOURS = [ 0xe53935, 0x1e88e5, 0x43a047, 0xfb8c00, 0x8e24aa, 0x00acc1 ];

let failures = 0;
let checks = 0;

function check(what, got, want) {
	let ok = (got == want);

	checks++;

	if (!ok)
		failures++;

	printf('%-38s %-10s %s\n', what, got, ok ? 'ok' : sprintf('want %s', want));
}

function raises(what, fn) {
	checks++;

	try {
		fn();
	} catch (e) {
		printf('%-38s %s\n', what, 'raises, ok');

		return;
	}

	failures++;
	printf('%-38s %s\n', what, 'DID NOT RAISE');
}

function screen_new() {
	let screen = lv.screen_create();

	screen.style({ bg_color: BG, bg_opa: lv.OPA_COVER, pad_all: 0,
		       pad_row: 0, pad_column: 0, border_width: 0, radius: 0 });
	screen.scrollable(false);

	return screen;
}

function box_new(parent, colour, w, h) {
	let box = lv.obj(parent);

	/* pad_all does not cover pad_row and pad_column, and the default theme
	   sets both. Flex and grid read them as the gap, so a container that
	   leaves them alone gets a 12 px gap it never asked for. */
	box.style({ bg_color: colour, bg_opa: lv.OPA_COVER, border_width: 0,
		    radius: 0, pad_all: 0, pad_row: 0, pad_column: 0 });
	box.scrollable(false);
	box.clickable(false);

	if (w != null)
		box.set({ w, h });

	return box;
}

if (!lv.init())
	die('cannot initialise LVGL');

if (!lv.display_drm(getenv('PANEL_DRM_DEVICE') ?? '/dev/dri/card0', -1))
	die('cannot open the DRM display');

printf('\n== encodings ==\n');

check('SIZE_CONTENT', lv.SIZE_CONTENT, 1073741823);
check('pct(100)', lv.pct(100), 536871012);
check('pct(50)', lv.pct(50), 536870962);
check('grid_fr(1)', lv.grid_fr(1), 536870812);
check('GRID_CONTENT', lv.GRID_CONTENT, 536870810);

printf('\n== font metrics ==\n');

let face = lv.font_load('/usr/share/glinet-panel-ui/fonts/inter_regular_15.bin');

check('inter_regular_15 loaded', face != null, true);
check('inter_regular_15 line height', lv.font_line_height(face), 19);

let thin = lv.font_load('/usr/share/glinet-panel-ui/fonts/inter_thin_112.bin');

/* A digit only face has no ascenders, so its line height is nowhere near 1.2
   times the pixel size. */
check('inter_thin_112 line height', lv.font_line_height(thin), 85);

raises('font_line_height, no such font', function() {
	lv.font_line_height(9999);
});

printf('\n== flex ==\n');

let flex = screen_new();

flex.flex_flow(lv.FLEX_FLOW_COLUMN);
flex.flex_align(lv.FLEX_ALIGN_START, lv.FLEX_ALIGN_CENTER, lv.FLEX_ALIGN_START);

let head = box_new(flex, COLOURS[0], lv.pct(100), 40);
let body = box_new(flex, COLOURS[1], lv.pct(100), 10);
let foot = box_new(flex, COLOURS[2], lv.pct(100), 30);

body.flex_grow(1);

let row = box_new(body, COLOURS[3], lv.pct(100), lv.pct(100));

row.flex_flow(lv.FLEX_FLOW_ROW_WRAP);
row.flex_align(lv.FLEX_ALIGN_SPACE_BETWEEN, lv.FLEX_ALIGN_CENTER,
	       lv.FLEX_ALIGN_SPACE_EVENLY);
row.style({ pad_all: 6, pad_column: 6, pad_row: 6 });

for (let i = 0; i < 6; i++)
	box_new(row, COLOURS[i], 60, 40);

lv.screen_load(flex);
lv.refresh();
lv.screenshot('/tmp/flex.png');

check('flex head width', head.width(), W);
check('flex head x', head.x(), 0);
check('flex head y', head.y(), 0);
check('flex foot y', foot.y(), H - 30);
check('flex grow fills the gap', body.height(), H - 40 - 30);
check('flex pct(100) on a child', row.width(), W);
check('flex content width', row.content_width(), W - 12);

let area = row.coords();

check('coords x1', area.x1, 0);
check('coords w', area.w, W);
check('coords h', area.h, row.height());
check('coords y2 - y1 + 1', area.y2 - area.y1 + 1, row.height());

printf('\n== SIZE_CONTENT ==\n');

let label = lv.label(flex);

label.text('Temp');
label.style({ text_font: face, text_color: 0xffffff, pad_all: 0,
	      border_width: 0 });
label.set({ w: lv.SIZE_CONTENT, h: lv.SIZE_CONTENT });

flex.update_layout();

/* The page froze this measurement as 42 with a comment saying the real width
   was 40.6. A content sized label is the measurement itself. */
check('content label width', label.width(), lv.text_width(face, 'Temp', 0));
check('content label height', label.height(), lv.font_line_height(face));

printf('\n== flex guards ==\n');

raises('flex_grow on a screen', function() { flex.flex_grow(1); });
raises('unknown layout name', function() { flex.layout('masonry'); });

printf('\n== grid, fixed tracks ==\n');

let grid = screen_new();

grid.grid_dsc([ 100, 100, 100 ], [ 50, 50 ]);
grid.grid_align(lv.GRID_ALIGN_START, lv.GRID_ALIGN_START);

let cells = [];

for (let r = 0; r < 2; r++)
	for (let c = 0; c < 3; c++) {
		let cell = box_new(grid, COLOURS[r * 3 + c]);

		cell.grid_cell(c, r);

		push(cells, cell);
	}

/*
 * A child that was never given a cell lands at 0,0 and stays there. The grid
 * drops it because its span defaults to zero, and lv_obj_refr_pos() refuses it
 * because its parent carries a layout, so neither placer touches it. This is
 * why grid_cell() defaults both spans to 1: the trap is only reachable by never
 * calling it at all.
 */
let orphan_cell = box_new(grid, 0xffffff, 20, 20);

orphan_cell.set({ x: 7, y: 9 });

lv.screen_load(grid);
lv.refresh();
lv.screenshot('/tmp/grid.png');

check('grid cell 0 width', cells[0].width(), 100);
check('grid cell 0 height', cells[0].height(), 50);
check('grid cell 4 x', cells[4].x(), 100);
check('grid cell 4 y', cells[4].y(), 50);
check('grid child with no cell sits at x 0', orphan_cell.x(), 0);
check('grid child with no cell sits at y 0', orphan_cell.y(), 0);

raises('grid_cell on a screen', function() { grid.grid_cell(0, 0); });
raises('grid_dsc with no array', function() { grid.grid_dsc(3, 2); });
raises('grid_dsc with an empty array', function() { grid.grid_dsc([], [ 1 ]); });

printf('\n== grid, second descriptor on the same widget ==\n');

/* The first pair must be freed only after the second is installed. Watch this
   one under valgrind. */
grid.grid_dsc([ lv.grid_fr(1), lv.grid_fr(2), lv.GRID_CONTENT ],
	      [ 60, lv.grid_fr(1) ]);

cells[0].grid_cell(0, 0, { span_x: 2 });
cells[1].set({ w: 40, h: 40 });
cells[1].grid_cell(2, 0, { span_y: 2, align_x: lv.GRID_ALIGN_CENTER,
			   align_y: lv.GRID_ALIGN_CENTER });
cells[2].set({ w: 30, h: 30 });
cells[2].grid_cell(0, 1, { align_x: lv.GRID_ALIGN_START,
			   align_y: lv.GRID_ALIGN_END });
cells[3].grid_cell(1, 1);
cells[4].hidden(true);
cells[5].hidden(true);

grid.update_layout();
lv.refresh();
lv.screenshot('/tmp/grid-redsc.png');

check('re-dsc row 0 height', cells[0].height(), 60);
check('re-dsc fr(2) is wider than fr(1)', cells[3].width() > cells[2].x() + 30,
      true);

/*
 * A cell that was stretched keeps the size the stretch gave it, even after it
 * is re-aligned and given an explicit one. lv_grid.c takes item_w from the
 * item's current coords for every alignment except STRETCH, and
 * lv_obj_refr_size() refuses to apply the new style width while both layout
 * bits are still set from the stretch. So the old size is carried forward.
 *
 * Asserted rather than fixed: it is LVGL's behaviour, and the way out is to
 * build a cell with the alignment it is going to keep.
 */
check('a stretched cell keeps its size', cells[1].width(), 100);
check('and its height', cells[1].height(), 50);

printf('\n== does a non-stretch cell keep an explicit size ==\n');

/*
 * Two children of one grid, both 40x40 and both centred in a 100x100 cell. The
 * only difference is that the second one was laid out as a STRETCH cell first.
 * Grid writes a child's size only for STRETCH, so if the two disagree it is the
 * earlier stretch that stuck, not the alignment that failed.
 */
let probe = screen_new();

probe.grid_dsc([ 100, 100 ], [ 100, 100 ]);

let fresh = box_new(probe, COLOURS[0], 40, 40);

fresh.grid_cell(0, 0, { align_x: lv.GRID_ALIGN_CENTER,
			align_y: lv.GRID_ALIGN_CENTER });

let stretched = box_new(probe, COLOURS[1]);

stretched.grid_cell(1, 0);

lv.screen_load(probe);
probe.update_layout();

check('stretch cell filled its cell', stretched.width(), 100);

stretched.set({ w: 40, h: 40 });
stretched.grid_cell(1, 0, { align_x: lv.GRID_ALIGN_CENTER,
			    align_y: lv.GRID_ALIGN_CENTER });
probe.update_layout();

check('never stretched, keeps w', fresh.width(), 40);
check('never stretched, keeps h', fresh.height(), 40);
check('never stretched, centred x', fresh.x(), 30);
/* The stretch sticks. See the note above. */
check('was stretched, ignores the resize', stretched.width(), 100);

printf('\n== layout by name ==\n');

grid.layout('none');
grid.update_layout();
cells[3].set({ x: 11, y: 13 });
grid.update_layout();

check('layout none frees x', cells[3].x(), 11);
check('layout none frees y', cells[3].y(), 13);

grid.layout('grid');
grid.update_layout();

check('layout grid takes over again', cells[3].x() != 11, true);

printf('\n== the arrays outliving their resource ==\n');

/*
 * Drop every ucode reference to a grid container that is still on screen, then
 * keep drawing. LVGL re-reads the two track pointers on every layout pass, so
 * this only holds if the object owns them rather than the resource.
 */
let orphan_screen = screen_new();
let orphan = box_new(orphan_screen, 0x202020, W, H);

orphan.grid_dsc([ lv.grid_fr(1), lv.grid_fr(1) ],
		[ lv.grid_fr(1), lv.grid_fr(1) ]);

let witness = [];

for (let i = 0; i < 4; i++) {
	let cell = box_new(orphan, COLOURS[i]);

	cell.grid_cell(i % 2, int(i / 2));

	push(witness, cell);
}

lv.screen_load(orphan_screen);
lv.refresh();

orphan = null;
gc();

witness[0].set({ w: lv.pct(100) });
lv.refresh();
lv.screenshot('/tmp/orphan.png');

check('orphaned grid still lays out x', witness[3].x(), int(W / 2));
check('orphaned grid still lays out y', witness[3].y(), int(H / 2));
check('orphaned grid cell height', witness[3].height(), int(H / 2));

printf('\n== teardown ==\n');

lv.screen_load(flex);
lv.refresh();

grid.delete();
orphan_screen.delete();
gc();

lv.refresh();

check('survived both delete paths', true, true);

printf('\n%s: %d of %d checks failed\n', failures ? 'FAIL' : 'PASS', failures,
       checks);

/* Hand the panel back rather than exiting on top of a live framebuffer. */
lv.drm_drop_master();

exit(failures ? 1 : 0);
