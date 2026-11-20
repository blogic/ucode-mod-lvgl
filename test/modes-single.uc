/*
 * SPDX-License-Identifier: GPL-2.0-only
 */

'use strict';

/*
 * The single-mode cases of the page framework, one per run because ui_build
 * keeps its screens in module state:
 *
 *	ucode /tmp/modes-single.uc nopin	no PIN: everything, as before
 *	ucode /tmp/modes-single.uc allprivate	PIN, no public root: keypad first
 *
 * Takes DRM master, like modes.uc.
 */

import * as lv from 'lv';
import { ui_build, view_status, view_wake, view_public } from 'lvgl.page';

let failures = 0;
let checks = 0;

function check(what, got, want) {
	let ok = (sprintf('%J', got) == sprintf('%J', want));

	checks++;

	if (!ok)
		failures++;

	printf('%-44s %-24J %s\n', what, got,
	       ok ? 'ok' : sprintf('want %J', want));
}

function stub(name, visibility) {
	let page = { name, needs: [], ctx: null };

	if (visibility)
		page.visibility = visibility;

	page.build = function(parent, ctx) {
		page.ctx = ctx;
	};

	return page;
}

const LOCK = { name: 'lock', needs: [], build: function() {} };
const PIN = { name: 'pin', needs: [], build: function() {} };

if (!lv.init())
	die('cannot initialise LVGL');

if (!lv.display_drm(getenv('PANEL_DRM_DEVICE') ?? '/dev/dri/card0', -1))
	die('cannot open the DRM display');

let which = ARGV[0];

if (which == 'nopin') {
	printf('== no PIN ==\n');

	let a = stub('traffic'), b = stub('qrcodes', 'public');

	ui_build([ a, b ], LOCK, PIN, null, null, true, true, { public: [] });
	view_wake();

	check('every root is shown', view_status().pages, [ 'traffic', 'qrcodes' ]);
	check('wake goes straight to the roots', view_public(), true);
	check('there is no private mode', view_status().private, false);
	check('every item is visible', a.ctx.visible('ssid', 'home'), true);
}
else if (which == 'allprivate') {
	printf('== PIN, no public root ==\n');

	let a = stub('traffic'), b = stub('system');

	ui_build([ a, b ], LOCK, PIN, '123456', null, true, true, { public: [] });

	check('no public root: keypad on wake', view_public(), false);
	check('nothing is shown before the PIN', view_status().pages, []);
}
else {
	die('usage: modes-single.uc nopin|allprivate');
}

printf('\n%s: %d of %d checks failed\n', failures ? 'FAIL' : 'PASS', failures,
       checks);

lv.drm_drop_master();

exit(failures ? 1 : 0);
