/*
 * SPDX-License-Identifier: GPL-2.0-only
 */

'use strict';

/*
 * Where a gauge puts its caption: beside the ring on either side, as before,
 * or below it, for a portrait panel where two gauges share 240 px. Takes DRM
 * master:
 *
 *	ucode /tmp/gauge.uc
 */

import * as lv from 'lv';
import { gauge_new } from 'lvgl.component.gauge';

let failures = 0;
let checks = 0;

function check(what, got, want) {
	let ok = (sprintf('%J', got) == sprintf('%J', want));

	checks++;

	if (!ok)
		failures++;

	printf('%-44s %-10J %s\n', what, got,
	       ok ? 'ok' : sprintf('want %J', want));
}

if (!lv.init())
	die('cannot initialise LVGL');

if (!lv.display_drm(getenv('PANEL_DRM_DEVICE') ?? '/dev/dri/card0', -1))
	die('cannot open the DRM display');

let screen = lv.screen_create();

screen.style({ pad_all: 0, border_width: 0 });
lv.screen_load(screen);

function cell(x, w, h) {
	let c = lv.obj(screen);

	c.style({ pad_all: 0, border_width: 0, bg_opa: lv.OPA_TRANSP });
	c.set({ x, y: 0, w, h });

	return c;
}

let left = gauge_new(cell(0, 160, 120), { caption: 'CPU', unit: '%' });
let below = gauge_new(cell(160, 120, 120), { caption: 'Temp', unit: '%',
					     below: true });

left.set(40);
below.set(40);
screen.update_layout();

check('beside: caption right of the ring',
      left.caption.coords().x1 > left.ring.coords().x2, true);
check('beside: caption level with the ring',
      left.caption.coords().y1 > left.ring.coords().y1 &&
      left.caption.coords().y2 < left.ring.coords().y2, true);

let rc = below.ring.coords(), cc = below.caption.coords();

check('below: caption under the ring', cc.y1 > rc.y2, true);
let off = (cc.x1 + cc.x2) - (rc.x1 + rc.x2);

check('below: caption centred on the ring', off >= -2 && off <= 2, true);
check('below: stays inside its cell', cc.y2 < 120 && rc.x1 >= 160, true);

printf('\n%s: %d of %d checks failed\n', failures ? 'FAIL' : 'PASS', failures,
       checks);

lv.drm_drop_master();

exit(failures ? 1 : 0);
