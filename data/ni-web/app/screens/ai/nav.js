/** @type {Web.NavArea} */
export default {
	id: 'ai',
	text: 'nav.ai',
	ic: '◎',
	needs: 'mcp',
	items: [
		{ id: 'access', text: 'nav.ai.access', load: () => import('../../../ai/access.js') },
		{ id: 'clients', text: 'nav.ai.clients', load: () => import('../../../ai/clients.js') },
		{ id: 'allow', text: 'nav.ai.allow', load: () => import('../../../ai/allow.js') },
		{ id: 'guides', text: 'nav.ai.guides', load: () => import('../../../ai/guides.js') }
	]
};
