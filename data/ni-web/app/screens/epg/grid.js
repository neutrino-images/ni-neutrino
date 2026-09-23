/* The grid: channels down, time to the right, one bouquet at a time.

   The widest screen of the page, and the one that would be the most nodes if it
   drew what it holds: a channel list on this box runs to thousands of entries,
   and three hours of half hour programmes is six of them per channel. So two
   windows are kept, a vertical one over the rows and a horizontal one over the
   axis, and only what both of them hold is an element. Both are worked out in
   grid.model.js, out of a scroll position and a measured viewport, so what is on
   screen is a value that can be read without a browser.

   AND THE AXIS IS AS WIDE AS THE BOX IS. How wide a minute is drawn is measured
   and not fixed, so the whole three hour window stands inside the room there is
   at every width this screen is a grid at. A fixed axis is a box somebody pushes
   sideways with a thumb. Below the width the frame calls a telephone the grid is
   not drawn at all: the same rows go up the other way round, a strip of channels
   over the evening of the one that is chosen.

   WHAT THIS SCREEN ASKS FOR, AND WHY IN THAT ORDER. The guide answers a row per
   channel identifier and does not name the channel (src/httpd/ep/ep_epg.cpp), so
   the channels come first, out of the listing, which is also what puts them in
   the order the bouquet holds them. The grid is then asked for the channels of
   the window by name. Naming them rather than naming the bouquet is what keeps
   the box from reading the guide of two thousand channels nobody is looking at:
   that read costs it one pass per channel of the answer.

   AND THE ONE THING IT MUST NOT CONCLUDE. A page of the grid ends at the number
   of channels asked for or at the ceiling on the events of one page, whichever
   comes first, and the second ends it early without dropping anything. So a page
   holding fewer rows than were asked for is not on that account the last one,
   and the cursor is the only thing that says whether a walk is over. It is read
   in one place, grid.model.js:nextCursor.

   Nothing here writes to the box. Everything it asks for is answered to a caller
   granted Read, so there is no control on this screen that a visitor is
   refused. */
import { html, Fragment, useState, useEffect, useLayoutEffect, useRef, Link, route } from '../../runtime.js';
import { api } from '../../api.js';
import * as store from '../../store.js';
import { isAborted, problemHref } from '../../problem.js';
import * as boxEvents from '../../events.js';
import { t } from '../../i18n.js';
import { clock, dayAndClock, duration as spanOf } from '../../fmt.js';
import { hrefFor } from '../../nav.js';
import { State } from '../../ui/state.js';
import { Button } from '../../ui/button.js';
import { Dot, dotParts } from '../../ui/dot.js';
import { Field } from '../../ui/field.js';
import { Sheet } from '../../ui/sheet.js';
import { elapsedShare } from '../../ui/onair.js';
import { Logo } from '../../ui/logo.js';
import { holdingBouquet, timerHref } from './schedule.js';
import text from './grid.text.js';
import * as model from './grid.model.js';

export const css = '/app/screens/epg/grid.css';
/** @returns {string} the sentence the frame draws under the name of this screen */
export function lead() { return t(text, 'grid.head'); }


/* The width the frame calls a telephone (app/css/shell.css), which is the width
   this screen stops being a grid at.

   A grid is an axis, an axis is width, and there is none down there: three hours
   across four hundred pixels is a lane nobody can read and a page somebody
   pushes sideways with a thumb. So under this width the grid is not drawn small,
   it is not drawn at all: the same rows go up the other way round. */
const GRID_FROM_PX = 600;

/* How often the clock is looked at, which is not how often the mark of the
   moment moves. That moves once a minute; this only asks whether the minute
   has turned, so that the mark catches up soon after whoever was typing has
   stopped rather than standing still for another whole minute. */
const TICK_MS = 15000;

/* How long the page waits after the box says the guide moved before it asks
   again. One of those arrives per channel whose current or next event changed,
   and a grid that asked on each would ask twenty times a minute for an answer
   that costs the box a read of the guide per row. */
const SETTLE_MS = 1500;

/* What is left under the grid, so that the last row is not against the edge of
   the window. */
const BOTTOM_PX = 16;

function nowSeconds() {
	return Math.floor(Date.now() / 1000);
}

/* The clock as this screen holds it. Whole minutes, because that is what the
   flag says and what the mark of the moment is drawn to, and because a state
   that changes every second is a screen that draws every second. */
function nowMinute() {
	return Math.floor(Date.now() / 60000);
}

/**
 * WHETHER THE PAGE IS IN THE MIDDLE OF SOMETHING A DRAW WOULD INTERRUPT.
 *
 * The mark of the moment moves by itself, so this screen draws without anybody
 * asking it to, and a draw is the one thing a control being used does not
 * survive: an open select shuts, a half typed word goes back to what the field
 * was last given, and a sheet drawn again loses the focus inside it. So the
 * clock waits for the next look.
 *
 * @returns {boolean}
 */
export function takingInput() {
	const on = document.activeElement;
	if (on !== null) {
		const tag = on.tagName;
		if (tag === 'INPUT' || tag === 'SELECT' || tag === 'TEXTAREA') {
			return true;
		}
		if (/** @type {HTMLElement} */ (on).isContentEditable) {
			return true;
		}
	}
	// One thing lies over this page and it is always the same element
	// (app/ui/sheet.js), so one class answers for every sheet there is.
	return document.querySelector('.sheet') !== null;
}

/**
 * One event, named the way this screen compares two of them.
 *
 * Both halves, because the guide files one identifier under every showing of an
 * event and the moment is which of them is meant (src/httpd/ep/ep_epg.cpp).
 *
 * @param {Api.Event} event
 * @returns {string}
 */
export function eventKey(event) {
	return event.id + '@' + event.start;
}

