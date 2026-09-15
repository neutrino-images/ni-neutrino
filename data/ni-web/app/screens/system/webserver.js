/* The screen for the server this page came from.

   The one screen here that can cut its own ground. Every other control on these pages
   changes something on the box; this one changes the way the box is being talked to.
   Saving a new port ends the connection the answer arrives on, and nothing else on
   this page would then tell anybody where to look.

   So three things are done that no other screen does. It says what will happen before
   it happens, in the words of the thing being lost rather than of the address being
   called. It reads where the box will be out of the box's own answer, because the box
   is the one that knows what it wrote. And it leads whoever saved to that address
   instead of carrying on drawing a page whose server has gone.

   It does not walk there by itself. The server needs a moment to come back, and a page
   that jumped the instant the answer arrived would land on a refused connection. The
   address is put on the screen as a link and the step is theirs.

   Three of what it shows cannot be changed from here, and that is the route's decision
   and not this screen's shyness: narrowing who reads without signing in locks out
   everybody else on the network rather than the person asking, and the switch under
   the old interface turns off the surface the plugins on this very box talk through.
   They are shown because somebody setting a server up is asking exactly those
   questions. */

import { html, useState, useEffect } from '../../runtime.js';
import * as store from '../../store.js';
import * as session from '../../session.js';
import { t } from '../../i18n.js';
import { State } from '../../ui/state.js';
import { Field } from '../../ui/field.js';
import { Switch } from '../../ui/switch.js';
import { Button } from '../../ui/button.js';
import { Ask, Act, failureText } from './parts.js';
import text from './webserver.text.js';

export const css = '/app/screens/system/system.css';
/** @returns {string} the sentence the frame draws under the name of this screen */
export function lead() { return t(text, 'sys.web.lead'); }


const kPath = '/api/v1/system/webserver';

/**
 * What the box says about its own server, asked for only where this request
 * carries enough to be answered.
 *
 * The whole of this screen is System, the reading included, so a request short
 * of it is not sent at all. Sent anyway it comes back a refusal, and a refusal
 * drawn where the form belongs is this page turning the box down on the box's
 * behalf, which is the one thing the frame stopped doing.
 *
 * @param {boolean} ask
 * @returns {Web.Snapshot<Api.Webserver> | null} null while nothing has been
 *          asked, which is a state the screen draws and not a hole in an answer
 */
function useReading(ask) {
	const [shot, setShot] = useState(/** @type {Web.Snapshot<Api.Webserver> | null} */ (null));
	useEffect(function () {
		if (!ask) {
			setShot(null);
			return undefined;
		}
		/* A refusal this address collected under an earlier session is not an
		   answer to this one, and watching an entry that already failed asks
		   the box for nothing: the store starts a load from nothing held, not
		   from something held that turned out to be a refusal. */
		if (store.read('GET', kPath).state === store.FAILED)
			store.reload('GET', kPath).catch(function () { });
		return store.watch('GET', kPath, null, setShot);
	}, [ask]);
	return shot;
}

/**
 * What a save would send: the members that differ from what the box says, and
 * no others.
 *
 * Only what differs, because the route takes a member left out to mean the
 * value stays. A body restating all four would write back whatever this page
 * last read, which on a box somebody edited at the television in the meantime is
 * a value nobody asked for.
 *
 * The password is the exception in both directions: there is nothing to read
 * back, so an empty field is not a difference and a field with anything in it
 * always is.
 *
 * @param {{ port: string, bind: string, user: string, password: string, logos: boolean }} draft
 * @param {Api.Webserver} now
 * @returns {Record<string, string | number | boolean>}
 */
export function changes(draft, now) {
	/** @type {Record<string, string | number | boolean>} */
	const body = {};
	if (draft.port !== String(now.port))
		body.port = Number(draft.port);
	if (draft.bind !== now.bind)
		body.bind = draft.bind;
	if (draft.user !== now.user)
		body.user = draft.user;
	if (draft.password !== '')
		body.password = draft.password;
	if (logosOf(now) !== null && draft.logos !== logosOf(now))
		body.channel_logos = draft.logos;
	return body;
}

/**
 * Whether the box carries the switch that decides if these pages fetch
 * channel logos, and null for a box that does not answer for it at all.
 *
 * Read through a cast because the document this tree generates does not
 * describe the member yet: it arrives with the build that writes it into the
 * server's own file. Drawn only where the box actually sent it, which is the
 * rule every other row on these pages follows, so this screen is right on a
 * box of either kind and there is no second version of it.
 *
 * @param {Api.Webserver} now
 * @returns {boolean | null}
 */
export function logosOf(now) {
	const said = /** @type {{ channel_logos?: unknown }} */ (/** @type {unknown} */ (now)).channel_logos;
	return typeof said === 'boolean' ? said : null;
}

/**
 * Where this page would be under another port, for the sentence shown before
 * the save.
 *
 * Built from where this page is and not from anything the box said, because at
 * that moment the box has not been asked yet. What the box answers afterwards is
 * the address used from then on, and the two are the same on every box that is
 * not behind something rewriting addresses.
 *
 * @param {number} port
 * @param {Location} where
 * @returns {string}
 */
