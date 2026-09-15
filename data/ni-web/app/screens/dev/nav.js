// What this destination holds, stated whole.
//
// IT IS NOT ALWAYS THERE, AND IT IS NOT EMPTY WHERE IT IS LEFT OUT. A build
// compiled without the API documentation carries neither the prose in the
// document this server writes about itself nor the reader page beside it; the
// paths, the groups and the levels stay in that document, so the first screen
// here still has its list. It is kept out of the bars regardless: that switch
// is thrown to make a small box fit, and advertising a destination there would
// be advertising the half of it the switch took away. The box says which kind
// of build it is (GET /api/v1/system/info, api_doc) and the frame leaves this
// out of both foldings where it says no; the address itself keeps working,
// because a bookmark that answers on Monday and 404s on Tuesday is worse than
// a destination nobody advertised, and the screen behind it says for itself
// which of the two builds it is reading.
/** @type {Web.NavArea} */
export default {
	id: 'dev',
	text: 'nav.dev',
	ic: '◇',
	apart: true,
	needs: 'api-doc',
	items: [
		{ id: 'api', text: 'nav.dev.api', was: ['routes'], load: () => import('./api.js') },
		{ id: 'events', text: 'nav.dev.events', load: () => import('./events.js') }
	]
};