/**
 * Which bouquet the grid is for: the one the address names, then the one the
 * channel that is playing sits in, and the one the box would open itself where
 * neither says.
 *
 * WHY WHAT IS PLAYING DECIDES IT. Somebody opening the guide with nothing said
 * is looking outwards from where they are, and where they are is the channel on
 * the television. Opening on the first bouquet instead put them in a list that
 * usually does not hold it.
 *
 * WHICH ONE, WHERE SEVERAL HOLD IT, is one rule and lives in one place
 * (holdingBouquet in schedule.js), because the channel schedule opens on the
 * same footing.
 *
 * @param {Api.Bouquet[]} bouquets
 * @param {string} asked
 * @param {Api.Bouquet[]} holding the bouquets that hold what is playing, in the
 *        box's own order, and empty where nothing is playing or nothing holds it
 * @returns {number}
 */
export function chosenBouquet(bouquets, asked, holding) {
	const named = Number(asked);
	if (asked !== '' && Number.isFinite(named)) {
		for (const one of bouquets) {
			if (one.id === named) {
				return one.id;
			}
		}
	}
	const home = holdingBouquet(holding);
	if (home !== 0) {
		return home;
	}
	/* The first that is not hidden. A hidden bouquet is one somebody took out
	   of the lists on the screen, and opening on it here would put it back. */
	for (const one of bouquets) {
		if (!one.hidden) {
			return one.id;
		}
	}
	const first = bouquets[0];
	return first === undefined ? 0 : first.id;
}

/**
 * The row of hours, and the flag of the moment standing in it.
 *
 * WHY THE FLAG IS DRAWN IN HERE AND NOT ON THE LINE IT BELONGS TO. The row of
 * hours is sticky at the top of the box and carries a z-index, which makes it a
 * stacking context: everything in the body below is behind it whatever it asks
 * for, so a flag hanging off the top of the line would slide under the hours the
 * moment somebody scrolled down. It also needs no idea of how wide the column of
 * names is, because the axis begins where that column ends.
 *
 * @param {{ from: number, to: number, left: number, right: number,
 *          each: number, now: number, at: number }} props
 * @returns {Web.Drawn}
 */
function Axis(props) {
	const marks = model.ticks(props.from, props.to, props.each);
	return html`<div class="grid__axis" style=${{ width: model.axisWidth(props.from, props.to, props.each) + 'px' }}>
		${marks.map(function (mark) {
			// The label hangs to the right of its mark, so a mark just off the
			// left edge still has a label on screen.
			if (mark.left + 80 <= props.left || mark.left >= props.right) {
				return null;
			}
			return html`<span class="grid__tick mono" key=${mark.at} style=${{ left: mark.left + 'px' }}>${clock(mark.at)}</span>`;
		})}
		${props.now < 0 ? null : html`<span class="grid__nowflag mono" style=${{ left: props.now + 'px' }}>
			<span class="sr">${t(text, 'grid.now')} </span>${clock(props.at)}
		</span>`}
	</div>`;
}

/**
 * One channel, as the column on the left says it.
 *
 * THE ONE THE BOX IS PLAYING CARRIES A MARK. The accent and never the red: red
 * on this page is the box writing to a disc. The mark is asked for by name
 * (app/ui/dot.js) so that no screen here decides which colour that is.
 *
 * THE WORD THAT BELONGS TO IT IS NOT IN THE CELL, and that is the one place on
 * this page where a mark and its word are drawn apart. The column is two hundred
 * pixels wide and a hundred and thirty two on a tablet; measured, a word beside
 * the dot leaves ninety three pixels for the name at the wide width and sixty
 * nine at the narrow, which turned a ten letter channel into three letters and
 * an ellipsis. So the word stands once under the grid, and the cell carries it
 * again for a reader who is being read to.
 *
 * @param {{ row: model.Row, onAir: boolean }} props
 * @returns {Web.Drawn}
 */
function Name(props) {
	const channel = props.row.channel;
	const place = channel.number > 0 ? String(channel.number) : t(text, 'grid.nonumber');
	const air = dotParts('onair', t(text, 'grid.playing'));
	return html`<div class="grid__name" data-channel=${channel.id}>
		<span class="grid__place mono">${place}</span>
		<${Logo} channel=${channel} size="sm" />
		<span class="grid__channel">${channel.name}</span>
		${props.onAir ? html`<span class="grid__air">
			<span class=${air.className} aria-hidden="true"></span>
			<span class="sr">${air.word}</span>
		</span>` : null}
	</div>`;
}

/**
 * The lane of one channel: what the horizontal window holds of its evening.
 *
 * @param {{ row: model.Row, from: number, to: number, left: number, right: number,
 *          each: number, now: number, at: number, picked: string,
 *          onPick: (row: model.Row, event: Api.Event) => void }} props
 * @returns {Web.Drawn}
 */
