// The header, which is the same header at every width.
//
// Wide it is one row: the mark, the eight destinations, what is playing, and who is
// signed in. Narrow the destinations leave it for the bar along the bottom and the row
// folds in two, with what is playing across the whole width underneath. Which of the
// two is on screen is decided by the stylesheet, so there is one piece of markup and
// no width is read in JavaScript.
//
// What is playing comes before the session and not after it. Both sit at the right
// edge, and of the two the channel is the one somebody looks at while the page is
// open.
//
// The second level is under the first and not beside it. A column down the left costs
// two hundred pixels on every screen, and the two screens that need the width most are
// the guide and the bouquets.
import { html, Link, useState, useEffect, useId } from '../runtime.js';
import { t } from '../i18n.js';
import text from '../shell.text.js';
import { labelOf, hrefFor } from '../nav.js';
import { askToSignIn, signOut } from './signin.js';
import { SIZES, chosen, choose } from './textsize.js';
import { OnAir } from './onair.js';
import { Sheet } from './sheet.js';
import { Dialog } from './dialog.js';

/* WHO IS SIGNED IN, AND THE ONLY PLACE ON THE PAGE THAT SAYS SO.

   Signed in this is a quiet pill carrying the name, and pressing it opens what there
   is to do about the session. Signed out it is one button that says Anmelden and no
   sentence beside it.

   The sentence is gone on purpose. A page that explains a missing session in the
   corner, and again over every list, and again under every control that is disabled
   for want of one, explains it five times to somebody who came to read something. What
   needs a session says so at the moment it is asked for: session.requireWrite() raises
   this same sheet and the write goes through behind it.

   THE NAME IS NOT A SIGN OUT BUTTON. That would be the one control on the page ending
   a session on a single press, sitting in the corner a pointer crosses on its way to
   the destinations.

   data-session says which of the two this is, because what a check outside this page
   can hold to is a value and not a sentence in one of two languages. */

/* WHAT THIS SESSION MAY DO, AS A SENTENCE AND NOT AS A WORD. One word for a
   level says nothing about the button the box has just refused; a sentence
   says what it will and will not take, which is the only thing worth knowing
   at that moment. The four are written out rather than built from the level,
   because a key a call assembles stands in no catalogue to be found. */
/** @type {Record<string, string>} */
const MAY = {
	public: 'shell.session.may.public',
	read: 'shell.session.may.read',
	write: 'shell.session.may.write',
	system: 'shell.session.may.system'
};

/* HOW BIG THE WRITING IS, AND IT IS NOT A SETTING OF THE BOX.

   This answers people who cannot read the page at the size it ships with. A key in the
   box's configuration would be one household agreeing on one number, and it would
   follow the box to a phone whose owner has already told it how large to draw text.
   See ui/textsize.js.

   Radio buttons and not three buttons drawn as chosen: this is one choice out of three,
   which is what a reader is told when it is written down as one, and what the arrow
   keys then walk through.

   IT IS IN THIS MENU BECAUSE THIS IS THE ONLY MENU THE HEADER HAS, and that means
   somebody who has not signed in cannot reach it, although reading these pages needs no
   session. What such a reader still has is the size their own browser is set to, which
   this page obeys now that every size on it is a fraction of the root. */
/**
 * @returns {Web.Drawn}
 */
function TextSize() {
	const group = useId();
	const [size, setSize] = useState(chosen());

	return html`<fieldset class="text-size">
		<legend>${t(text, 'shell.textsize')}</legend>
		<div class="text-size-set">
			${SIZES.map(function (one) {
				return html`<label key=${one.id}>
					<input
						type="radio"
						name=${group}
						value=${one.id}
						checked=${one.id === size}
						onChange=${function () { choose(one.id); setSize(one.id); }} />
					<span>${t(text, 'shell.textsize.' + one.id)}</span>
				</label>`;
			})}
		</div>
		<p class="text-size-where">${t(text, 'shell.textsize.where')}</p>
	</fieldset>`;
}

/**
 * @param {{
 *   status?: Web.ShellStatus | null,
 *   onAct?: () => void
 * }} props onAct is what a sheet hands in so that it closes before this one
 *   opens: two of these over one another are two focus traps, and the one
 *   underneath is the one nobody gets out of
 * @returns {Web.Drawn}
 */
