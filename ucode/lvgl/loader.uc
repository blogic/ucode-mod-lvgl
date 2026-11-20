'use strict';

import * as ubus from 'ubus';
import * as uci from 'uci';

let app_dir;
let app_config;
let published;
let configured;

function published_read() {
	try {
		return ubus.list() ?? [];
	}
	catch (e) {
		return [];
	}
}

function configured_read() {
	let cursor = uci.cursor();
	let found = {};

	if (!cursor.load(app_config))
		return found;

	for (let option, value in cursor.get_all(app_config, '@panel[0]') ?? {})
		if (value != null && value != '')
			found[option] = true;

	return found;
}

function requires_met(page) {
	let wanted = page.requires;

	if (wanted == null)
		return true;

	published ??= published_read();

	for (let object in type(wanted) == 'array' ? wanted : [ wanted ])
		if (index(published, object) < 0)
			return false;

	return true;
}

function options_met(page) {
	let wanted = page.requires_option;

	if (wanted == null)
		return true;

	configured ??= configured_read();

	for (let option in type(wanted) == 'array' ? wanted : [ wanted ])
		if (!configured[option])
			return false;

	return true;
}

function page_read(path, name) {
	let page;

	try {
		let entry = loadfile(path);

		page = entry ? entry() : null;
	}
	catch (e) {
		warn(sprintf('panel: page %s failed to load: %s\n', name, e));

		return null;
	}

	if (type(page) != 'object' || type(page.build) != 'function') {
		warn(sprintf('panel: page %s returned no builder\n', name));

		return null;
	}

	if (!requires_met(page) || !options_met(page))
		return null;

	page.needs ??= [];
	page.name = name;
	page.path = path;

	return page;
}

/**
 * page_fresh - a new instance of a page, with module state of its own
 * @page: a page this loader returned
 *
 * A page keeps its widgets in module state, so building the same instance
 * twice leaves it holding widgets the first build's screen has deleted. Each
 * call runs the file again and returns an instance that has built nothing yet.
 *
 * Return: the new instance, or @page itself when it did not come from a file
 * or no longer loads.
 */
export function page_fresh(page) {
	if (!page?.path)
		return page;

	let fresh = page_read(page.path, page.name);

	if (!fresh)
		return page;

	fresh.config = page.config;
	fresh.test = page.test;

	return fresh;
};

/**
 * loader_init - say where the application keeps its pages
 * @opts: dir, the directory holding pages/, subpages/ and templates/; config,
 *	  the uci config whose panel section requires_option is checked against
 *
 * Call once, before any page is loaded.
 */
export function loader_init(opts) {
	app_dir = opts.dir;
	app_config = opts.config;
};

/**
 * page_load - load a root page by name
 * @name: the name from the uci pages list
 * @section: its config page section, else null. With a template option it
 *	      makes a template instance; without one it only carries settings
 *	      for the page of that name, such as its visibility.
 *
 * Return: the page object, or null when it will not load, its ubus object is
 * absent, or an option it names is not set.
 */
export function page_load(name, section) {
	let path = section?.template
		? sprintf('%s/templates/%s.uc', app_dir, section.template)
		: sprintf('%s/pages/%s.uc', app_dir, name);
	let page = page_read(path, name);

	if (page)
		page.config = section;

	return page;
};

/**
 * subpage_load - load a sub page by name
 * @name: the file name under subpages/, without the extension
 *
 * Return: the page object, or null.
 */
export function subpage_load(name) {
	return page_read(sprintf('%s/subpages/%s.uc', app_dir, name), name);
};

/**
 * pages_load - load the root set
 * @names: page names, in the order they are shown
 * @sections: config page sections, keyed by name
 *
 * Return: the pages that loaded, in order. A page that did not is left out.
 */
export function pages_load(names, sections) {
	let loaded = [];

	for (let name in names) {
		let page = page_load(name, sections[name]);

		if (page)
			push(loaded, page);
	}

	return loaded;
};
