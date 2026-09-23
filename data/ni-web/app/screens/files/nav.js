// What this destination holds, stated whole.
//
// The third is what the old interface kept on two pages under two names: what
// is mounted now, and what is to be mounted next time. The second of those is
// the one written here, beside the first rather than somewhere else entirely.
/** @type {Web.NavArea} */
export default {
	id: 'files',
	text: 'nav.files',
	ic: '▣',
	items: [
		{ id: 'list', text: 'nav.files.list', load: () => import('./list.js') },
		{ id: 'mounts', text: 'nav.files.mounts', load: () => import('./mounts.js') },
		{ id: 'netfs', text: 'nav.files.netfs', load: () => import('./netfs.js') }
	]
};
