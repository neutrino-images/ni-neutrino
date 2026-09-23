// What this destination holds, stated whole.
/** @type {Web.NavArea} */
export default {
	id: 'system',
	text: 'nav.system',
	ic: '⚙',
	items: [
		{ id: 'box', text: 'nav.system.box', load: () => import('./box.js') },
		{ id: 'daemons', text: 'nav.system.daemons', load: () => import('./daemons.js') },
		{ id: 'plugins', text: 'nav.system.plugins', load: () => import('./plugins.js') },
		{ id: 'config', text: 'nav.system.config', load: () => import('./config.js') },
		{ id: 'webserver', text: 'nav.system.webserver', load: () => import('./webserver.js') }
	]
};
