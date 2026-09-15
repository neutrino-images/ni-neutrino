/* What the box is writing to a disc right now, and the two ways of stopping one of them.
   Three things about this API decide the whole shape of this screen, and all three were
   measured against a running box.

   Several at once is the ordinary case. The box records as many channels as its tuners and
   its own ceiling allow, and the shift it keeps of what it is showing is one more of them.

   What names one is its number, and nothing else about it. The number is the timer event
   the recording carries, handed out once and never twice while the daemon runs, and it is
   what the route that ends one takes. Neither obvious alternative works: two recordings of
   one channel are normal, and the file a shift writes is renamed the moment somebody keeps
   it.

   The event stream reports the first start and the last stop and nothing in between.
   record-start and record-stop are made out of one latch that says whether the box is
   recording at all (src/zapit/zapit.cpp, SetRecordMode returns early when the state it is
   handed is the state it is in), so with two running, the second start and the first stop
   are both silent. Measured: starting a recording beside a running timeshift produced no
   record-start at all, and ending one of the two produced no event of any kind.

   So nothing here is written forward from an event. This screen asks again on its own while
   it is open, and again after each of its own writes, because the box answers all three
   with accepted. */
import { html, useState, useEffect } from '../../runtime.js';
import * as store from '../../store.js';
import * as session from '../../session.js';
import { t } from '../../i18n.js';
import text from './recordings.text.js';
import { clock, duration, bytes, isTimeshift, recordingMark } from '../../fmt.js';
import { State } from '../../ui/state.js';
import { Button } from '../../ui/button.js';
import { StateChip } from '../../ui/dot.js';
import { Table } from '../../ui/table.js';
import { Dialog } from '../../ui/dialog.js';
import { Select } from '../../ui/select.js';
import { toast } from '../../ui/toast.js';

export const css = '/app/screens/timers/recordings.css';
/** @returns {string} the sentence the frame draws under the name of this screen */
export function lead() { return t(text, 'rec.lead'); }


/* How often the list is asked for again while somebody is looking at it.

   A poll on a screen of this box wants a reason, and the reason is the latch above: between
   two recordings there are transitions the box never announces. Five seconds costs one list
   and one stat per running recording, the store folds two asks that overlap into one, and
   it stops with the screen. */
const RELOAD_MS = 5000;

/* How long a row says it is being stopped before it goes back to offering the control
   again.

   There is nothing to wait for: the box answers accepted, what actually ends the
   writing is a loop nothing here can hear from, and no event announces the end of one
   recording out of two. So the mark is worn for a while and then given up. */
const ENDING_MS = 15000;

// The lengths the control below offers, in hours. Which one it opens on is the
// box's own setting, not one of these.
const kHourChoices = [1, 2, 3, 4];

/* What the control opens on where the box will not say how long it normally
   records. A build without that settings section refuses the read (measured:
   501 on the generic build), and the control still has to open on something.
   It is the value shown in the control and changeable before anything is sent,
   not a length used behind anybody's back. */
const kFallbackHours = 2;

const kSecondsPerHour = 3600;

/**
 * The sentence over the list and the word in the class beside it. Pulled out of the drawing
 * so the count and the mark cannot come to say two different things, and so both can be
 * asserted without a box.
 *
 * @param {number} count
 * @returns {{ mark: string, said: string }}
 */
export function summaryOf(count) {
	if (count <= 0)
		return { mark: 'rec-none', said: t(text, 'rec.empty') };
	if (count === 1)
		return { mark: 'rec-one', said: t(text, 'rec.count.one') };
	return { mark: 'rec-many', said: t(text, 'rec.count.many', { count: count }) };
}

/**
 * How a recording was begun, in words, and the word the box sent for one this page has no
 * word for. The document states no closed set here on purpose, so a third way of beginning
 * one arrives as itself instead of as a blank.
 *
 * @param {string} startedBy
 * @returns {string}
 */
export function startedByWord(startedBy) {
	if (startedBy === 'timer')
		return t(text, 'rec.by.timer');
	if (startedBy === 'immediate')
		return t(text, 'rec.by.immediate');
	return t(text, 'rec.by.other', { word: startedBy });
}

/**
 * How long this box records for when nobody says, out of the section it keeps that
 * in. A section this build does not carry, or one that could not be read, answers
 * nought.
 *
 * @param {Api.SettingValueList | null} settings
 * @returns {number}
 */
