// The runtime, named once for the whole page. Everything this interface loads but
// does not carry is addressed here and nowhere else: the element factory, the
// template tag, the hooks and the router. A version that moves or renames one of them
// is one edit in this file, and a check holds the rest of the tree to it.
//
// The addresses are root absolute because the document is served under its own paths
// as well as under the root, and a relative name would resolve against whatever the
// address bar last held.
import { h, render, Fragment } from '/vendor/preact.module.js';
import htm from '/vendor/htm.module.js';

export { h, render, Fragment };
export const html = htm.bind(h);

export {
	useState,
	useEffect,
	useLayoutEffect,
	useRef,
	useMemo,
	useCallback,
	useId
} from '/vendor/hooks.module.js';

export { default as Router, Link, route, getCurrentUrl } from '/vendor/preact-router.module.js';

/* The one module of the runtime that does not arrive with the page.

   It is what plays a web television channel in a browser that does not play
   the playlist format by itself, which is every browser but one. It is large,
   it is wanted on one kind of channel only, and in that one browser it is
   wanted never, so it is fetched when something asks for it rather than at the
   top of a file. That is the whole difference between this and the four
   above: an address inside a call is fetched when the call is made.

   Named here like the other four and for the same reason, so a version that
   moves or renames it is one edit and no screen binds itself to a name in
   somebody else's bytes. */
export function loadHls() {
	return import('/vendor/hls.module.js');
}
