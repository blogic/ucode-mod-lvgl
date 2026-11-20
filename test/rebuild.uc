/*
 * SPDX-License-Identifier: GPL-2.0-only
 */

'use strict';

/*
 * A mode change must not hand a page the widgets of its previous build.
 *
 * The pages here come from files, like real ones, and keep a widget in
 * module state the way qrcodes.uc keeps its pager. Calling build() on the
 * same instance again would leave that widget pointing at what the rebuild
 * deleted, and the next update raises "widget has already been deleted".
 * Takes DRM master, like modes.uc:
 *
 *	ucode /tmp/rebuild.uc
 */

import * as lv from 'lv';
import { mkdir, writefile, unlink, rmdir } from 'fs';
import { loader_init, page_load } from 'lvgl.loader';
import { ui_build, nav_flush, view_status, view_wake, view_pin,
	 view_logout, view_lock } from 'lvgl.page';

const DIR = '/tmp/lvgl-rebuild-test';

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

function survives(what, fn) {
	checks++;

	try {
		fn();
		printf('%-44s %-24s ok\n', what, 'no error');
	} catch (e) {
		failures++;
		printf('%-44s %s\n', what, e);
	}
}

/* Keeps its box across calls, and touches it on every enter. */
const STATEFUL = `
import * as lv from 'lv';

let box;

return {
	visibility: '%s',
	build: function(parent, ctx) {
		if (!box)
			box = lv.obj(parent);
	},
	enter: function() {
		box.set({ w: 10, h: 10 });
	}
};
`;

mkdir(DIR);
mkdir(DIR + '/pages');
writefile(DIR + '/pages/open.uc', sprintf(STATEFUL, 'public'));
writefile(DIR + '/pages/closed.uc', sprintf(STATEFUL, 'private'));

if (!lv.init())
	die('cannot initialise LVGL');

if (!lv.display_drm(getenv('PANEL_DRM_DEVICE') ?? '/dev/dri/card0', -1))
	die('cannot open the DRM display');

loader_init({ dir: DIR, config: 'lvgl_rebuild_test_absent' });

let pin = { name: 'pin', needs: [], unlock: null };

pin.build = function(parent, ctx) {
	pin.unlock = ctx.unlock;
};

ui_build([ page_load('open', null), page_load('closed', null) ],
	 { name: 'lock', needs: [], build: function() {} }, pin, '123456',
	 null, true, true, { public: [] });

view_wake();
check('starts public', view_status().pages, [ 'open' ]);

survives('PIN rebuilds stateful pages', function() {
	view_pin();
	pin.unlock();
	nav_flush();
});
check('private after the PIN', view_status().private, true);

survives('logout rebuilds stateful pages', function() {
	view_logout();
});
check('public after logout', view_status().private, false);

survives('timeout rebuilds stateful pages', function() {
	view_pin();
	pin.unlock();
	nav_flush();
	view_lock();
});
check('idle after the timeout', view_status().locked, true);

unlink(DIR + '/pages/open.uc');
unlink(DIR + '/pages/closed.uc');
rmdir(DIR + '/pages');
rmdir(DIR);

printf('\n%s: %d of %d checks failed\n', failures ? 'FAIL' : 'PASS', failures,
       checks);

lv.drm_drop_master();

exit(failures ? 1 : 0);
