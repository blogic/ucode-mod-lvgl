/*
 * SPDX-License-Identifier: GPL-2.0-only
 */

'use strict';

/*
 * The runtime's reading of the public list. Needs no display, but writes a
 * throwaway config into the uci directory and removes it again:
 *
 *	ucode /tmp/settings.uc
 *
 * LVGL_TEST_CONFDIR names another directory, for a runtime whose cursor
 * points elsewhere; the default is /etc/config.
 */

import * as uci from 'uci';
import { writefile, unlink } from 'fs';
import { runtime_init, settings } from 'lvgl.runtime';

const CONFIG = 'lvgl_settings_test';
const DIR = getenv('LVGL_TEST_CONFDIR') ?? '/etc/config';

let failures = 0;
let checks = 0;

function check(what, got, want) {
	let ok = (sprintf('%J', got) == sprintf('%J', want));

	checks++;

	if (!ok)
		failures++;

	printf('%-44s %-30J %s\n', what, got,
	       ok ? 'ok' : sprintf('want %J', want));
}

/* An empty file is a valid config for uci to add a section to. */
writefile(sprintf('%s/%s', DIR, CONFIG), '');

let cursor = uci.cursor(DIR);

cursor.load(CONFIG);
cursor.add(CONFIG, 'panel');
cursor.set(CONFIG, '@panel[0]', 'public',
	   [ 'ssid:guest', 'netbird:peer-a' ]);
cursor.commit(CONFIG);

runtime_init({ config: CONFIG });

check('public list is read', settings.public,
      [ 'ssid:guest', 'netbird:peer-a' ]);

unlink(sprintf('%s/%s', DIR, CONFIG));

printf('\n%s: %d of %d checks failed\n', failures ? 'FAIL' : 'PASS', failures,
       checks);

exit(failures ? 1 : 0);