function Lane(props) {
	const row = props.row;
	const width = model.axisWidth(props.from, props.to, props.each);
	const events = row.events;

	/* Three states and not two. A row whose guide has not arrived and a row the
	   guide holds nothing for look the same on screen if they are both drawn
	   empty, and one of them is about to fill itself in. */
	if (events === null) {
		return html`<div class="grid__lane" style=${{ width: width + 'px' }}>
			<span class="grid__wait">${t(text, 'grid.waiting')}</span>
		</div>`;
	}

	const drawn = model.blocks(events, props.from, props.to, props.left, props.right, props.each);
	/* HOW FAR THE PROGRAMME ON NOW HAS COME, DRAWN BESIDE ITS BOX AND NOT
	   INSIDE IT. Inside, its left edge would be the box's padding edge, which is
	   three pixels in from where the programme actually begins, and a bar that
	   begins three pixels late ends three pixels past the one line it exists to
	   meet.

	   And it is worked out from where the line stands rather than from a share of
	   the box: an event that began before this window is drawn clipped at the
	   left edge, so a share of what is drawn is not the share that has run. */
	const running = drawn.find(function (block) { return model.isOnAir(block.event, props.at); });

	return html`<div class="grid__lane" style=${{ width: width + 'px' }}>
		${events.length === 0 ? html`<span class="grid__none">${t(text, 'grid.none')}</span>` : null}
		${drawn.map(function (block) {
			const ends = block.event.start + block.event.duration;
			const key = eventKey(block.event);
			/* What is on air right now, marked where it is drawn. One block of
			   a row at most, which is the same thing the guide answers for one
			   channel asked what it is showing now. */
			const onAir = model.isOnAir(block.event, props.at);
			return html`<button
				type="button"
				key=${key}
				class=${'grid__event' + (onAir ? ' is-onair' : '') + (props.picked === key ? ' is-picked' : '')}
				data-channel=${row.channel.id}
				aria-pressed=${props.picked === key ? 'true' : 'false'}
				aria-label=${t(text, onAir ? 'grid.event.onair' : 'grid.event', {
					title: block.event.title,
					channel: row.channel.name,
					start: clock(block.event.start),
					end: clock(ends),
					percent: elapsedShare(block.event, props.at)
				})}
				style=${{ left: block.left + 'px', width: block.width + 'px' }}
				onClick=${function () { props.onPick(row, block.event); }}>
				<span class="grid__clock mono">${clock(block.event.start)}</span>
				<span class="grid__title">${block.event.title}</span>
			</button>`;
		})}
		${running === undefined || props.now < 0
			? null
			: html`<span class="grid__gone" aria-hidden="true" style=${{
				left: running.left + 'px',
				width: Math.max(0, Math.min(running.width, props.now - running.left)) + 'px'
			}}></span>`}
		${row.truncated
			? html`<span class="grid__cut" title=${t(text, 'grid.cut.hint')}>${t(text, 'grid.cut')}</span>`
			: null}
	</div>`;
}

/**
 * THE STRIP OF CHANNELS THE FOLDED GRID IS CHOSEN FROM.
 *
 * It runs off the side, and what says there is more is the chip at the edge
 * being cut, which the frame's own .chipbar leaves room for (app/css/shell.css).
 * A bar three pixels high under a row of pills says nothing the cut pill does
 * not say better, and on a touch screen there is no bar to see anyway.
 *
 * AND IT PAGES AS IT IS PUSHED. A strip that stopped at the hundred channels the
 * first answer carried would be a strip that ends where the bouquet does not,
 * and the nineteen hundred behind it would be reachable by nothing on this
 * screen. The rule is the wide grid's own, turned ninety degrees: ask for the
 * next page while there are still two chips between the thumb and the end.
 *
 * Names and no pictures. A bouquet is a hundred channels in hand, a picture
 * apiece is a hundred requests a telephone makes before it has drawn anything.
 *
 * @param {{ rows: model.Row[], chosen: string, hasMore: boolean,
 *           onPick: (id: string) => void, onEnd: () => void }} props
 * @returns {Web.Drawn}
 */
function Strip(props) {
	const strip = useRef(/** @type {HTMLElement | null} */ (null));

	/* The chosen chip brought into view, because it is chosen from two places
	   and one of them is a sheet: a strip showing a selection that is four
	   hundred pixels off the side is a strip saying nothing was chosen. */
	useEffect(function () {
		const box = strip.current;
		if (box === null || props.chosen === '') {
			return;
		}
		const chip = box.querySelector('[data-channel="' + CSS.escape(props.chosen) + '"]');
		if (chip !== null) {
			chip.scrollIntoView({ inline: 'center', block: 'nearest' });
		}
	}, [props.chosen]);

	return html`<div
		class="chipbar grid__strip"
		ref=${strip}
		role="group"
		aria-label=${t(text, 'grid.channels')}
		onScroll=${function (/** @type {Event} */ e) {
			const box = /** @type {HTMLElement} */ (e.currentTarget);
			if (model.wantsMoreAlong(box, props.hasMore)) {
				props.onEnd();
			}
		}}>
		${props.rows.map(function (row) {
			const mine = row.channel.id === props.chosen;
			return html`<button
				type="button"
				key=${row.channel.id}
				class="chip grid__chan"
				data-channel=${row.channel.id}
				aria-pressed=${mine ? 'true' : 'false'}
				onClick=${function () { props.onPick(row.channel.id); }}>${row.channel.name}</button>`;
		})}
	</div>`;
}

/**
 * A CHANNEL BY NAME, WHICH IS WHAT A STRIP IS NO GOOD FOR.
 *
 * Pushing a thumb through two thousand six hundred chips is not an act of
 * choosing, so under the strip there is the other way: type a piece of the name.
 * The list it searches is the bouquet whole and not the page in hand, and the
 * rest of the bouquet is walked while this is open. That walk is the expensive
 * one, twenty six reads of the listing on the largest bouquet this box ships,
 * and it is here rather than on the first paint because here somebody asked for
 * it.
 *
 * @param {{ open: boolean, rows: model.Row[], chosen: string, walking: boolean,
 *           onPick: (id: string) => void, onClose: () => void }} props
 * @returns {Web.Drawn}
 */
function Finder(props) {
	const [query, setQuery] = useState('');
	const found = model.matching(props.rows, query, model.PICK_AT_ONCE);

	return html`<${Sheet} open=${props.open} onClose=${props.onClose} label=${t(text, 'grid.pick')}>
		<h2>${t(text, 'grid.pick')}</h2>
		<${Field}
			label=${t(text, 'grid.pick.find')}
			value=${query}
			onInput=${function (/** @type {{ currentTarget: HTMLInputElement }} */ e) {
				setQuery(e.currentTarget.value);
			}} />
		${props.walking ? html`<p class="note" role="status">${t(text, 'grid.more')}</p>` : null}
		${found.items.length === 0
			? html`<p class="note">${t(text, 'grid.pick.none')}</p>`
			: html`<ul class="grid__found">
				${found.items.map(function (row) {
					return html`<li key=${row.channel.id}>
						<button
							type="button"
							class="btn"
							data-channel=${row.channel.id}
							aria-pressed=${row.channel.id === props.chosen ? 'true' : 'false'}
							onClick=${function () { props.onPick(row.channel.id); }}>${row.channel.name}</button>
					</li>`;
				})}
			</ul>`}
		${found.more ? html`<p class="note">${t(text, 'grid.pick.cut')}</p>` : null}
	<//>`;
}