export function boxHours(settings) {
	const rows = (settings && settings.items) || [];
	for (const row of rows) {
		if (row.id !== 'record_hours')
			continue;
		const hours = Number(row.value);
		return Number.isFinite(hours) && hours > 0 ? hours : 0;
	}
	return 0;
}

/**
 * The lengths offered, with the box's own among them wherever it is not one of the
 * four written above. Its value is never dropped for being unusual: a box set to
 * record for six hours means six.
 *
 * @param {number} preferred
 * @returns {{ value: string, label: string }[]}
 */
export function hourOptions(preferred) {
	const all = kHourChoices.slice();
	if (preferred > 0 && all.indexOf(preferred) === -1)
		all.push(preferred);
	all.sort(function (a, b) { return a - b; });
	return all.map(function (one) {
		return {
			value: String(one),
			label: one === 1 ? t(text, 'rec.hours.one') : t(text, 'rec.hours.many', { count: one }),
		};
	});
}

/**
 * What the box calls this channel, asked for by the identifier the recording carries. A
 * recording names its channel and nothing more; a channel the list does not hold answers as
 * its identifier rather than as a blank.
 *
 * @param {{ id: string }} props
 * @returns {Web.Drawn}
 */
export function ChannelName(props) {
	const [shot, setShot] = useState(function () {
		return store.read('GET', '/api/v1/channels/{id}', { params: { id: props.id } });
	});

	useEffect(function () {
		return store.watch('GET', '/api/v1/channels/{id}', { params: { id: props.id } }, setShot);
	}, [props.id]);

	const one = shot.data;
	return html`<span class="rec-channel">${(one && one.name) ? one.name : props.id}</span>`;
}

