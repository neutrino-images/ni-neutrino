// The one thing that lies over the page.
//
// The menu of the narrow folding is a sheet, signing in is a sheet, and a
// question before something irreversible is a sheet. One mechanism, so there
// is one focus trap, one escape key and one place where the focus goes back to
// where it came from. Two would be two, and the second would be the one
// nobody tested with a keyboard.
import { html, Fragment, Link, useRef, useEffect } from '../runtime.js';
import { t } from '../i18n.js';
import text from '../shell.text.js';
import { labelOf, hrefFor } from '../nav.js';

const FOCUSABLE = 'a[href], button:not([disabled]), input:not([disabled]), select:not([disabled]), textarea:not([disabled]), [tabindex]:not([tabindex="-1"])';

/**
 * @param {{
 *   open: boolean,
 *   centred?: boolean,
 *   label?: string,
 *   children?: unknown,
 *   onClose: () => void
 * }} props
 * @returns {Web.Drawn}
 */
export function Sheet(props) {
	const box = useRef(/** @type {HTMLDivElement | null} */ (null));

	useEffect(function () {
		if (!props.open)
			return undefined;

		const before = /** @type {HTMLElement | null} */ (document.activeElement);
		const node = box.current;
		const first = node && /** @type {HTMLElement | null} */ (node.querySelector(FOCUSABLE));
		if (first)
			first.focus();

		/**
		 * @param {KeyboardEvent} event
		 * @returns {void}
		 */
		function onKey(event) {
			if (event.key === 'Escape') {
				event.preventDefault();
				props.onClose();
				return;
			}
			if (event.key !== 'Tab' || !box.current)
				return;

			const all = box.current.querySelectorAll(FOCUSABLE);
			const top = /** @type {HTMLElement | undefined} */ (all[0]);
			const last = /** @type {HTMLElement | undefined} */ (all[all.length - 1]);
			if (!top || !last)
				return;
			if (event.shiftKey && document.activeElement === top) {
				event.preventDefault();
				last.focus();
			} else if (!event.shiftKey && document.activeElement === last) {
				event.preventDefault();
				top.focus();
			}
		}

		document.addEventListener('keydown', /** @type {EventListener} */ (onKey), true);
		return function () {
			document.removeEventListener('keydown', /** @type {EventListener} */ (onKey), true);
			// Back where it came from, because a sheet that closes and leaves
			// the focus on the body sends the next key press to the page.
			if (before && before.focus)
				before.focus();
		};
	}, [props.open]);

	if (!props.open)
		return null;

	return html`<${Fragment}>
		<div class="scrim" onClick=${props.onClose}></div>
		<div
			class=${props.centred ? 'sheet centred' : 'sheet'}
			role="dialog"
			aria-modal="true"
			aria-label=${props.label}
			ref=${box}>${props.children}</div>
	<//>`;
}

// THE EIGHT DESTINATIONS, AND NOTHING ELSE.
//
// This used to be an accordion of the destinations and their screens, because it
// was the only way to a screen while the narrow folding had one bar. The screens
// are pills under the header now at every width, so what is left for this to
// carry is the four destinations the bar along the bottom has no room for, and
// the other four beside them so that the list is the whole list.
//
// Who is signed in is not in here either. That control is in the header and is
// there once.
/**
 * @typedef {object} SheetArea
 * @property {string} id
 * @property {string} label
 * @property {string} icon
 * @property {string} href
 * @property {boolean} current
 */

/**
 * @param {readonly Web.NavArea[] | null | undefined} areas
 * @param {string} [activeArea]
 * @returns {SheetArea[]}
 */
export function sheetAreas(areas, activeArea) {
	return (areas || []).map(function (area) {
		return {
			id: area.id,
			label: labelOf(area),
			icon: area.ic || '\u2022',
			href: hrefFor(area.id),
			current: area.id === activeArea
		};
	});
}

/**
 * @param {{
 *   open: boolean,
 *   areas?: readonly Web.NavArea[],
 *   activeArea?: string,
 *   onClose: () => void
 * }} props
 * @returns {Web.Drawn}
 */
export function MenuSheet(props) {
	const shown = sheetAreas(props.areas, props.activeArea);

	return html`<${Sheet} open=${props.open} onClose=${props.onClose} label=${t(text, 'shell.areas')}>
		<h2>${t(text, 'shell.areas')}</h2>
		<ul class="sheet-areas">
			${shown.map(function (area) {
				return html`<li key=${area.id}>
					<${Link}
						href=${area.href}
						data-current=${area.current ? 'page' : null}
						onClick=${props.onClose}>
						<span class="ic" aria-hidden="true">${area.icon}</span>
						<span>${area.label}</span>
					<//>
				</li>`;
			})}
		</ul>
	<//>`;
}
