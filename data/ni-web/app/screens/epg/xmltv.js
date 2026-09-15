/* The guide as one document, which is a link and deliberately not a screen.

   What the box writes here is every user bouquet's whole schedule as one XMLTV file.
   That belongs in a file and not in a DOM: it is built for another program to read,
   it is megabytes on a full channel list, and drawing it would be this interface's
   own channel schedule spelt out a second time in somebody else's markup.

   The address is put together out of the path the document knows and a query beside
   it rather than written as one string, because a string carrying a query is not a
   path anything can hold to the document the server writes about itself. */
import { html, useState } from '../../runtime.js';
import { t } from '../../i18n.js';
import { Select } from '../../ui/select.js';
import text from './xmltv.text.js';

export const css = '/app/screens/epg/schedule.css';
/** @returns {string} the sentence the frame draws under the name of this screen */
export function lead() { return t(text, 'epg.xmltv.what'); }


const kDocument = '/api/v1/epg/xmltv';

// The three the route declares, in the order a person would read them. all is
// what the route answers when it is told nothing, and it is named here rather
// than left out so that the control always says what is going to be fetched.
const kModes = ['all', 'tv', 'radio'];

/**
 * The address one choice asks for.
 *
 * @param {string} mode
 * @returns {string}
 */
export function documentHref(mode) {
	return kDocument + '?mode=' + encodeURIComponent(mode);
}

/**
 * @param {Record<string, never>} _props
 * @returns {Web.Drawn}
 */
export default function Xmltv(_props) {
	const [mode, setMode] = useState('all');

	/** @param {{ currentTarget: HTMLSelectElement }} event */
	function pickMode(event) {
		setMode(event.currentTarget.value);
	}

	/* THE TWO WAYS OUT OF HERE, AND WHY ONLY ONE OF THEM OPENS A TAB.

	   Looking at the document leaves the application for a page of somebody
	   else's, so it goes in a tab of its own: in this one the choice above and
	   where the page was scrolled to would be gone, and the way back is a second
	   whole load of the application. Keeping the file gets no tab, because a tab
	   that only starts a download is an empty window left behind. */
	const href = documentHref(mode);
	// What a person would type somewhere else, which is the whole address and
	// not the path: the other program is not this page and has no origin.
	const whole = window.location.origin + href;

	return html`<section class="epg epg-doc">
		<${Select}
			label=${t(text, 'epg.xmltv.mode')}
			value=${mode}
			onChange=${pickMode}
			options=${kModes.map(function (one) {
				return { value: one, label: t(text, 'epg.xmltv.mode.' + one) };
			})} />
		<p>${t(text, 'epg.xmltv.address')}</p>
		<code class="epg-address mono">${whole}</code>
		<p class="epg-actions">
			<a class="btn" href=${href} target="_blank" rel="noopener">${t(text, 'epg.xmltv.open')}</a>
			${' '}
			<a class="btn" href=${href} download="epg.xml">${t(text, 'epg.xmltv.save')}</a>
		</p>
		<p>${t(text, 'epg.xmltv.cost')}</p>
	</section>`;
}
