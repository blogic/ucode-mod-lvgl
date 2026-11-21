/*
 * SPDX-License-Identifier: GPL-2.0-only
 */

'use strict';

/*
 * The theme's screen size follows LVGL_ROTATION, which it reads once at
 * import, before any page lays itself out. Run once per value; needs no
 * display:
 *
 *	for r in 0 90 180 270 x; do LVGL_ROTATION=$r ucode /tmp/theme.uc; done
 *
 * Anything but 90, 180 and 270 counts as 0.
 */

import { W, H, ROTATION, LOCK_X } from 'lvgl.theme';

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

let asked = getenv('LVGL_ROTATION');
let deg = (asked == '90' || asked == '180' || asked == '270') ? +asked : 0;
let portrait = (deg == 90 || deg == 270);

printf('== LVGL_ROTATION=%s ==\n', asked ?? '(unset)');

check('rotation', ROTATION, deg);
check('width', W, portrait ? 240 : 320);
check('height', H, portrait ? 320 : 240);
check('derived sizes follow the width', LOCK_X, W - 12 - 14);

printf('\n%s: %d of %d checks failed\n', failures ? 'FAIL' : 'PASS', failures,
       checks);

exit(failures ? 1 : 0);
