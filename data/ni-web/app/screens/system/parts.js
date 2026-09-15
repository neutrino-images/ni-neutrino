/* The pieces the five screens of this destination share, and nothing beyond them.

   They live here rather than beside the frame's own building blocks because a block
   hoisted before a second screen wants it is a file with ten writers.

   THE TWO THINGS THIS FILE IS REALLY FOR.

   The first is asking. Four addresses of this destination cost more than an error
   message when they are hit by mistake: standby, reboot, shutdown and the program
   restart all take the picture off the screen, and two of them take the box with it. So
   a control for one of those asks first, with a sentence that says what happens, and
   the sentence is the catalogue's and never a path.

   The second is that nothing here says anything in advance about a session. What wants
   a session says so at the moment it is asked for: the control raises the sign in sheet
   and the call goes through behind it, so a person who never presses anything is never
   told about a session at all. */

import { html, useState } from '../../runtime.js';
import * as session from '../../session.js';
import { t } from '../../i18n.js';
import { Dialog } from '../../ui/dialog.js';
import text from './system.text.js';

/**
 * Runs something once the request carries enough to be allowed it.
 *
 * The sheet is raised where the session is short and the call follows the sign in; a
 * person who closes the sheet has said no, which is not a failure and is not reported
 * as one.
 *
 * @param {'write'|'system'|undefined} needs
 * @param {() => void} act
 * @returns {void}
 */
export function withSession(needs, act) {
	if (needs === undefined) {
		act();
		return;
	}
	session.requireLevel(needs).then(act, function () {
		// The sheet was closed. Nothing was asked of the box and nothing is
		// said about it: a sentence here would be the page explaining a
		// decision the person just made.
	});
}

/**
 * What the box said about a call that did not go through, as one sentence.
 *
 * The box's own words and never a sentence invented from the status: what came back
 * says what was refused, and anything written here would say what this page guessed was
 * refused.
 *
 * @param {unknown} caught
 * @returns {string}
 */
export function failureText(caught) {
	const failed = /** @type {{ problem?: { title?: unknown, detail?: unknown } } | null} */ (caught);
	const problem = (failed && failed.problem) ? failed.problem : null;
	const title = (problem && typeof problem.title === 'string') ? problem.title : '';
	const detail = (problem && typeof problem.detail === 'string') ? problem.detail : '';
	if (title !== '' && detail !== '')
		return title + ': ' + detail;
	if (title !== '')
		return title;
	if (detail !== '')
		return detail;
	return t(text, 'sys.answer.failed');
}

/**
 * A control that asks the box for something.
 *
 * @param {{ label: string, needs?: 'write'|'system', primary?: boolean, busy?: boolean, onClick: () => void }} props
 *   needs is the level the address behind it wants, and a control without one
 *   is a control that asks the box for nothing
 * @returns {Web.Drawn}
 */
export function Act(props) {
	return html`<button
		type="button"
		class=${props.primary ? 'btn primary' : 'btn'}
		disabled=${props.busy === true}
		onClick=${function () { withSession(props.needs, props.onClick); }}>${props.label}</button>`;
}

/**
 * The same control for something that cannot be taken back, with the question in front
 * of it.
 *
 * The question says what happens and not which address is called. A button beside
 * seventy others that are spelt alike is not a deliberate grasp, and the sentence is
 * what turns the second press into one.
 *
 * The question comes first and the sign in second, so the two are never on the screen at
 * once: two sheets over each other are two focus traps, and the one underneath is the
 * one nobody gets out of.
 *
 * @param {{ label: string, question: string, needs?: 'write'|'system', busy?: boolean, onConfirm: () => void }} props
 * @returns {Web.Drawn}
 */
export function Ask(props) {
	const [open, setOpen] = useState(false);

	return html`<span class="sys-ask">
		<${Act}
			label=${props.label}
			busy=${props.busy}
			onClick=${function () { setOpen(true); }} />
		<${Dialog}
			open=${open}
			title=${props.label}
			confirmLabel=${props.label}
			onCancel=${function () { setOpen(false); }}
			onConfirm=${function () { setOpen(false); withSession(props.needs, props.onConfirm); }}>
			<p>${props.question}</p>
		<//>
	</span>`;
}
