// What this destination holds, stated whole.
//
// The grid first, because the first entry is what /epg opens on and what
// somebody reaching for the guide means is the whole evening at once. The
// three behind it narrow that down: one channel, one search, one file.
/** @type {Web.NavArea} */
export default {
	id: 'epg',
	text: 'nav.epg',
	ic: '▤',
	items: [
		{ id: 'grid', text: 'nav.epg.grid', load: () => import('./grid.js') },
		{ id: 'schedule', text: 'nav.epg.schedule', load: () => import('./schedule.js') },
		{ id: 'search', text: 'nav.epg.search', load: () => import('./search.js') },
		{ id: 'xmltv', text: 'nav.epg.xmltv', load: () => import('./xmltv.js') }
	]
};