export function Status(props) {
	const session = props.status && props.status.session;
	const signedIn = !!(session && session.authenticated);
	const user = (session && session.user) || '';
	const [menu, setMenu] = useState(false);
	const [leaving, setLeaving] = useState(false);

	/* A session can go while this is open: it runs out overnight, or a write
	   somewhere else is refused and the sheet that asks for a name goes up.
	   Both are drawn by taking these off the screen, and both have to forget
	   them as well, or the next sign in raises whichever was left standing. */
	/* A session can go while this is open: it runs out overnight, or a write
	   somewhere else is refused and the sheet that asks for a name goes up.
	   The question about signing out has to go with it, or the next sign in
	   raises the one that was left standing. The menu itself stays: it holds
	   the text size, which a reader without a session is entitled to, and it
	   draws itself as the signed out one from the next line onwards. */
	useEffect(function () {
		if (!signedIn)
			setLeaving(false);
	}, [signedIn]);

	/* The same press opens the same menu either way. It used to go straight to
	   the sheet that asks for a name, which put the text size behind a session
	   that reading does not need, and behind a password the reader who most
	   needs larger writing is least likely to have. */
	function act() {
		if (props.onAct)
			props.onAct();
		setMenu(true);
	}

	/* The menu and the question are never on screen together: the one press
	   that opens the question closes the menu in the same draw, so this file
	   keeps the rule the sheet is built on rather than stacking two of them. */
	function ask() {
		setMenu(false);
		setLeaving(true);
	}

	const named = t(text, 'shell.session.user', { user: user });
	const may = MAY[(session && session.level) || 'public'];

	/* The name alone is what is on it, and the sentence about it is what a
	   reader is given: a control read out as "root" is one nobody can tell is a
	   control, and the whole sentence written across the bar is a third of the
	   row spent saying something that changes once a week. */
	return html`<div class="status" data-session=${signedIn ? 'in' : 'out'}>
		<button
			class="session-act"
			type="button"
			title=${signedIn ? named : null}
			aria-haspopup="dialog"
			aria-expanded=${menu ? 'true' : 'false'}
			aria-label=${signedIn ? named : null}
			onClick=${act}>
			${signedIn
				? html`<span class="dot run" aria-hidden="true"></span><span class="session-who">${user}</span>`
				: t(text, 'shell.signin.open')}
		</button>
		<${Sheet}
			open=${menu}
			centred=${true}
			label=${signedIn ? named : t(text, 'shell.session.none')}
			onClose=${function () { setMenu(false); }}>
			<h2>${signedIn ? named : t(text, 'shell.session.none')}</h2>
			${signedIn
				? (may ? html`<p class="session-may">${t(text, may)}</p>` : null)
				: html`<p class="session-may">${t(text, 'shell.signin.why')}</p>`}
			<${TextSize} />
			<ul class="sheet-areas session-menu">
				${signedIn
					? html`<li>
						<${Link}
							href=${hrefFor('system', 'webserver')}
							onClick=${function () { setMenu(false); }}>${t(text, 'shell.session.webserver')}<//>
					</li>
					<li>
						<button type="button" onClick=${ask}>${t(text, 'shell.signout')}</button>
					</li>`
					: html`<li>
						<button type="button" onClick=${function () {
							setMenu(false);
							askToSignIn();
						}}>${t(text, 'shell.signin.open')}<//>
					</li>`}
			</ul>
		<//>
		<${Dialog}
			open=${signedIn && leaving}
			title=${t(text, 'shell.signout')}
			confirmLabel=${t(text, 'shell.signout')}
			onCancel=${function () { setLeaving(false); }}
			onConfirm=${function () { setLeaving(false); signOut(); }}>
			<p>${t(text, 'shell.signout.ask')}</p>
		<//>
	</div>`;
}

/* DRAWN AS THE ONE THE PAGE IS ON, AND NOT ANNOUNCED AS THE CURRENT ONE, which
   are two different statements and were being made with one attribute.

   aria-current is read out, and a reader that meets it twice on one page says
   "current" twice about two different things: this row names the destination
   and the pills under it name the screen inside that destination, and only the
   second of those is where the address actually is. So the mark that is read
   out belongs to the pills and is there exactly once, and this row states in an
   attribute of its own that it is the destination being shown, which is what
   the stylesheet colours it by.

   THE LAST ONE IS SET APART AND IS NOT COLOURED. A rule with a bar before it
   says "and then there is this"; a coloured button in a row of plain ones says
   "this is the one you are on", which is a statement this row already makes
   with the attribute above and must not make twice. */
