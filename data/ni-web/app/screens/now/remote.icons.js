// The pictures on the keys of the drawn remote. One table and not one per
// language, because a picture reads the same in every language.

// SVGs are taken from https://fonts.google.com/icons?icon.size=24

import { html } from '../../runtime.js';

/** @type {Record<string, Web.Drawn>} */
export default {
	/* colored keys */
	KEY_RED:	html`<svg xmlns="http://www.w3.org/2000/svg" height="24px" viewBox="0 -960 960 960" width="24px" fill="#f44336"><path d="M120-120v-720h720v720H120Z"/></svg>`,
	KEY_GREEN:	html`<svg xmlns="http://www.w3.org/2000/svg" height="24px" viewBox="0 -960 960 960" width="24px" fill="#4caf50"><path d="M120-120v-720h720v720H120Z"/></svg>`,
	KEY_YELLOW:	html`<svg xmlns="http://www.w3.org/2000/svg" height="24px" viewBox="0 -960 960 960" width="24px" fill="#ffeb3b"><path d="M120-120v-720h720v720H120Z"/></svg>`,
	KEY_BLUE:	html`<svg xmlns="http://www.w3.org/2000/svg" height="24px" viewBox="0 -960 960 960" width="24px" fill="#3f51b5"><path d="M120-120v-720h720v720H120Z"/></svg>`,
	KEY_POWER:	html`<svg xmlns="http://www.w3.org/2000/svg" height="24px" viewBox="0 -960 960 960" width="24px" fill="#f44336"><path d="M480-80q-83 0-156-31.5T197-197q-54-54-85.5-127T80-480q0-84 31.5-156.5T197-763l56 56q-44 44-68.5 102T160-480q0 134 93 227t227 93q134 0 227-93t93-227q0-67-24.5-125T707-707l56-56q54 54 85.5 126.5T880-480q0 83-31.5 156T763-197q-54 54-127 85.5T480-80Zm-40-360v-440h80v440h-80Z"/></svg>`,
	KEY_RECORD:	html`<svg xmlns="http://www.w3.org/2000/svg" height="24px" viewBox="0 -960 960 960" width="24px" fill="#f44336"><path d="M281.5-281.5Q200-363 200-480t81.5-198.5Q363-760 480-760t198.5 81.5Q760-597 760-480t-81.5 198.5Q597-200 480-200t-198.5-81.5Z"/></svg>`,
	/* currentColor keys */
	KEY_DOWN:	html`<svg xmlns="http://www.w3.org/2000/svg" height="24px" viewBox="0 -960 960 960" width="24px" fill="currentColor"><path d="M480-360 280-560h400L480-360Z"/></svg>`,
	KEY_FORWARD:	html`<svg xmlns="http://www.w3.org/2000/svg" height="24px" viewBox="0 -960 960 960" width="24px" fill="currentColor"><path d="M100-240v-480l360 240-360 240Zm400 0v-480l360 240-360 240Z"/></svg>`,
	KEY_LEFT:	html`<svg xmlns="http://www.w3.org/2000/svg" height="24px" viewBox="0 -960 960 960" width="24px" fill="currentColor"><path d="M560-280 360-480l200-200v400Z"/></svg>`,
	KEY_MUTE:	html`<svg xmlns="http://www.w3.org/2000/svg" height="24px" viewBox="0 -960 960 960" width="24px" fill="currentColor"><path d="M792-56 671-177q-25 16-53 27.5T560-131v-82q14-5 27.5-10t25.5-12L480-368v208L280-360H120v-240h128L56-792l56-56 736 736-56 56Zm-8-232-58-58q17-31 25.5-65t8.5-70q0-94-55-168T560-749v-82q124 28 202 125.5T840-481q0 53-14.5 102T784-288ZM650-422l-90-90v-130q47 22 73.5 66t26.5 96q0 15-2.5 29.5T650-422ZM480-592 376-696l104-104v208Zm-80 238v-94l-72-72H200v80h114l86 86Zm-36-130Z"/></svg>`,
	KEY_NEXT:	html`<svg xmlns="http://www.w3.org/2000/svg" height="24px" viewBox="0 -960 960 960" width="24px" fill="currentColor"><path d="M504-480 320-664l56-56 240 240-240 240-56-56 184-184Z"/></svg>`,
	KEY_NEXTSONG:	html`<svg xmlns="http://www.w3.org/2000/svg" height="24px" viewBox="0 -960 960 960" width="24px" fill="currentColor"><path d="M660-240v-480h80v480h-80Zm-440 0v-480l360 240-360 240Z"/></svg>`,
	KEY_PAGEDOWN:	html`<svg xmlns="http://www.w3.org/2000/svg" height="48px" viewBox="0 -960 960 960" width="48px" fill="currentColor"><path d="M480-200 240-440l42-42 198 198 198-198 42 42-240 240Zm0-253L240-693l42-42 198 198 198-198 42 42-240 240Z"/></svg>`,
	KEY_PAGEUP:	html`<svg xmlns="http://www.w3.org/2000/svg" height="48px" viewBox="0 -960 960 960" width="48px" fill="currentColor"><path d="m282-225-42-42 240-240 240 240-42 42-198-198-198 198Zm0-253-42-42 240-240 240 240-42 42-198-198-198 198Z"/></svg>`,
	KEY_PAUSE:	html`<svg xmlns="http://www.w3.org/2000/svg" height="24px" viewBox="0 -960 960 960" width="24px" fill="currentColor"><path d="M560-200v-560h160v560H560Zm-320 0v-560h160v560H240Z"/></svg>`,
	KEY_PLAY:	html`<svg xmlns="http://www.w3.org/2000/svg" height="24px" viewBox="0 -960 960 960" width="24px" fill="currentColor"><path d="M320-200v-560l440 280-440 280Z"/></svg>`,
	KEY_PREVIOUS:	html`<svg xmlns="http://www.w3.org/2000/svg" height="24px" viewBox="0 -960 960 960" width="24px" fill="currentColor"><path d="M560-240 320-480l240-240 56 56-184 184 184 184-56 56Z"/></svg>`,
	KEY_PREVIOUSSONG: html`<svg xmlns="http://www.w3.org/2000/svg" height="24px" viewBox="0 -960 960 960" width="24px" fill="currentColor"><path d="M220-240v-480h80v480h-80Zm520 0L380-480l360-240v480Z"/></svg>`,
	KEY_REWIND:	html`<svg xmlns="http://www.w3.org/2000/svg" height="24px" viewBox="0 -960 960 960" width="24px" fill="currentColor"><path d="M860-240 500-480l360-240v480Zm-400 0L100-480l360-240v480Z"/></svg>`,
	KEY_RIGHT:	html`<svg xmlns="http://www.w3.org/2000/svg" height="24px" viewBox="0 -960 960 960" width="24px" fill="currentColor"><path d="M400-280v-400l200 200-200 200Z"/></svg>`,
	KEY_STOP:	html`<svg xmlns="http://www.w3.org/2000/svg" height="24px" viewBox="0 -960 960 960" width="24px" fill="currentColor"><path d="M240-240v-480h480v480H240Z"/></svg>`,
	KEY_UP:		html`<svg xmlns="http://www.w3.org/2000/svg" height="24px" viewBox="0 -960 960 960" width="24px" fill="currentColor"><path d="m280-400 200-200 200 200H280Z"/></svg>`,
	KEY_VOLUMEDOWN:	html`<svg xmlns="http://www.w3.org/2000/svg" height="48px" viewBox="0 -960 960 960" width="48px" fill="currentCOlor"><path d="M200-450v-60h560v60H200Z"/></svg>`,
	KEY_VOLUMEUP:	html`<svg xmlns="http://www.w3.org/2000/svg" height="48px" viewBox="0 -960 960 960" width="48px" fill="currentColor"><path d="M450-450H200v-60h250v-250h60v250h250v60H510v250h-60v-250Z"/></svg>`,
	/* remamings */
	KEY_FAVORITES:	html`FAV`,
	KEY_SUBTITLE:	html`SUBT`,
	KEY_TIME:	html`TIMER`
};
