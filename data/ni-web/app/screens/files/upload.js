/* Putting one file on the box.
 *
 * The ceiling is said before a file is picked and not after one is refused. The body
 * of a request reaches this server whole, so how large one may be is the transport's
 * ceiling on a body and there is no larger one behind it. A person who reaches for a
 * recording has to read that this is not the way recordings travel, and reading it
 * after a minute of sending is reading it too late.
 *
 * The number is written here because nothing the box answers states it, and it is
 * written once, beside the one call that runs into it. The box stays the authority: a
 * body over the ceiling is answered 413 whatever this file believes, so a server
 * configured differently makes this optimistic and never wrong.
 *
 * Nothing is replaced without being asked. The route refuses a name that is taken
 * unless the call says otherwise, and answers 409; replacing is a second call the
 * person makes, with the name they would be replacing in front of them.
 *
 * And why the answer is read and not only the failure: the route answers 201 for a
 * name that was free and 204 for one that was replaced, on purpose, so a caller can
 * tell the two apart.
 *
 * There is no progress bar, and that is a decision rather than an omission. The only
 * way a browser reports how much of a body has gone out is XMLHttpRequest, every call
 * this page places goes through one file, and a check refuses that name anywhere in
 * the application because the second token, the one retry and the shape of a refusal
 * all live in that one file. What is left is a ceiling of a mebibyte and a way to
 * stop it. */

import { html, useState, useRef } from '../../runtime.js';
import { api } from '../../api.js';
import { t } from '../../i18n.js';
import { bytes } from '../../fmt.js';
import { Button } from '../../ui/button.js';
import { toast } from '../../ui/toast.js';
import text from './files.text.js';
import { joinPath } from './paths.js';
import { problemOf } from './answer.js';

// What the transport takes, one mebibyte, as server.cpp sets it.
const CEILING = 1024 * 1024;

/**
 * @param {{ dir: string, onDone: () => void }} props
 * @returns {Web.Drawn}
 */
export function Upload(props) {
	const [chosen, setChosen] = useState(/** @type {File | null} */ (null));
	const [sending, setSending] = useState(false);
	// The two refusals this screen answers itself, kept apart from each other:
	// one is a file this page will not send, the other is a name the box will
	// not take without being told to.
	const [refused, setRefused] = useState('');
	const [taken, setTaken] = useState('');
	const [problem, setProblem] = useState(/** @type {string} */ (''));
	const control = useRef(/** @type {AbortController | null} */ (null));
	const field = useRef(/** @type {HTMLInputElement | null} */ (null));

	const limit = bytes(CEILING);

	function forget() {
		setChosen(null);
		setRefused('');
		setTaken('');
		setProblem('');
		if (field.current) {
			field.current.value = '';
		}
	}

	/** @param {Event} event */
	function pick(event) {
		const input = /** @type {HTMLInputElement} */ (event.currentTarget);
		const list = input.files;
		const file = (list && list.length > 0) ? list[0] : null;
		setTaken('');
		setProblem('');
		if (!file) {
			setChosen(null);
			setRefused('');
			return;
		}
		/* Refused here and never sent. A body over the ceiling is refused by
		   the transport after all of it has travelled, so sending it would
		   cost the person the whole upload to learn what the line above the
		   field already says. */
		if (file.size > CEILING) {
			setChosen(null);
			setRefused(t(text, 'files.upload.toobig', {
				name: file.name, size: bytes(file.size), limit: limit,
			}));
			return;
		}
		setChosen(file);
		setRefused('');
	}

	/** @param {boolean} replace */
	async function send(replace) {
		if (!chosen) {
			return;
		}
		const where = joinPath(props.dir, chosen.name);
		const stopper = new AbortController();
		control.current = stopper;
		setSending(true);
		setTaken('');
		setProblem('');
		try {
			/* The bytes as they are, under no type of this page's choosing:
			   the body of this route is the file and nothing wraps it
			   (ep_storage.cpp). The answer is handed over untouched so that
			   the status can be read off it, which is what tells a name that
			   was free from one that was replaced. */
			const answer = await api('PUT', '/api/v1/storage/file', {
				query: { path: where, overwrite: replace ? true : undefined },
				body: chosen,
				accept: 'response',
				signal: stopper.signal,
			});
			const made = answer && answer.status === 201;
			toast(t(text, made ? 'files.upload.created' : 'files.upload.replaced', { name: chosen.name }));
			forget();
			props.onDone();
		} catch (caught) {
			const said = problemOf(caught);
			if (said.status === 409) {
				setTaken(t(text, 'files.upload.taken', { name: chosen.name }));
			} else if (said.status === 413) {
				// The box's own ceiling, met although this page believed the
				// file was under it. Said in this page's words because the box
				// states the refusal and not the number.
				setProblem(t(text, 'files.upload.toolarge', { limit: limit }));
			} else {
				// Everything else as the box wrote it. A sentence invented
				// here would say what this page guessed the box refused.
				setProblem(said.detail || said.title);
			}
		} finally {
			control.current = null;
			setSending(false);
		}
	}

	function stop() {
		if (control.current) {
			control.current.abort();
		}
	}

	return html`<div class="files-panel">
		<h2>${t(text, 'files.upload.title')}</h2>
		<p class="files-hint" id="files-upload-ceiling">${t(text, 'files.upload.ceiling', { limit: limit })}</p>
		<div class="files-form">
			<label class="field">
				<span class="label">${t(text, 'files.upload.label')}</span>
				<input
					type="file"
					ref=${field}
					disabled=${sending}
					aria-describedby="files-upload-ceiling"
					onChange=${pick} />
			</label>
			${sending
				? html`<${Button} class="files-upload-stop" onClick=${stop}>${t(text, 'files.upload.cancel')}<//>`
				: html`<${Button}
					class="files-upload-send"
					primary=${true}
					disabled=${!chosen}
					onClick=${function () { send(false); }}>${t(text, 'files.upload.send')}<//>`}
		</div>
		${sending && chosen
			? html`<p class="files-hint" role="status">${t(text, 'files.upload.running', { name: chosen.name })}</p>`
			: null}
		${refused ? html`<p class="files-hint" role="alert">${refused}</p>` : null}
		${taken
			? html`<p class="files-hint" role="alert">
				${taken}${' '}
				<${Button} class="files-upload-replace" onClick=${function () { send(true); }}>${t(text, 'files.upload.replace')}<//>
			</p>`
			: null}
		${problem ? html`<p class="files-hint" role="alert">${problem}</p>` : null}
	</div>`;
}
