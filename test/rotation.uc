/*
 * SPDX-License-Identifier: GPL-2.0-only
 */

'use strict';

/*
 * Display rotation on the DRM panel.
 *
 * For every rotation, loads a screen with a red square in the logical top
 * left corner and checks that the logical resolution follows: 90 and 270
 * swap width and height. Where the square lands on the glass is for the eye.
 * Takes DRM master, so stop whatever owns the panel first:
 *
 *	/etc/init.d/glinet-panel-ui stop
 *	ucode /tmp/rotation.uc
 *	/etc/init.d/glinet-panel-ui start
 */

import * as lv from 'lv';

const RED = 0xff0000;
const BLACK = 0x000000;
const BOX = 20;

let failures = 0;
let checks = 0;

function check(what, got, want) {
	let ok = (sprintf('%J', got) == sprintf('%J', want));

	checks++;

	if (!ok)
		failures++;

	printf('%-44s %-12J %s\n', what, got,
	       ok ? 'ok' : sprintf('want %J', want));
}

function raises(what, fn) {
	checks++;

	try {
		fn();
	} catch (e) {
		printf('%-44s %-12s ok\n', what, 'raises');

		return;
	}

	failures++;
	printf('%-44s DID NOT RAISE\n', what);
}

if (!lv.init())
	die('cannot initialise LVGL');

if (!lv.display_drm(getenv('PANEL_DRM_DEVICE') ?? '/dev/dri/card0', -1))
	die('cannot open the DRM display');

check('starts unrotated', lv.display_rotation(), 0);
raises('refuses 45 degrees', () => lv.display_rotation(45));

const PW = 320, PH = 240;

for (let deg in [ 0, 90, 180, 270, 0 ]) {
	printf('\n== %d degrees ==\n', deg);

	check('rotation is set', lv.display_rotation(deg), deg);

	let screen = lv.screen_create();

	screen.style({ bg_color: BLACK, bg_opa: lv.OPA_COVER, border_width: 0,
		       pad_all: 0, radius: 0 });
	screen.scrollable(false);

	let box = lv.obj(screen);

	box.style({ bg_color: RED, bg_opa: lv.OPA_COVER, border_width: 0,
		    radius: 0, pad_all: 0 });
	box.set({ x: 0, y: 0, w: BOX, h: BOX });

	lv.screen_load(screen);
	lv.refresh();

	let portrait = (deg == 90 || deg == 270);

	check('logical width', screen.width(), portrait ? PH : PW);
	check('logical height', screen.height(), portrait ? PW : PH);
}

printf('\n%s: %d of %d checks failed\n', failures ? 'FAIL' : 'PASS', failures,
       checks);

lv.drm_drop_master();

exit(failures ? 1 : 0);
