// What this destination holds, which only the running box can say. The tables name
// sixteen sections and a build carries the rows it was compiled with, so a list written
// here would be a second truth beside the box's own and would offer a section this box
// does not have.
//
// Every section is the same screen with a different name, because one form is generated
// from what the box declares.
const form = () => import('./form.js');

/** @type {Web.NavArea} */
export default {
	id: 'settings',
	text: 'nav.settings',
	ic: '⚒',
	items: (/** @type {Web.Context} */ ctx) => ctx.api("GET", "/api/v1/settings/sections").then(function (answer) {
		const rows = (answer && answer.items) || [];
		return rows.map(function (row) {
			// The key first and the plain name behind it: a section this page
			// has words for is shown in them, and one it has never heard of is
			// shown as the box spells it rather than as a key.
			return { id: row.id, text: 'nav.settings.' + row.id, label: row.id, load: form };
		});
	})
};
