/** @type {Web.NavArea} */
export default {
	id: 'recordings',
	text: 'nav.recordings',
	ic: '●',
	items: [
		{ id: 'archive', text: 'nav.recordings.archive', load: () => import('./archive.js') },
		{ id: 'running', text: 'nav.recordings.running', load: () => import('./running.js') }
	]
};
