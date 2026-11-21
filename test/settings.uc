/*
 * SPDX-License-Identifier: GPL-2.0-only
 */

'use strict';

/*
 * The runtime's reading of the public list and the PIN. Needs no display, but writes a
 * throwaway config into the uci directory and removes it again:
 *
 *	ucode /tmp/settings.uc
 *
 * LVGL_TEST_CONFDIR names another directory, for a runtime whose cursor
 * points elsewhere; the default is /etc/config.
 */

import * as uci from 'uci';
import { writefile, unlink } from 'fs';
import { runtime_init, settings, pin_valid } from 'lvgl.runtime';

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

/* Sets the PIN option and reads it back through the runtime. */
function pin_read(value) {
	cursor.load(CONFIG);

	if (value == null)
		cursor.delete(CONFIG, '@panel[0]', 'pin');
	else
		cursor.set(CONFIG, '@panel[0]', 'pin', value);

	cursor.commit(CONFIG);
	runtime_init({ config: CONFIG });

	return settings.pin;
}

check('no PIN option: no PIN', pin_read(null), null);
check('empty PIN option: no PIN', pin_read(''), null);

for (let pin in [ '1234', '12345', '123456', '12345678' ]) {
	check(sprintf('PIN %s is taken', pin), pin_read(pin), pin);
	check(sprintf('PIN %s is valid', pin), pin_valid(settings.pin), true);
}

/* A PIN that is set but unusable must keep the private pages shut: it stays
   set, so the framework still has a keypad and a public mode, and nobody can
   type it. */
for (let pin in [ '123', '123456789', '12a4', ' 1234' ]) {
	check(sprintf('bad PIN "%s" stays set', pin), pin_read(pin), pin);
	check(sprintf('bad PIN "%s" is not valid', pin),
	      pin_valid(settings.pin), false);
}

let listed = pin_read([ '1234', '5678' ]);

check('PIN given as a list stays set', !!listed, true);
check('PIN given as a list is not valid', pin_valid(listed), false);
check('removing the PIN clears it', pin_read(null), null);

check('pin_valid: nothing', pin_valid(null), false);

unlink(sprintf('%s/%s', DIR, CONFIG));

printf('\n%s: %d of %d checks failed\n', failures ? 'FAIL' : 'PASS', failures,
       checks);

exit(failures ? 1 : 0);