/** @returns {Web.Drawn} */
export default function Recordings() {
	const [shot, setShot] = useState(function () {
		return store.read('GET', '/api/v1/recordings');
	});
	const [current, setCurrent] = useState(function () {
		return store.read('GET', '/api/v1/channels/current');
	});
	const [settings, setSettings] = useState(function () {
		return store.read('GET', '/api/v1/settings/{section}', { params: { section: 'recording' } });
	});
	/* Only a way to ask for another draw. How long a recording has been running
	   and how long a row has been saying it is stopping are both worked out
	   from the clock where they are drawn, and the clock is not state this page
	   keeps: a recording that began after the last redraw would otherwise be
	   drawn as having begun in the future. */
	const bump = useState(0)[1];
	// The numbers this screen has asked the box to stop, each with the moment
	// its mark is given up again. See ENDING_MS.
	const [ending, setEnding] = useState(/** @type {{ id: number, until: number }[]} */([]));
	const [asked, setAsked] = useState(/** @type {Api.Recording | null} */(null));
	const [hours, setHours] = useState(0);

	/* Asked for when a control is pressed and not before it. This screen used to
	   read what the request was granted and draw one of two sentences over a set
	   of controls it then withheld, which is the page refusing on the box's behalf
	   before anybody has asked for anything. The sheet closing without a sign in
	   rejects and nothing is said: that is the person saying no. */
	/**
	 * @param {() => void} act
	 * @returns {void}
	 */
	function whenAllowed(act) {
		session.requireWrite().then(act, function () { });
	}

	useEffect(function () {
		const stopList = store.watch('GET', '/api/v1/recordings', null, setShot);
		const stopCurrent = store.watch('GET', '/api/v1/channels/current', null, setCurrent);
		const stopSettings = store.watch('GET', '/api/v1/settings/{section}',
			{ params: { section: 'recording' } }, setSettings);
		const tick = window.setInterval(function () {
			bump(function (n) { return n + 1; });
			store.reload('GET', '/api/v1/recordings');
		}, RELOAD_MS);

		return function () {
			stopList();
			stopCurrent();
			stopSettings();
			window.clearInterval(tick);
		};
	}, []);

	const rows = (shot.data && shot.data.items) || [];
	const shift = rows.filter(function (one) { return one.timeshift; })[0] || null;
	const channel = current.data;
	const preferred = boxHours(settings.data);
	const chosen = hours > 0 ? hours : (preferred > 0 ? preferred : kFallbackHours);
	const summary = summaryOf(rows.length);

	/**
	 * @param {Api.Recording} one
	 * @returns {boolean}
	 */
	function stopping(one) {
		const at = Date.now();
		for (const mark of ending) {
			if (mark.id === one.id && mark.until > at)
				return true;
		}
		return false;
	}

	const anyStopping = rows.filter(stopping).length > 0;

	/* What the box was asked is not what the box has done, so every write here is
	   followed by asking for the list again, and the tick above carries the rest.
	   Nothing is written forward into the store. */
		/**
		 * @param {string} said
		 * @returns {void}
		 */
	function afterWrite(said) {
		toast(said);
		store.reload('GET', '/api/v1/recordings');
	}

	/**
	 * @param {unknown} caught
	 * @returns {void}
	 */
	function refused(caught) {
		// A caught value is not known to be an error at all until it has been
		// looked at, and what a refusal says about itself is the box's sentence
		// and never one invented here.
		const failure = /** @type {{ problem?: { title?: string, detail?: string } } | null} */ (caught);
		const problem = failure && failure.problem ? failure.problem : null;
		toast((problem && (problem.detail || problem.title)) || t(text, 'rec.need.write'), 'bad');
	}

	/**
	 * @param {Api.Recording} one
	 * @returns {void}
	 */
	function stopOne(one) {
		setEnding(function (was) {
			// The marks that have run out go with it, so a screen somebody
			// leaves open all evening does not keep one per press.
			const at = Date.now();
			const live = was.filter(function (mark) { return mark.until > at; });
			return live.concat([{ id: one.id, until: at + ENDING_MS }]);
		});
		/* The shift is ended through the route that is about the shift and not
		   through its number, although its number would end the writing too:
		   that route is also what stops the box beginning the next one by
		   itself, which is what somebody pressing this means
		   (src/coreapi/recordings.h). */
		const asking = one.timeshift
			? store.write('DELETE', '/api/v1/recordings/timeshift', {
				touches: ['/api/v1/recordings'],
			})
			: store.write('DELETE', '/api/v1/recordings/{id}', {
				params: { id: one.id },
				touches: ['/api/v1/recordings'],
			});

		asking.then(function () {
			afterWrite(t(text, 'rec.stop.asked', { what: whatOf(one) }));
		}, refused);
	}

	function startShift() {
		store.write('POST', '/api/v1/recordings/timeshift', {
			touches: ['/api/v1/recordings'],
		}).then(function () {
			afterWrite(t(text, 'rec.shift.start.asked'));
		}, refused);
	}

	/* Recording now is making a timer whose start has come, which is the one shape of
	   it the daemon runs and is why the kind carries a name of its own
	   (src/coreapi/timers.cpp). The end is the caller's: the daemon takes a recording
	   that ends after it begins and nothing else. No title is sent, because what the
	   guide called the programme is something the box reads for itself. */
	function startHere() {
		if (!channel)
			return;
		const from = Math.floor(Date.now() / 1000);
		store.write('POST', '/api/v1/timers', {
			body: {
				kind: 'immediate-record',
				channel_id: channel.id,
				start: from,
				stop: from + chosen * kSecondsPerHour,
			},
			touches: ['/api/v1/recordings', '/api/v1/timers'],
		}).then(function () {
			afterWrite(t(text, 'rec.start.asked', { channel: channel.name }));
		}, refused);
	}

	/**
	 * @param {Api.Recording} one
	 * @returns {string}
	 */
	function whatOf(one) {
		if (one.timeshift)
			return t(text, 'rec.kind.timeshift');
		return one.title || t(text, 'rec.title.none');
	}

	const columns = [
		{
			id: 'state', label: t(text, 'rec.col.state'), sortable: false, mono: false,
			/* A pill, so that what is running is a shape and not only a word.
			   The timeshift wears the running outline and a recording wears
			   none: the red is the mark inside the pill and putting it round
			   the word as well would spend it twice on one row. */
			cell: function (/** @type {Api.Recording} */ one) {
				return html`<${StateChip}
					kind=${recordingMark(one)}
					tone=${isTimeshift(one) ? 'good' : ''}
					word=${one.timeshift ? t(text, 'rec.kind.timeshift') : t(text, 'rec.kind.recording')} />`;
			},
		},
		{
			id: 'channel', label: t(text, 'rec.col.channel'), sortable: false, mono: false,
			cell: function (/** @type {Api.Recording} */ one) { return html`<${ChannelName} id=${one.channel_id} />`; },
		},
		{
			id: 'title', label: t(text, 'rec.col.title'), sortable: false, mono: false, wide: true,
			cell: function (/** @type {Api.Recording} */ one) {
				return html`<span>
					${one.title || t(text, 'rec.title.none')}
					<span class="hint">${startedByWord(one.started_by)}</span>
				</span>`;
			},
		},
		{
			id: 'since', label: t(text, 'rec.col.since'), sortable: false, mono: false,
			cell: function (/** @type {Api.Recording} */ one) { return clock(one.start); },
		},
		{
			id: 'running', label: t(text, 'rec.col.running'), sortable: false, mono: false,
			cell: function (/** @type {Api.Recording} */ one) { return duration(Math.floor(Date.now() / 1000) - one.start); },
		},
		{
			id: 'size', label: t(text, 'rec.col.size'), sortable: false, mono: false,
			/* Absent and nought are two different answers here and the shape
			   says so: nought is what a recording begun a moment ago weighs,
			   and an absent size is a file that could not be reached at all. */
			cell: function (/** @type {Api.Recording} */ one) {
				return one.size === undefined ? t(text, 'rec.size.unknown') : bytes(one.size);
			},
		},
		{
			id: 'path', label: t(text, 'rec.col.path'), sortable: false, mono: true,
			cell: function (/** @type {Api.Recording} */ one) { return html`<span title=${one.path}>${one.path}</span>`; },
		},
		{
			id: 'act', label: t(text, 'rec.col.act'), sortable: false, mono: false,
			cell: function (/** @type {Api.Recording} */ one) {
				const busy = stopping(one);
				/* The shift and a recording carry different marks as well as
				   different words, because which of the two a control acts on
				   is the whole of what can go wrong here and a position in the
				   list is not a name for it. */
				return html`<${Button}
					class=${one.timeshift ? 'rec-stop rec-stop-shift' : 'rec-stop rec-stop-rec'}
					disabled=${busy}
					onClick=${function () { whenAllowed(function () { setAsked(one); }); }}>
					${busy
						? t(text, 'rec.stopping')
						: (one.timeshift ? t(text, 'rec.stop.shift') : t(text, 'rec.stop'))}
				<//>`;
			},
		},
	];

	return html`<section class="rec">
		${/* Only over a list that was read. A box whose recordings could not be
		     read is not a box recording nothing, and a sentence saying so over
		     a refusal would be the page stating the opposite of what happened. */
			shot.data
				? html`<p class=${'rec-summary ' + summary.mark}>${summary.said}</p>`
				: null}

		<${State}
			problem=${shot.error ? shot.error.problem : null}
			phase=${shot.phase}
			onRetry=${function () { store.reload('GET', '/api/v1/recordings'); }}>
			${rows.length
				? html`<${Table}
					columns=${columns}
					rows=${rows}
					rowKey=${function (/** @type {Api.Recording} */ one) { return one.id; }} />`
				: null}
		<//>

		${anyStopping ? html`<p class="hint rec-accepted">${t(text, 'rec.accepted')}</p>` : null}

		<div class="rec-acts">
			<${Select}
				id="rec-hours"
				label=${t(text, 'rec.hours')}
				value=${String(chosen)}
				hint=${preferred > 0 ? t(text, 'rec.hours.box') : t(text, 'rec.hours.unknown')}
				options=${hourOptions(preferred)}
				onChange=${function (/** @type {Web.On<HTMLSelectElement>} */ event) { setHours(Number(event.currentTarget.value)); }} />
			<p class="rec-on">${channel
				? t(text, 'rec.start.on', { channel: channel.name })
				: t(text, 'rec.start.nochannel')}</p>
			<${Button}
				class="rec-start"
				primary=${true}
				disabled=${!channel}
				onClick=${function () { whenAllowed(startHere); }}>${t(text, 'rec.start')}<//>
			${shift
				? null
				: html`<${Button} class="rec-shift" onClick=${function () { whenAllowed(startShift); }}>
					${t(text, 'rec.shift.start')}<//>`}
		</div>

		<${Dialog}
			open=${!!asked}
			title=${t(text, 'rec.stop.ask.title')}
			confirmLabel=${t(text, 'rec.stop')}
			onCancel=${function () { setAsked(null); }}
			onConfirm=${function () {
				const one = asked;
				setAsked(null);
				if (one)
					stopOne(one);
			}}>
			<p>${asked ? t(text, 'rec.stop.ask.body', { what: whatOf(asked) }) : ''}</p>
			${asked ? html`<p><${ChannelName} id=${asked.channel_id} /></p>` : null}
		<//>
	</section>`;
}
