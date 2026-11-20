/*
 * SPDX-License-Identifier: GPL-2.0-only
 */

'use strict';

/*
 * Page sections in the loader: a config page section names a template, or
 * now also only carries settings for a plain page, such as its visibility.
 * Needs no display:
 *
 *	ucode /tmp/loader.uc
 *
 * Writes its pages to /tmp/lvgl-loader-test and removes them again.
 */

import { mkdir, writefile, unlink, rmdir } from 'fs';
import { loader_init, page_load } from 'lvgl.loader';

const DIR = '/tmp/lvgl-loader-test';

let failures = 0;
let checks = 0;

function check(what, got, want) {
	let ok = (sprintf('%J', got) == sprintf('%J', want));

	checks++;

	if (!ok)
		failures++;

	printf('%-44s %-20J %s\n', what, got,
	       ok ? 'ok' : sprintf('want %J', want));
}

const PAGE = "return { visibility: 'public', build: function() {} };\n";

mkdir(DIR);
mkdir(DIR + '/pages');
mkdir(DIR + '/templates');
writefile(DIR + '/pages/plain.uc', PAGE);
writefile(DIR + '/templates/tpl.uc', PAGE);

loader_init({ dir: DIR, config: 'lvgl_loader_test_absent' });

let page = page_load('plain', null);

check('plain page loads', page?.name, 'plain');
check('page default visibility kept', page?.visibility, 'public');

page = page_load('plain', { name: 'plain', visibility: 'private' });

check('section without template still loads', page?.name, 'plain');
check('section is handed over as config', page?.config?.visibility, 'private');

page = page_load('mine', { name: 'mine', template: 'tpl' });

check('section with template uses it', page?.name, 'mine');

check('missing page stays null', page_load('absent', null), null);

unlink(DIR + '/pages/plain.uc');
unlink(DIR + '/templates/tpl.uc');
rmdir(DIR + '/pages');
rmdir(DIR + '/templates');
rmdir(DIR);

printf('\n%s: %d of %d checks failed\n', failures ? 'FAIL' : 'PASS', failures,
       checks);

exit(failures ? 1 : 0);
