/*
 * SPDX-License-Identifier: GPL-2.0-only
 */

'use strict';

/*
 * Public and private mode of the page framework.
 *
 * Builds the ui from stub pages and walks the mode changes through the
 * exported view_* calls: wake into public, PIN into private, logout, cancel
 * of the keypad, and the idle timeout. Takes DRM master, so stop whatever
 * owns the panel first:
 *
 *	/etc/init.d/glinet-panel-ui stop
 *	ucode /tmp/modes.uc
 *	/etc/init.d/glinet-panel-ui start
 */

import * as lv from 'lv';
import { ui_build, nav_flush, view_status, view_wake, view_pin, view_lock,
	 view_goto, view_public, view_logout,
	 view_pin_cancel } from 'lvgl.page';

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

/* A root that records what it was built with. */
function stub(name, visibility, config) {
	let page = { name, needs: [], builds: 0, ctx: null };

	if (visibility)
		page.visibility = visibility;
	if (config)
		page.config = config;

	page.build = function(parent, ctx) {
		page.builds++;
		page.ctx = ctx;
		lv.label(parent).text(name);
	};

	return page;
}

function lock_stub() {
	return { name: 'lock', needs: [], build: function(parent, ctx) {} };
}

function pin_stub() {
	let pin = { name: 'pin', needs: [], unlock: null };

	pin.build = function(parent, ctx) {
		pin.unlock = ctx.unlock;
	};

	return pin;
}

function build(list, pin_code, items) {
	let pin = pin_stub();

	ui_build(list, lock_stub(), pin, pin_code, null, true, true,
		 { public: items ?? [] });

	return pin;
}

if (!lv.init())
	die('cannot initialise LVGL');

if (!lv.display_drm(getenv('PANEL_DRM_DEVICE') ?? '/dev/dri/card0', -1))
	die('cannot open the DRM display');

/*
 * One ui per process: ui_build keeps its screens in module state. The no-PIN
 * case is covered by modes-nopin.uc for the same reason.
 */
printf('== PIN set, mixed visibility ==\n');

let traffic = stub('traffic');
let qrcodes = stub('qrcodes', 'private', { visibility: 'public' });
let ports = stub('ports', 'public');
let system = stub('system', 'public', { visibility: 'private' });

let pin = build([ traffic, qrcodes, ports, system ], '123456',
		[ 'ssid:guest' ]);

check('starts on the idle screen', view_status().locked, true);
check('starts public', view_status().private, false);
check('public roots exist', view_public(), true);

view_wake();
check('wake shows the public roots', view_status().pages,
      [ 'qrcodes', 'ports' ]);
check('wake lands on the first public root', view_status().page, 'qrcodes');
check('config visibility beats the page default',
      index(view_status().pages, 'system') < 0, true);
check('built public', qrcodes.ctx.private, false);
check('public item is visible', qrcodes.ctx.visible('ssid', 'guest'),
      true);
check('private item is hidden', qrcodes.ctx.visible('ssid', 'home'), false);
check('a private root cannot be reached', view_goto('traffic'), false);

printf('\n== keypad, cancelled ==\n');

check('keypad opens from public', view_pin(), true);
check('keypad is showing', view_status().pin, true);
view_pin_cancel();
check('cancel returns to public', view_status().private, false);
check('cancel lands on the first public root', view_status().page, 'qrcodes');

printf('\n== keypad, unlocked ==\n');

view_goto('ports');
view_pin();
pin.unlock();
nav_flush();
check('PIN makes it private', view_status().private, true);
check('private shows every root', view_status().pages,
      [ 'traffic', 'qrcodes', 'ports', 'system' ]);
check('private lands on the first root', view_status().page, 'traffic');
check('pages were rebuilt private', qrcodes.ctx.private, true);
check('private item is visible now', qrcodes.ctx.visible('ssid', 'home'),
      true);
check('a private root can be reached', view_goto('system'), true);

printf('\n== logout ==\n');

view_logout();
check('logout makes it public', view_status().private, false);
check('logout lands on the first public root', view_status().page, 'qrcodes');
check('logout leaves the idle screen alone', view_status().locked, false);
check('pages were rebuilt public', qrcodes.ctx.private, false);

printf('\n== idle timeout from private ==\n');

view_pin();
pin.unlock();
nav_flush();
check('private again', view_status().private, true);
view_lock();
check('timeout shows the idle screen', view_status().locked, true);
check('timeout ends private', view_status().private, false);
view_wake();
check('the next wake is public', view_status().pages, [ 'qrcodes', 'ports' ]);

printf('\n%s: %d of %d checks failed\n', failures ? 'FAIL' : 'PASS', failures,
       checks);

lv.drm_drop_master();

exit(failures ? 1 : 0);