/**
 * One channel's evening, down the page instead of across it.
 *
 * The same three things a lane carries and in the same order: when it starts,
 * what it is, and whether it is the one on now. What a lane says with a position
 * this says with a line of its own.
 *
 * @param {{ row: model.Row, at: number, picked: string,
 *          onPick: (row: model.Row, event: Api.Event) => void }} props
 * @returns {Web.Drawn}
 */
function Slots(props) {
	const row = props.row;
	const events = row.events;

	if (events === null) {
		return html`<p class="note">${t(text, 'grid.waiting')}</p>`;
	}
	if (events.length === 0) {
		return html`<${State} empty=${t(text, 'grid.none')} />`;
	}

	return html`<ul class="grid__slots" aria-label=${t(text, 'grid.list', { channel: row.channel.name })}>
		${events.map(function (event) {
			const ends = event.start + event.duration;
			const key = eventKey(event);
			const onAir = model.isOnAir(event, props.at);
			const share = elapsedShare(event, props.at);
			return html`<li class="grid__slot" key=${key} data-on-air=${onAir ? 'one' : null}>
				<span class="grid__sclock mono">${clock(event.start)}</span>
				<button
					type="button"
					class=${props.picked === key ? 'grid__swhat is-picked' : 'grid__swhat'}
					aria-pressed=${props.picked === key ? 'true' : 'false'}
					aria-label=${t(text, onAir ? 'grid.event.onair' : 'grid.event', {
						title: event.title,
						channel: row.channel.name,
						start: clock(event.start),
						end: clock(ends),
						percent: share
					})}
					onClick=${function () { props.onPick(row, event); }}>
					<span class="grid__stitle">${event.title}</span>
					<span class="grid__swhen">${t(text, 'grid.slot.when', { start: clock(event.start), end: clock(ends) })}</span>
					${onAir ? html`<${Dot} kind="onair" word=${t(text, 'grid.running')} />` : null}
					${onAir ? html`<span class="grid__sbar"><i style=${{ width: share + '%' }}></i></span>` : null}
				</button>
				<${Link} class="btn grid__srec" href=${timerHref(event)} title=${t(text, 'grid.record')}>
					<span aria-hidden="true">⏺</span><span class="sr">${t(text, 'grid.record')}</span>
				<//>
			</li>`;
		})}
	</ul>`;
}

/**
 * What one event says about itself, out of what the listing already carries.
 *
 * The long text is not asked for here: it is one more read of the guide per
 * event somebody rests on, and what a listing carries is the short text.
 *
 * @param {{ pick: { channel: Api.Channel, event: Api.Event } | null, onClose: () => void }} props
 * @returns {Web.Drawn}
 */
function Detail(props) {
	const pick = props.pick;
	if (pick === null) {
		return null;
	}
	const ends = pick.event.start + pick.event.duration;
	return html`<div class="grid__detail" role="status">
		<div class="grid__dhead">
			<h2 class="grid__dtitle">${pick.event.title}</h2>
			<${Button} onClick=${props.onClose}>${t(text, 'grid.detail.close')}<//>
		</div>
		<p class="grid__dwhen">${pick.channel.name} · ${t(text, 'grid.detail.when', {
			start: dayAndClock(pick.event.start),
			end: clock(ends),
			duration: spanOf(pick.event.duration)
		})}</p>
		${pick.event.description === '' ? null : html`<p class="grid__dtext">${pick.event.description}</p>`}
	</div>`;
}

/**
 * @param {{ param?: string }} props
 * @returns {Web.Drawn}
 */
