// What the page's own layers hand each other, and the few pieces that turn the
// server's document into something a call site can be held to.
//
// The division is the whole point and is worth stating once:
//
//   Api.*  is what the box says. It is in api.d.ts, it is written out of
//          src/httpd/openapi.cpp on every run, and nothing in it is edited.
//          A shape changes by changing the server.
//
//   Web.*  is what this page says to itself: the state of a store entry, what
//          a screen is handed, how a screen is registered. It is here, it is
//          written by hand, and it is in the commit.
//
// What is NOT here is a type that belongs to one module and is used by one
// module. Those are @typedef in the module itself, next to the code they
// describe, and are reached as import('./store.js').Entry where anybody else
// needs one. This file is for the shapes that cross the whole page, because a
// screen should not have to name six modules to say what it draws.

declare namespace Api {
	/** every route of this API, as the one call spells it */
	type Key = keyof Ops;

	/** what a value in a path or a query may be, before it is spelt into one */
	type Scalar = string | number | boolean;

	/**
	 * How a caller wants the answer read. json is almost everything; blob is a
	 * screenshot and a downloaded file, text is a playlist, and response is
	 * the answer untouched for the one caller that needs a header off it.
	 */
	type Accept = 'json' | 'blob' | 'text' | 'response';

	/**
	 * What the box answers on that route, and unknown for a route this
	 * document does not name.
	 *
	 * Unknown and not never: the store asks with a method and an address it
	 * was handed, so its calls resolve to no key at all, and a caller that
	 * cannot be told what it will get should have to look before it uses it.
	 */
	type Result<K extends string> = K extends Key ? Ops[K]['result'] : unknown;

	/** the pieces of that route's template, and never for a route with none */
	type PathValues<K extends string> = K extends Key ? Ops[K]['params'] : Record<string, Scalar>;

	/** that route's query, and never for a route that takes none */
	type QueryValues<K extends string> = K extends Key ? Ops[K]['query'] : Record<string, Scalar>;

	/** that route's body, and never for a route that takes none */
	type BodyValue<K extends string> = K extends Key ? Ops[K]['body'] : unknown;

	/**
	 * The two routes whose body does not go out as one flat object, and what
	 * the page hands over for each instead. Both are held here, by name, so
	 * that widening the member below costs exactly these two routes and no
	 * third one, and so that a reader can see at a glance what has to change
	 * elsewhere before this can go.
	 *
	 * PUT /api/v1/bouquets/{bouquet}/channels: the document states a JSON
	 * array. The one call serializes a plain object and hands anything else
	 * to fetch as it stands (data/ni-web/app/api.js), and an array handed to
	 * fetch arrives as its members joined by commas, so what goes out is the
	 * text of the array. This says string for that reason, and refuses the
	 * array itself, which would be sent wrong and answered with a refusal
	 * nobody could read. It goes away when that one call serializes an array.
	 *
	 * PUT /api/v1/storage/file used to be the second of these, because the
	 * route declared two query parameters and no body while its handler read
	 * one. It declares the body now, so the generated type says what it takes
	 * and nothing is written here about it.
	 */
	type RawBody<K extends string> =
		K extends 'PUT /api/v1/bouquets/{bouquet}/channels' ? string :
		never;

	/**
	 * What one call hands back, once the way it asked for the answer is taken
	 * into account. A caller that asked for bytes gets bytes whatever the
	 * document says the JSON of that route looks like.
	 */
	type Answer<K extends string, A> =
		A extends 'blob' ? Blob :
		A extends 'text' ? string :
		A extends 'response' ? Response :
		Result<K>;

	/**
	 * Everything a call may say about itself besides the values for the
	 * template. Bound to the route, so that a query this route does not take
	 * and a body it does not read are refused here rather than sent.
	 */
	type Options<K extends string, A extends Accept> = {
		/** the values for the pieces of the template, and null for a route with none */
		params?: PathValues<K> | null;
		/** the query, whose order does not matter: the one call sorts it */
		query?: Partial<QueryValues<K>>;
		/** the body: a flat object for every route that declares one, and what RawBody says for the two that do not */
		body?: BodyValue<K> | RawBody<K>;
		/** the screen's own controller, so a screen left behind stops asking */
		signal?: AbortSignal;
		/** how to read the answer, json when it is not said */
		accept?: A;
		/**
		 * Whether to ask the box for a fresh token and send again once.
		 * 'token' asks and sends again but never opens the sign in sheet,
		 * which is right for the two calls that sign in and out.
		 */
		recover?: boolean | 'token';
		/** the address prefixes this call changes, for the store to grey */
		touches?: readonly string[];
	};

	/**
	 * How a call may be spelt. The long form above, or, for the common case of
	 * a route whose template wants values and whose call says nothing else,
	 * the values on their own:
	 *
	 *     api('GET', '/api/v1/settings/{section}', { section: 'general' })
	 *
	 * The two cannot be confused, because no template in this API carries a
	 * piece named after one of the options.
	 */
	type Call<K extends string, A extends Accept> = Options<K, A> | PathValues<K>;
}

declare namespace Web {
	/** what the store knows about one address */
	type State = 'empty' | 'loading' | 'ready' | 'error';

