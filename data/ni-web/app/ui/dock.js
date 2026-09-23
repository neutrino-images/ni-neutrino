// The bar along the bottom, which is where the destinations go on anything
// narrower than a laptop.
//
// FOUR AND A FIFTH THAT OPENS THE REST. Not eight: a target under 44 pixels is
// one a thumb misses, and eight across a 360 pixel screen leaves 45 pixels each
// with nothing between them. The four are the ones asked for all day; the rest
// are one tap away behind the fifth, which is also how a destination that is
// not one of the four says it is the one being shown.
//
// Along the bottom and not along the top, on the tablet as well as on the
// phone: a device held in two hands is reached at the bottom of the screen.
import { html, Link } from '../runtime.js';
import { t } from '../i18n.js';
import text from '../shell.text.js';
import { labelOf, areaById, hrefFor } from '../nav.js';

// The four, by identifier, in the order they are drawn. Written out here
// rather than taken as the first four of the navigation, because which of them
// is asked for all day is a judgement and the order of the navigation is not.
export const inDock = ['now', 'channels', 'epg', 'timers'];

/**
 * @param {readonly Web.NavArea[] | null | undefined} areas
 * @returns {Web.NavArea[]} the four, in the order above, and only the ones this
 *          build has
 */
export function dockAreas(areas) {
	/** @type {Web.NavArea[]} */
	const out = [];
	for (const id of inDock) {
		for (const area of areas || []) {
			if (area.id === id)
				out.push(area);
		}
	}
	return out;
}

/**
 * @param {{
 *   areas?: readonly Web.NavArea[],
 *   activeArea?: string,
 *   open?: boolean,
 *   onOpen: () => void
 * }} props
 * @returns {Web.Drawn}
 */
export function Dock(props) {
	const shown = dockAreas(props.areas);
	/* The fifth is marked when the destination on screen is not one of the
	   four, so the bar always says where the page is rather than saying
	   nothing about five of the eight. */
	const elsewhere = inDock.indexOf(props.activeArea || '') === -1;
	const here = areaById(props.activeArea || '');

	return html`<nav class="dock" aria-label=${t(text, 'shell.areas')}>
		${shown.map(function (area) {
			return html`<${Link}
				key=${area.id}
				href=${hrefFor(area.id)}
				data-current=${area.id === props.activeArea ? 'page' : null}>
				<span class="ic" aria-hidden="true">${area.ic || '•'}</span>
				<span>${labelOf(area)}</span>
			<//>`;
		})}
		<button
			type="button"
			class="dock-more"
			aria-haspopup="dialog"
			aria-expanded=${props.open ? 'true' : 'false'}
			data-current=${elsewhere ? 'page' : null}
			onClick=${props.onOpen}>
			<span class="ic" aria-hidden="true">⋯</span>
			<span>${elsewhere && here ? labelOf(here) : t(text, 'shell.more.areas')}</span>
		</button>
	</nav>`;
}