export default function Grid(props) {
	const asked = props.param === undefined ? '' : props.param;

	const [bouquets, setBouquets] = useState(/** @type {Api.Bouquet[] | null} */ (null));
	/* What the box is playing, held by the shared store rather than asked for
	   here: a zap invalidates that address (app/events.js), so the mark follows
	   the box without this screen asking for anything again. A box playing
	   nothing refuses it, and that is an answer as well. */
	const [current, setCurrent] = useState(/** @type {Web.Snapshot<Api.Channel> | null} */ (null));
	/* Which bouquets hold what is playing, so the guide opens where the viewer
	   is. Null until that is settled, which is what keeps the screen from
	   loading one bouquet and then another. */
	const [holding, setHolding] = useState(/** @type {Api.Bouquet[] | null} */ (null));
	const [rows, setRows] = useState(/** @type {model.Row[] | null} */ (null));
	const [listMore, setListMore] = useState(false);
	const [failure, setFailure] = useState(/** @type {Web.Failure | null} */ (null));
	const [from, setFrom] = useState(model.windowStart(nowSeconds()));
	const [scrollTop, setScrollTop] = useState(0);
	const [scrollLeft, setScrollLeft] = useState(0);
	const [viewHeight, setViewHeight] = useState(0);
	const [viewWidth, setViewWidth] = useState(0);
	const [nameWidth, setNameWidth] = useState(0);
	const [folded, setFolded] = useState(false);
	const [chosen, setChosen] = useState('');
	const [finding, setFinding] = useState(false);
	const [picked, setPicked] = useState(/** @type {{ channel: Api.Channel, event: Api.Event } | null} */ (null));
	/* The clock in whole minutes, which is the only thing on this screen that
	   changes without anybody touching it. Held as state rather than read at
	   each draw, because what makes the mark of the moment move is this
	   changing: a screen that read the clock instead would draw the same
	   picture over and over and never a different one. */
	const [minute, setMinute] = useState(nowMinute());

	const box = useRef(/** @type {HTMLElement | null} */ (null));
	// Whether where this guide opens has been settled. See the read below.
	const decided = useRef(false);

	/* What is in flight, and which question it is an answer to.

	   The token is what tells an answer about the bouquet somebody has just
	   left from an answer about the one they are looking at: both arrive, and
	   only one of them is about this screen. */
	const live = useRef({
		token: 0,
		cursor: '',
		listing: false,
		filling: '',
		control: /** @type {AbortController | null} */ (null),
		settle: 0
	});

	/* What the box event below reads when its wait is over, which is a moment
	   after the draw that started it. A closure would hold the window and the
	   rows of the draw it was made in, and by then they are a window somebody
	   has scrolled away from. */
	const held = useRef({
		from: 0,
		to: 0,
		rows: /** @type {model.Row[] | null} */ (null),
		first: 0,
		count: 0
	});

	const to = model.windowEnd(from);
	/* Nought until both questions are answered, which is what the whole screen
	   waits on: nothing walks a listing for a bouquet that is about to be
	   replaced by the one the viewer is actually watching. */
	const bouquet = (bouquets === null || holding === null) ? 0 : chosenBouquet(bouquets, asked, holding);
	// The channel the box is on, or nothing at all, which is a box in standby
	// and every other box with nothing playing.
	const playing = current && current.data ? current.data.id : '';
	/* Two different nothings, told apart: the box has not said yet, and the box
	   has said it is playing nothing. Only the second is an answer to act on,
	   and this is what the read below waits for. */
	const settledOn = (current === null || current.state === 'empty' || current.state === 'loading')
		? ''
		: '#' + playing;
	const rowHeight = model.ROW_HEIGHT;
	/* How wide a minute is drawn, out of the box as it was measured. Folded
	   there is no axis at all, and the figure is then whatever it was: nothing
	   reads it. */
	const each = model.minutePx(viewWidth, nameWidth);

	/* WHICH ROWS ARE DRAWN, AND THE ONE PLACE THE TWO FOLDINGS PART.
	   Everything behind this is the same code at both widths: which rows have
	   no guide yet, which are asked for again when the box says the guide
	   moved, and which are handed to the drawing all read this band and never
	   the folding. */
	const view = folded
		? model.only(rows === null ? [] : rows, chosen)
		: model.band({
			total: rows === null ? 0 : rows.length,
			viewport: viewHeight,
			scrollTop: scrollTop,
			hasMore: listMore,
			rowHeight: rowHeight
		});

	/* Before the first measurement there is nothing to window by, and then the
	   whole axis is drawn rather than the two hundred pixels a width of nought
	   would leave: the vertical window bounds what that can cost, and a first
	   paint showing a strip of the evening and then filling itself in is a
	   flicker nobody asked for. */
	const measured = !folded && viewWidth > 0;
	const left = measured ? scrollLeft - model.OVERSCAN_PX : 0;
	const right = measured ? scrollLeft + viewWidth + model.OVERSCAN_PX : model.axisWidth(from, to, each);

	held.current = {
		from: from,
		to: to,
		rows: rows,
		first: view.first,
		count: view.count
	};

	/* The rows of the window whose guide has not arrived, as one string, so
	   that the effect below has something it can compare: a fresh array every
	   draw would be a fresh dependency every draw, and the asking would never
	   come to rest. */
	const missing = rows === null ? '' : model.unfilled(rows, view.first, view.count).join(',');
	/* WHEN THE NEXT PAGE OF CHANNELS IS ASKED FOR, WHICH IS THREE DIFFERENT
	   GESTURES AND NEVER NONE.

	   Wide, the window over the rows comes within half a screen of the end of
	   what is in hand. Folded, the strip is pushed to within two chips of its
	   end, which is the same rule on the other axis. And with the picker open,
	   straight through to the end of the bouquet, because a search that only
	   searched the page in hand would be the same hole wearing a search field.

	   What is deliberately not here is the fourth: walking a bouquet on the first
	   paint. That would be twenty six reads of the listing for a screen showing
	   one channel's evening. */
	const wantsChannels = rows !== null && (
		finding
			? listMore
			: (!folded && model.wantsMoreChannels(view, rows.length, listMore)));

	/**
	 * @param {unknown} caught
	 * @returns {void}
	 */
	function refuse(caught) {
		// A call this screen stopped itself is not a fault to draw.
		if (isAborted(caught)) {
			return;
		}
		setFailure(/** @type {Web.Failure} */ (caught));
	}

	/* Nothing rather than null for a screen that has no controller yet: the
	   one call passes this straight to fetch, and what it says about itself is
	   a signal or no member at all. */
	/**
	 * @returns {AbortSignal | undefined}
	 */
	function signal() {
		const control = live.current.control;
		return control === null ? undefined : control.signal;
	}

	/**
	 * One ask of the guide for the channels named, walked to the end of what it
	 * has to say about them.
	 *
	 * @param {string[]} ids
	 * @param {number} token
	 * @param {number} opens
	 * @param {number} closes
	 * @returns {Promise<void>}
	 */
	async function askGuide(ids, token, opens, closes) {
		let cursor = '';
		/* One page per channel named is the most this can take: a page always
		   carries at least one row, so the cursor always advances and the walk
		   always ends. The bound is stated rather than trusted, because what a
		   wrong end costs here is a screen that asks for ever. */
		for (let page = 0; page < ids.length; page++) {
			/** @type {{ channels: string, from: number, to: number, limit: number, cursor?: string }} */
			const query = {
				channels: ids.join(','),
				from: opens,
				to: closes,
				/* A ceiling on channels and not on events: one row carries up
				   to sixty four events of its own, so this many channels can
				   be twelve hundred events and the page ends before the
				   channels do. That is what the cursor below is for. */
				limit: ids.length
			};
			if (cursor !== '') {
				query.cursor = cursor;
			}

			const answer = await api('GET', '/api/v1/epg/grid', { query: query, signal: signal() });
			/* Two questions this could be the answer to somebody has left: a
			   bouquet they have switched away from, and a window they have
			   moved. The second is the quieter of the two, because the rows
			   are still the right rows: written in, the guide of the hour
			   before would sit under an axis that is not its own, and every
			   row would then count as answered and never be asked again. */
			if (token !== live.current.token || opens !== held.current.from) {
				return;
			}
			setRows(function (was) { return was === null ? was : model.absorb(was, answer.items); });

			const next = model.nextCursor(answer);
			// Empty is the end. The same cursor twice would be a walk that
			// stopped advancing, and asking again on it would be a loop.
			if (next === '' || next === cursor) {
				return;
			}
			cursor = next;
		}
	}

	/**
	 * The next page of the bouquet's channels.
	 *
	 * @param {number} id
	 * @param {number} token
	 * @returns {Promise<void>}
	 */
	async function askChannels(id, token) {
		/** @type {{ bouquet: number, mode: 'tv', limit: number, cursor?: string }} */
		const query = { bouquet: id, mode: 'tv', limit: 100 };
		if (live.current.cursor !== '') {
			query.cursor = live.current.cursor;
		}
		const answer = await api('GET', '/api/v1/channels', { query: query, signal: signal() });
		if (token !== live.current.token) {
			return;
		}
		const cursor = model.nextChannelCursor(answer);
		live.current.cursor = cursor;
		setListMore(cursor !== '');
		setRows(function (was) {
			return (was === null ? [] : was).concat(model.rowsFor(answer.items));
		});
	}

	/**
	 * @param {number} id
	 * @returns {void}
	 */
	function walkChannels(id) {
		if (live.current.listing || id === 0) {
			return;
		}
		const token = live.current.token;
		live.current.listing = true;
		askChannels(id, token).then(function () {
			live.current.listing = false;
		}, function (caught) {
			live.current.listing = false;
			refuse(caught);
		});
	}

	// The screen's own controller, so that everything it asked for stops when
	// it is left behind rather than drawing over the screen that replaced it.
	useEffect(function () {
		const control = new AbortController();
		live.current.control = control;
		const state = live.current;
		return function () {
			state.token++;
			control.abort();
			state.control = null;
			if (state.settle !== 0) {
				clearTimeout(state.settle);
				state.settle = 0;
			}
		};
	}, []);

	// Which folding, followed rather than read once: this is a screen somebody
	// turns a tablet sideways in front of.
	useEffect(function () {
		const media = window.matchMedia('(min-width: ' + GRID_FROM_PX + 'px)');
		function settle() {
			setFolded(!media.matches);
		}
		settle();
		media.addEventListener('change', settle);
		return function () { media.removeEventListener('change', settle); };
	}, []);

	/* How much of the grid is on screen.

	   Measured from where the grid actually sits down to the bottom of the
	   window, and not taken from the frame: the bar above it is as tall as the
	   words in it and the panel under it comes and goes.

	   Before the paint and not after it: what is drawn depends on this, so a
	   measurement taken a frame later is a frame of the grid drawn to a width of
	   nought. */
	useLayoutEffect(function () {
		function measure() {
			const element = box.current;
			if (element === null) {
				return;
			}
			const seat = element.getBoundingClientRect();
			const room = Math.max(model.ROW_HEIGHT * 3, window.innerHeight - seat.top - BOTTOM_PX);
			setViewHeight(room);
			/* clientWidth and not the rectangle: it is inside the border and
			   inside whatever bar the browser put down the side, so the axis
			   worked out from it fills the box exactly and the box never
			   scrolls sideways.

			   The column of names is read out of the stylesheet and not written
			   here: the stylesheet owns it because it is the one that narrows it
			   on a tablet. */
			setViewWidth(element.clientWidth);
			const named = parseFloat(window.getComputedStyle(element).getPropertyValue('--grid-name'));
			setNameWidth(Number.isFinite(named) ? named : 0);
		}
		measure();
		window.addEventListener('resize', measure);
		/* AND THE BOX ITSELF, WHICH IS NOT THE WINDOW. What made this necessary
		   is the bar down the side: the first measurement is taken before there
		   are rows enough to need one, the bar arrives with them and takes
		   fifteen pixels out of the box without the window changing by a pixel,
		   and an axis worked out from the width before it is fifteen pixels of
		   sideways scrolling that nothing would ever correct. */
		const element = box.current;
		const watch = element === null ? null : new ResizeObserver(measure);
		if (watch !== null && element !== null) {
			watch.observe(element);
		}
		return function () {
			window.removeEventListener('resize', measure);
			if (watch !== null) {
				watch.disconnect();
			}
		};
	}, [folded, picked === null, rows === null]);

	useEffect(function () {
		api('GET', '/api/v1/bouquets', { signal: signal() }).then(function (answer) {
			setBouquets(answer.items);
		}, refuse);
	}, []);

	useEffect(function () {
		return store.watch('GET', '/api/v1/channels/current', null, setCurrent);
	}, []);

	/* Where the channel that is playing sits, asked for once and only where the
	   address has not already said which bouquet this is. It costs the box a read
	   of every bouquet's members (src/httpd/ep/ep_channels.cpp); working the same
	   thing out from here would be one request per bouquet.

	   ONCE, AND NOT AGAIN ON THE NEXT ZAP. This decides where the guide opens and
	   nothing after that: a list that moved out from under a reader would be
	   worse than one that opened in the wrong place.

	   A refusal is no bouquet in particular rather than a fault to draw. */
	useEffect(function () {
		if (asked !== '') {
			decided.current = false;
			setHolding([]);
			return undefined;
		}
		if (decided.current || settledOn === '') {
			return undefined;
		}
		decided.current = true;
		if (settledOn === '#') {
			// Answered, and what it says is that nothing is playing.
			setHolding([]);
			return undefined;
		}
		let live = true;
		api('GET', '/api/v1/bouquets', {
			query: { holds: settledOn.slice(1) },
			signal: signal()
		}).then(function (answer) {
			if (live) {
				setHolding(answer.items);
			}
		}, function (caught) {
			if (live && !isAborted(caught)) {
				setHolding([]);
			}
		});
		return function () { live = false; };
	}, [asked, settledOn]);

	// A different bouquet is a different list, so the rows go and the walk of
	// the listing begins again at the front.
	useEffect(function () {
		if (bouquet === 0) {
			return;
		}
		live.current.token++;
		live.current.cursor = '';
		live.current.listing = false;
		live.current.filling = '';
		setRows(null);
		setListMore(false);
		setPicked(null);
		setChosen('');
		setFinding(false);
		setFailure(null);
		walkChannels(bouquet);
	}, [bouquet]);

	// The window moved, so what every row holds is about a time nobody is
	// looking at any more. The channels stay: they are what the listing cost,
	// and keeping them is what keeps the screen still while the guide is on
	// its way.
	useEffect(function () {
		live.current.filling = '';
		setPicked(null);
		setRows(function (was) { return was === null ? was : model.forgetGuide(was); });
	}, [from]);

	// More channels, once the window comes within half a screen of the end of
	// what is in hand.
	useEffect(function () {
		if (wantsChannels) {
			walkChannels(bouquet);
		}
	}, [wantsChannels, rows === null ? 0 : rows.length]);

	// And the guide for the rows on screen that have none.
	useEffect(function () {
		if (missing === '' || live.current.filling === missing) {
			return;
		}
		const token = live.current.token;
		live.current.filling = missing;
		askGuide(missing.split(','), token, from, to).then(function () {
			if (live.current.filling === missing) {
				live.current.filling = '';
			}
		}, function (caught) {
			if (live.current.filling === missing) {
				live.current.filling = '';
			}
			refuse(caught);
		});
	}, [missing, from]);

	/* THE CLOCK, WHICH IS THE ONE THING HERE THAT MOVES BY ITSELF.

	   What is on air, where the mark of the moment stands and how far the bar
	   under a running programme has come are all worked out from this.

	   Looked at oftener than it moves, and moved only when it has: handing the
	   same minute back is a state that did not change and no draw at all. And it
	   does not move at all while somebody is inside a control, because a draw
	   would take what they were doing away from them. */
	useEffect(function () {
		const beat = setInterval(function () {
			if (takingInput()) {
				return;
			}
			setMinute(nowMinute());
		}, TICK_MS);
		return function () { clearInterval(beat); };
	}, []);

	/* The box says the guide moved, and this asks the rows on screen again.

	   What the event carries is a channel and a type and nothing else
	   (src/coreapi/base/eventbus.h), so it cannot be drawn from; and it arrives once
	   per channel whose current event changed, so it is waited out rather than
	   obeyed. What comes back replaces what is drawn without emptying it first,
	   which is the difference between a grid that flickers every few minutes and
	   one that does not. */
	useEffect(function () {
		return boxEvents.on('epg-updated', function () {
			if (live.current.settle !== 0) {
				return;
			}
			live.current.settle = setTimeout(function () {
				live.current.settle = 0;
				const now = held.current;
				if (now.rows === null) {
					return;
				}
				const ids = model.inBand(now.rows, now.first, now.count);
				if (ids.length === 0) {
					return;
				}
				askGuide(ids, live.current.token, now.from, now.to).catch(refuse);
			}, SETTLE_MS);
		});
	}, []);

	/**
	 * @param {model.Row} row
	 * @param {Api.Event} event
	 * @returns {void}
	 */
	function pick(row, event) {
		const key = eventKey(event);
		setPicked(function (was) {
			return was !== null && eventKey(was.event) === key ? null : { channel: row.channel, event: event };
		});
	}

	if (failure !== null) {
		// What the box said about its own refusal, and no sentence of this
		// screen's making beside it.
		return html`<${State}
			problem=${{
				title: failure.problem.title,
				detail: failure.problem.detail,
				href: problemHref(failure.problem)
			}}
			onRetry=${function () {
				setFailure(null);
				live.current.filling = '';
				setRows(function (was) { return was === null ? was : model.forgetGuide(was); });
			}} />`;
	}

	if (bouquets === null || rows === null) {
		return html`<${State} phase="first" />`;
	}

	if (bouquets.length === 0) {
		return html`<${State} empty=${t(text, 'grid.nobouquets')} />`;
	}

	const pickedKey = picked === null ? '' : eventKey(picked.event);
	// Whether this bouquet holds what is playing, which is what says whether
	// there is a mark on this screen for the line under it to explain.
	const marked = playing !== '' && rows.some(function (row) { return row.channel.id === playing; });
	/* As tall as it needs to be, and never taller than the room it has. A box
	   fixed at the room would be a frame of nothing under a bouquet of three
	   channels, and one fixed at the rows would run off the bottom of the
	   window on a bouquet of two hundred. */
	const filled = model.HEAD_HEIGHT
		+ rows.length * rowHeight
		+ (listMore ? rowHeight : 0);
	// Whole minutes, so that the mark, the flag and the bars are all drawn to
	// the one moment the flag has written on it.
	const moment = minute * 60;
	// Where the window holds a later time than now, what is shown as "now" is
	// what is on at the edge of it, which is what somebody looking at next
	// week means by the first thing on the screen.
	const at = Math.min(Math.max(moment, from), to - 1);
	const shown = rows.slice(view.first, view.first + view.count);
	const nowLeft = model.atPixel(moment, from, to, each);
	const here = shown[0];

	return html`<div
		class=${folded ? 'epg-grid is-folded' : 'epg-grid'}
		style=${{ '--grid-head': model.HEAD_HEIGHT + 'px' }}>
		<div class="grid__bar">
			<label class="grid__pick">
				<span class="grid__plabel">${t(text, 'grid.bouquet')}</span>
				<select
					value=${String(bouquet)}
					onChange=${function (/** @type {Event} */ e) {
						const target = /** @type {HTMLSelectElement} */ (e.currentTarget);
						// The address carries the choice, so that a grid can be
						// sent to somebody and opened where it was left.
						route(hrefFor('epg', 'grid', target.value));
					}}>
					${bouquets.map(function (one) {
						return html`<option key=${one.id} value=${String(one.id)}>${one.name}</option>`;
					})}
				</select>
			</label>
			<div class="grid__when">
				<${Button} onClick=${function () { setFrom(from - model.WINDOW_MINUTES * 60); }}>${t(text, 'grid.back')}<//>
				<${Button} onClick=${function () { setFrom(model.windowStart(nowSeconds())); }}>${t(text, 'grid.now')}<//>
				<${Button} onClick=${function () { setFrom(from + model.WINDOW_MINUTES * 60); }}>${t(text, 'grid.forward')}<//>
			</div>
			<p class="grid__window">${t(text, 'grid.window', { from: dayAndClock(from), to: clock(to) })}</p>
		</div>

		<${Detail} pick=${picked} onClose=${function () { setPicked(null); }} />

		${rows.length === 0 && !listMore
			? html`<${State} empty=${t(text, 'grid.empty')} />`
			: folded
			? html`<${Fragment}>
				<${Strip}
					rows=${rows}
					chosen=${here === undefined ? '' : here.channel.id}
					hasMore=${listMore}
					onPick=${setChosen}
					onEnd=${function () { walkChannels(bouquet); }} />
				<${Finder}
					open=${finding}
					rows=${rows}
					chosen=${here === undefined ? '' : here.channel.id}
					walking=${listMore}
					onPick=${function (/** @type {string} */ id) { setChosen(id); setFinding(false); }}
					onClose=${function () { setFinding(false); }} />
				<div class="grid__nowbar">
					<span class="grid__nowmark" aria-hidden="true"></span>
					<span class="mono">${t(text, 'grid.now')} ${clock(at)}</span>
					<${Button} class="grid__find" onClick=${function () { setFinding(true); }}>${t(text, 'grid.pick')}<//>
				</div>
				${here === undefined
					? null
					: html`<${Slots} row=${here} at=${at} picked=${pickedKey} onPick=${pick} />`}
				${listMore ? html`<p class="grid__more" role="status">${t(text, 'grid.more')}</p>` : null}
			<//>`
			/* In the tab order: this box scrolls in both directions, and
			   somebody arriving with a keyboard has to be able to move it
			   before there is a programme in it to move to. */
			: html`<div
				class="grid__scroll"
				ref=${box}
				role="group"
				aria-label=${t(text, 'grid.table')}
				tabindex="0"
				style=${{ height: viewHeight > 0 ? Math.min(viewHeight, filled) + 'px' : null }}
				onScroll=${function (/** @type {Event} */ e) {
					const target = /** @type {HTMLElement} */ (e.currentTarget);
					setScrollTop(target.scrollTop);
					setScrollLeft(target.scrollLeft);
				}}>
				<div class="grid__head">
					<div class="grid__corner"></div>
					<${Axis} from=${from} to=${to} left=${left} right=${right} each=${each} now=${nowLeft} at=${at} />
				</div>
				<div class="grid__body">
					${/* THE MARK OF THE MOMENT, IN THE COORDINATES THE PROGRAMMES ARE IN.
					      Offset by the column of names, because that column is
					      part of the row and the axis is not: the line stands
					      over the lanes and never over the names. The offset is
					      the stylesheet's own figure and not a copy of it, so
					      the line follows the column when a tablet narrows it,
					      in the same frame, without waiting to be measured. */
					nowLeft < 0
						? null
						: html`<div class="grid__nowline" aria-hidden="true" style=${{ left: 'calc(var(--grid-name) + ' + nowLeft + 'px)' }}></div>`}
					<div style=${{ height: view.padTop + 'px' }}></div>
					${shown.map(function (row) {
						const mine = row.channel.id === playing;
						return html`<div
							class=${mine ? 'grid__row is-onair' : 'grid__row'}
							key=${row.channel.id}
							data-channel=${row.channel.id}
							data-on-air=${mine ? 'one' : null}
							style=${{ height: rowHeight + 'px' }}>
							<${Name} row=${row} onAir=${mine} />
							<${Lane}
								row=${row}
								from=${from}
								to=${to}
								left=${left}
								right=${right}
								each=${each}
								now=${nowLeft}
								at=${moment}
								picked=${pickedKey}
								onPick=${pick} />
						</div>`;
					})}
					<div style=${{ height: view.padBottom + 'px' }}></div>
					${listMore ? html`<p class="grid__more" role="status">${t(text, 'grid.more')}</p>` : null}
				</div>
			</div>`}
		${/* The word the mark in the column of names stands for, said once and
		      under the thing it explains. One mark and one word, so there is
		      nothing here to match up: a legend of several would ask a reader to
		      tell two colours apart, which is the thing a legend is for avoiding.

		      Only where there is a mark to explain. The folded strip draws none,
		      and a bouquet that does not hold what is playing draws none either,
		      so both would otherwise carry a line about a dot nobody can find. */''}
		${folded || !marked
			? null
			: html`<p class="grid__legend"><${Dot} kind="onair" word=${t(text, 'grid.playing')} /></p>`}
	</div>`;
}
