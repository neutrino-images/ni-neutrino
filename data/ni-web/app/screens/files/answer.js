/* What this destination reads off an answer, in one place because all three of
   its screens read the same things off one.

   Nothing here reaches the network and nothing holds state. */


/**
 * A refusal out of whatever was thrown, with every member there.
 *
 * A rejection is unknown until it has been looked at, and everything the client
 * throws carries the box's own document under problem. What comes back is the
 * shape the frame draws a refusal in, so a screen has one test and not four.
 *
 * @param {unknown} caught
 * @returns {{ status: number, type: string, title: string, detail: string }}
 */
export function problemOf(caught) {
	const error = /** @type {{ problem?: { status?: unknown, type?: unknown, title?: unknown, detail?: unknown } } | null} */ (caught);
	const body = (error && error.problem) || {};
	return {
		status: typeof body.status === 'number' ? body.status : 0,
		type: typeof body.type === 'string' ? body.type : '',
		title: typeof body.title === 'string' ? body.title : '',
		detail: typeof body.detail === 'string' ? body.detail : '',
	};
}

/* The one refusal in this destination that is the design working rather than
   something going wrong.

   Two files inside the released directories are reached by no file route at all,
   whoever is asking: the store each of the two web servers keeps of who may reach
   this box. Reading one hands over a hash to be attacked at leisure, writing one
   hands over the box for good. They stay in the listing, because what is refused
   is the reaching and not the knowing.

   Recognised by the name the box gives the refusal and never by a list of file
   names kept here. */
/** @param {{ type: string }} problem @returns {boolean} */
export function isOutOfReach(problem) {
	return problem.type === '/errors/access-store';
}
