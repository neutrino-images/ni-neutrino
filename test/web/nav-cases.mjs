import { visibleAreas } from '../../data/ni-web/app/nav.js';

let checked = 0;
let failed = 0;

/**
 * @param {boolean} ok
 * @param {string} what
 */
function is(ok, what) {
	checked++;
	if (!ok) {
		failed++;
		process.stderr.write('nav: ' + what + '\n');
	}
}

/** @param {readonly { id: string }[]} list */
function ids(list) {
	return list.map(function (a) { return a.id; }).join(',');
}

const all = [
	{ id: 'plain', items: [] },
	{ id: 'doc', needs: 'api-doc', items: [] },
	{ id: 'ai', needs: 'mcp', items: [] }
];

is(ids(visibleAreas(all, null)) === 'plain,doc', 'before the box has said, an area needing AI access is held back');
is(ids(visibleAreas(all, { apiDoc: true, mcp: true })) === 'plain,doc,ai', 'a build with both offers both');
is(ids(visibleAreas(all, { apiDoc: true, mcp: false })) === 'plain,doc', 'a build without AI access does not offer it');
is(ids(visibleAreas(all, { apiDoc: false, mcp: true })) === 'plain,ai', 'the two switches are read apart');

if (checked !== 4) {
	process.stderr.write('nav: ran ' + checked + ' checks where there are 4\n');
	process.exit(1);
}
if (failed)
	process.exit(1);
console.log('nav: ' + checked + ' checks');