/**
 * @param {{
 *   areas?: readonly Web.NavArea[],
 *   activeArea?: string
 * }} props
 * @returns {Web.Drawn}
 */
export function AreaTabs(props) {
	const areas = props.areas || [];

	return html`<nav class="areas" aria-label=${t(text, 'shell.areas')}>
		${areas.map(function (area) {
			const here = area.id === props.activeArea;
			return html`<${Link}
				key=${area.id}
				href=${hrefFor(area.id)}
				class=${area.apart ? 'apart' : null}
				data-current=${here ? 'page' : null}>${labelOf(area)}<//>`;
		})}
	</nav>`;
}

/* The screens of the destination that is open. Null while the answer to what
   they are is still on its way, which is the state of the settings and of
   nothing else: an empty row would say the destination has no screens. */
/**
 * @param {{
 *   area?: Web.NavArea | null,
 *   items?: Web.NavEntry[] | null,
 *   activeEntry?: string
 * }} props
 * @returns {Web.Drawn}
 */
export function EntryTabs(props) {
	const area = props.area;
	const items = props.items;
	if (!area || !items || items.length === 0)
		return null;

	return html`<nav class="entries" aria-label=${t(text, 'shell.second', { area: labelOf(area) })}>
		${items.map(function (entry) {
			return html`<${Link}
				key=${entry.id}
				href=${hrefFor(area.id, entry.id)}
				aria-current=${entry.id === props.activeEntry ? 'page' : null}>${labelOf(entry)}<//>`;
		})}
	</nav>`;
}

/* THE MARK SAYS WHETHER THE EVENT STREAM IS CARRYING. Accent while a stream is
   open, grey while there is none.

   NOT WHAT THE BOX IS ON, which is what it used to say and what the chip
   further along the row already says in words. Two marks for one fact left the
   one thing the page cannot otherwise show unsaid: everything on these screens
   refreshes itself off that stream, and a page whose stream has fallen goes on
   looking exactly right while it grows an hour stale. This is the only place
   that says so at every width and on every screen.

   Read as a colour and never as the only statement of it: the screen under
   system says the same thing in a sentence, and a reader who separates no
   colours is told there rather than here.

   Drawn here rather than fetched as the icon file: a picture behind an img
   takes no colour from the page, and that file cannot be given one either,
   because its other reader is a tab strip with no page to take a colour from. */
/**
 * @param {{ carrying?: boolean }} props
 * @returns {Web.Drawn}
 */
function Mark(props) {
	return html`<svg class="brand-mark" data-live=${props.carrying ? 'one' : null}
		viewBox="0 0 32 32" aria-hidden="true">
		<circle cx="9" cy="23" r="3" fill="currentColor" />
		<path d="M9 16a7 7 0 0 1 7 7" fill="none" stroke="currentColor" stroke-width="3" stroke-linecap="round" />
		<path d="M9 9a14 14 0 0 1 14 14" fill="none" stroke="currentColor" stroke-width="3" stroke-linecap="round" />
	</svg>`;
}

/**
 * @param {{
 *   areas?: readonly Web.NavArea[],
 *   activeArea?: string,
 *   area?: Web.NavArea | null,
 *   items?: Web.NavEntry[] | null,
 *   activeEntry?: string,
 *   status?: Web.ShellStatus | null,
 *   event?: { title?: string, start?: number, duration?: number } | null
 * }} props
 * @returns {Web.Drawn}
 */
export function Topbar(props) {
	const status = props.status;

	return html`<header class="topbar">
		<div class="topbar-main">
			<span class="brand">
				<${Mark} carrying=${!!(status && status.carrying)} />${t(text, 'shell.brand')}
			</span>
			<${AreaTabs} areas=${props.areas} activeArea=${props.activeArea} />
			<${OnAir}
				channel=${status ? status.channel : null}
				event=${props.event}
				recordings=${status && status.recordings ? status.recordings.length : 0} />
			<${Status} status=${status} />
		</div>
		<${EntryTabs} area=${props.area} items=${props.items} activeEntry=${props.activeEntry} />
	</header>`;
}
