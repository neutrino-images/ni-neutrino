// What the page uses of the playlist library, and nothing else.
//
// WRITTEN BY HAND AND IN THE COMMIT, unlike api.d.ts beside it. The library
// itself is not in this tree: the buildsystem fetches it and puts it under
// /vendor on a box, and the development container's fetch script puts it there
// too. Neither of them unpacks it into test/web/node_modules, because nothing
// about the check needs the whole archive, and a check that needed it would be
// a check that cannot run until somebody has fetched a hundred kilobytes of
// JavaScript.
//
// So this states the six names the page reaches for, and it is deliberately
// the smallest thing that can be true: a declaration that described the whole
// library would be a second copy of somebody else's interface, kept in step by
// nobody. The page also asks the library at run time whether it is supported
// at all, which is what actually stands between a version that moved something
// and a black rectangle.

export type HlsErrorData = {
	fatal: boolean;
	type: string;
	details: string;
};

export interface HlsInstance {
	loadSource(url: string): void;
	attachMedia(video: HTMLMediaElement): void;
	on(event: string, listener: (event: string, data: HlsErrorData) => void): void;
	destroy(): void;
}

declare const Hls: {
	new (config?: Record<string, unknown>): HlsInstance;
	isSupported(): boolean;
	Events: { ERROR: string; MANIFEST_PARSED: string };
};

export default Hls;