	/** and, while it is loading, which of the three loads this is */
	type Phase = '' | 'first' | 'again' | 'writing';

	/** what a request was granted, in the order the box orders them */
	type Level = 'public' | 'read' | 'write' | 'system';

	/** what every refusal arrives as */
	type Failure = import('../../../data/ni-web/app/problem.js').ApiError;

	/**
	 * What a watcher of one store entry is handed, every time that entry
	 * changes. data is what was last answered and stays put across a reload,
	 * which is the difference between a page that flickers on every event and
	 * one that does not.
	 */
	type Snapshot<T> = {
		state: State;
		phase: Phase;
		data: T | null;
		error: Failure | null;
		at: number;
		url: string;
	};

	/**
	 * A catalogue of words: one object per language, flat keys, and the value
	 * a whole sentence with {name} in it where something is filled in. Every
	 * part of the page keeps its own beside itself, so that ten of them can be
	 * written at once without anybody editing a file somebody else is in;
	 * test/check-web-text.sh holds them all to this shape and to each other.
	 */
	type Catalog = Record<string, Record<string, string>>;

	/** what is put into the places a sentence leaves open */
	type Values = Record<string, string | number>;

	/** what a screen draws: one node, or the several a template with more than one root makes */
	type Drawn = import('preact').VNode<any> | import('preact').VNode<any>[] | null;

	/**
	 * What a screen is handed to reach the box with. Written as the modules
	 * themselves rather than as a list of their functions, so that a function
	 * added to one of them is here without anybody editing this.
	 */
	type Context = {
		api: typeof import('../../../data/ni-web/app/api.js').api;
		session: typeof import('../../../data/ni-web/app/session.js');
		events: typeof import('../../../data/ni-web/app/events.js');
	};

	/** what is read to put a name on a destination or an entry */
	type NavNode = {
		id: string;
		/** the key of this page's own word for it, where there is one */
		text?: string;
		/** what the box called it, for one this page has no word for */
		label?: string;
	};

	/** one entry of a destination's second level */
	type NavEntry = NavNode & {
		/**
		 * The module a screen is. Beside the screen itself it may name a
		 * stylesheet and offer the sentence the frame draws under the name of
		 * the screen; the sentence is a function because a value read at load
		 * would be read before anybody has chosen a language.
		 */
		load: () => Promise<{ default: (props: any) => Drawn, css?: string, lead?: () => string }>;
		/**
		 * Identifiers this entry used to be reached under, so that an address
		 * somebody wrote down before two screens became one still opens the
		 * screen rather than the page's own not-found note. Never drawn: what
		 * a bar shows is id.
		 */
		was?: readonly string[];
	};

	/**
	 * One destination of the first level, with the table it states. items is
	 * allowed to be a question rather than a list, because the sections of the
	 * settings are only known to the running box.
	 */
	type NavArea = NavNode & {
		items: NavEntry[] | ((ctx: Context) => NavEntry[] | Promise<NavEntry[]>);
		/** the one character the bar along the bottom and the sheet draw it with */
		ic?: string;
		/** whether it is set apart from the rest of the row by a rule before it */
		apart?: boolean;
		/** what the build has to carry for this one to be offered at all */
		needs?: 'api-doc';
	};

	/** what the frame itself shows, which is everything and nothing in particular */
	type ShellStatus = {
		channel: Api.Channel | null;
		/** what the guide says is on that channel now, and null where it says nothing */
		event: Api.Event | null;
		recordings: Api.Recording[];
		session: import('../../../data/ni-web/app/session.js').Session | null;
		/**
		 * What the box said about its own build, and null before it has said.
		 * Here rather than in a module of its own because the frame is the only
		 * part of the page that reads it and it arrives on the same answer the
		 * frame already asks for.
		 */
		build: { apiDoc: boolean } | null;
		/**
		 * Whether the event stream is open right now. The frame's own state
		 * because the mark in the corner is the one place the page says it, and
		 * every screen behind that mark refreshes itself off that stream.
		 */
		carrying: boolean;
	};

	/**
	 * A refusal as the four members a screen draws. Not Api.Problem: the state
	 * block also draws faults this page raised for itself, which have a title
	 * and a sentence and no status and no type.
	 */
	type Shown = {
		title?: string;
		detail?: string;
		href?: string;
	};

	/** which column a table is ordered by, and which way */
	type Sort = { column: string, dir: 'asc' | 'desc' };

	/**
	 * An event as the handler on one element is handed it: the event itself,
	 * with currentTarget narrowed to that element.
	 *
	 * Here rather than left to the checker because nothing can infer it. The
	 * templates are htm, which takes the handler as one more value in a tagged
	 * template and hands it on untyped, so a handler written inline against a
	 * control has no shape to be read off the control it is on. Every one of
	 * them therefore says what it is put on, and it is worth saying the whole
	 * of it: an input's handler is handed an InputEvent whose currentTarget is
	 * the HTMLInputElement, and it is exactly that second half that lets the
	 * checker see through to .value and .checked.
	 *
	 * currentTarget and not target, which is the element the event started at
	 * and is only the same element for a control with nothing inside it. The
	 * page reads the one it put the handler on.
	 */
	type On<T extends EventTarget, E extends Event = Event> =
		import('preact').JSX.TargetedEvent<T, E>;
}