export function addressUnder(port, where) {
	return where.protocol + '//' + where.hostname + ':' + String(port) + '/';
}

/**
 * One line of the server's own file: what it is called, and beside it what the
 * box answered. A name beside its value and never above it, for the reason
 * written over .sys-read in the stylesheet.
 *
 * @param {{ title: string, rows: string[], none: string }} props
 * @returns {Web.Drawn}
 */
function Networks(props) {
	return html`<dt>${props.title}</dt>
		<dd>${props.rows.length === 0
			? props.none
			: html`<ul class="sys-list">
				${props.rows.map(function (one) {
					return html`<li key=${one} class="mono">${one}</li>`;
				})}
			</ul>`}</dd>`;
}

/**
 * @returns {Web.Drawn}
 */
export default function Webserver() {
	/* What this request was granted, kept only so that the reading is asked for
	   the moment a sign in makes it possible. Which level admits what is asked
	   of the session layer and never worked out from here. */
	const [, setGranted] = useState(session.state());
	useEffect(function () {
		return session.subscribe(setGranted);
	}, []);
	const maySee = session.canSystem();

	/* Arriving is the act, so the sheet is raised on the way in. This screen
	   used to send the reading regardless and draw what came back, which put
	   the box's word Forbidden where the port belongs and left the page there:
	   a sign in afterwards changed nothing on screen, because the address had
	   already been asked and answered. Somebody who closes the sheet is left
	   with the way back in under the heading. */
	useEffect(function () {
		if (maySee)
			return;
		session.requireSystem().catch(function () { });
	}, [maySee]);

	const shot = useReading(maySee);
	const [draft, setDraft] = useState(
		/** @type {{ port: string, bind: string, user: string, password: string, logos: boolean } | null} */ (null));
	const [said, setSaid] = useState('');
	const [moved, setMoved] = useState('');
	const [busy, setBusy] = useState(false);

	const held = shot === null ? null : shot.data;

	/* Seeded once and never again. A second reading arriving while somebody is
	   typing would otherwise take the field out from under them, and there is
	   nothing on this box that changes these values behind this page's back
	   except the screen at the television, which is a person standing in the
	   same room. */
	useEffect(function () {
		if (draft === null && held) {
			setDraft({
				port: String(held.port),
				bind: held.bind,
				user: held.user,
				password: '',
				logos: logosOf(held) !== false
			});
		}
	}, [held, draft]);

	const now = held || null;
	const wanted = (now && draft) ? changes(draft, now) : {};
	const nothing = Object.keys(wanted).length === 0;

	/* The sentence in front of the save, which is the whole of what this screen
	   owes somebody about to lose the connection they are reading it on. */
	const question = (wanted.port !== undefined)
		? t(text, 'sys.web.save.ask.port',
		    { address: addressUnder(Number(wanted.port), window.location) })
		: t(text, 'sys.web.save.ask.plain');

	function save() {
		if (!now || !draft)
			return;
		setSaid('');
		setBusy(true);
		store.write('PUT', kPath, { body: wanted, touches: [kPath] }).then(
			function (answer) {
				setBusy(false);
				/* The address the box named and not the one worked out above.
				   The box is what wrote the file, and on a box reached through
				   something that rewrites addresses it is the only one of the
				   two that was ever true. */
				if (answer.caller.port_moved) {
					setMoved(answer.caller.address
						|| addressUnder(answer.webserver.port, window.location));
					return;
				}
				setSaid(answer.caller.restarting
					? t(text, 'sys.web.restarting')
					: t(text, 'sys.web.nothing'));
				if (draft) {
					setDraft({ port: draft.port, bind: draft.bind, user: draft.user,
						password: '', logos: draft.logos });
				}
			},
			function (caught) {
				setBusy(false);
				setSaid(failureText(caught));
			});
	}

	if (moved !== '') {
		/* The page stops being a form the moment its server is on the way out.
		   Drawing the fields on would invite a second save into a connection
		   that is already gone. */
		return html`<div class="sys">
			<section class="sys-block" data-part="moved">
				<h2>${t(text, 'sys.web.moved.title')}</h2>
				<p class="sys-lead">${t(text, 'sys.web.moved.lead', { address: moved })}</p>
				<p class="sys-lead">${t(text, 'sys.web.moved.wait')}</p>
				<p><a class="btn primary" data-go="moved" href=${moved}>${t(text, 'sys.web.moved.go')}</a></p>
			</section>
		</div>`;
	}

	return html`<div class="sys">
		<section class="sys-block">

			${/* The way back in for somebody who closed the sheet. It asks for
			     the level and nothing else: what happens when it is granted is
			     that the session says so, this draws again, and the reading
			     goes out. */
				maySee ? null : html`<p class="sys-row" data-part="signin">
					<${Button} primary=${true} onClick=${function () { session.requireSystem().catch(function () { }); }}>
						${t(text, 'sys.web.signin')}
					<//>
				</p>`}

			<${State}
				phase=${shot === null ? '' : shot.phase}
				problem=${(shot !== null && shot.error) ? shot.error.problem : null}>
				${(now && draft) ? html`<div>
					<h3>${t(text, 'sys.web.where.title')}</h3>
					<${Field}
						id="web-port"
						label=${t(text, 'sys.web.port')}
						type="number"
						min=${1}
						max=${65535}
						value=${draft.port}
						hint=${t(text, 'sys.web.port.hint')}
						onInput=${function (/** @type {Event} */ e) {
							const box = /** @type {HTMLInputElement} */ (e.currentTarget);
							setDraft({ port: box.value, bind: draft.bind, user: draft.user, password: draft.password, logos: draft.logos });
						}} />
					<${Field}
						id="web-bind"
						label=${t(text, 'sys.web.bind')}
						value=${draft.bind}
						hint=${t(text, 'sys.web.bind.hint')}
						onInput=${function (/** @type {Event} */ e) {
							const box = /** @type {HTMLInputElement} */ (e.currentTarget);
							setDraft({ port: draft.port, bind: box.value, user: draft.user, password: draft.password, logos: draft.logos });
						}} />

					<h3>${t(text, 'sys.web.who.title')}</h3>
					<${Field}
						id="web-user"
						label=${t(text, 'sys.web.user')}
						value=${draft.user}
						autocomplete="username"
						hint=${t(text, 'sys.web.user.hint')}
						onInput=${function (/** @type {Event} */ e) {
							const box = /** @type {HTMLInputElement} */ (e.currentTarget);
							setDraft({ port: draft.port, bind: draft.bind, user: box.value, password: draft.password, logos: draft.logos });
						}} />
					<${Field}
						id="web-password"
						label=${t(text, 'sys.web.password')}
						type="password"
						value=${draft.password}
						autocomplete="new-password"
						hint=${t(text, 'sys.web.password.hint')}
						onInput=${function (/** @type {Event} */ e) {
							const box = /** @type {HTMLInputElement} */ (e.currentTarget);
							setDraft({ port: draft.port, bind: draft.bind, user: draft.user, password: box.value, logos: draft.logos });
						}} />
					<p class="sys-lead" data-part="haspassword">${now.has_password
						? t(text, 'sys.web.password.set')
						: t(text, 'sys.web.password.none')}</p>

					${/* Only what this page does, and the sentence says so: the box
					     draws its own screens from its own logos and never looks
					     here. It sits with the other three because a save of it
					     stops the server like a save of any of them, and drawn
					     anywhere else it would look like the one harmless one. */ null}
					${logosOf(now) === null ? null : html`<div data-part="logos">
						<h3>${t(text, 'sys.web.look.title')}</h3>
						<${Switch}
							id="web-logos"
							label=${t(text, 'sys.web.logos')}
							hint=${t(text, 'sys.web.logos.hint')}
							checked=${draft.logos}
							onChange=${function (/** @type {Event} */ e) {
								const box = /** @type {HTMLInputElement} */ (e.currentTarget);
								setDraft({ port: draft.port, bind: draft.bind, user: draft.user,
									password: draft.password, logos: box.checked });
							}} />
					</div>`}

					<div class="sys-row" data-act="save">
						${nothing
							? html`<${Act}
								label=${t(text, 'sys.web.save')}
								busy=${true}
								onClick=${function () {}} />`
							: html`<${Ask}
								label=${t(text, 'sys.web.save')}
								question=${question}
								needs="system"
								busy=${busy}
								onConfirm=${save} />`}
					</div>
					${said !== '' ? html`<p class="sys-said" role="status" data-part="said">${said}</p>` : null}
					${said !== '' ? html`<p><${Act}
						label=${t(text, 'sys.web.again')}
						onClick=${function () { window.location.reload(); }} /></p>` : null}
				</div>` : null}
			<//>
		</section>

		${/* Not a heading over nothing. The three lines under it are the box's
		     answer, and a request that has not been granted enough to read that
		     answer is not shown a title and a sentence promising it. */
			now === null ? null : html`<section class="sys-block">
			<h2>${t(text, 'sys.web.reach.title')}</h2>
			<p class="sys-lead">${t(text, 'sys.web.reach.lead')}</p>
			<dl class="sys-read">
				<${Networks}
					title=${t(text, 'sys.web.lanread')}
					rows=${now.lan_read}
					none=${t(text, 'sys.web.lanread.none')} />
				<${Networks}
					title=${t(text, 'sys.web.proxies')}
					rows=${now.trusted_proxies}
					none=${t(text, 'sys.web.proxies.none')} />
				<dt>${t(text, 'sys.web.legacy')}</dt>
				<dd data-part="legacy">${now.legacy_enabled
					? t(text, 'sys.web.legacy.on')
					: t(text, 'sys.web.legacy.off')}</dd>
			</dl>
		</section>`}
	</div>`;
}
