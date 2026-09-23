# NI-Web

The web interface of a Neutrino box: the pages a browser loads, and nothing
else. What answers them is `src/httpd/` beside it, and what that asks is
`src/coreapi/`.

## There is no build step

What the browser loads is the file in this tree. No bundler, no transpiler,
nothing emitted. Preact, its hooks, htm and the router arrive as published
modules under `/vendor`; the pages import them through `app/runtime.js` and
nowhere else.

That is why the development server's docroot points straight at these
sources: **a changed file plus a reload in the browser is the whole edit
cycle.** It is also why this interface leaves the tree as bytes that are
already what they will be on the box, and why no firmware build needs node.

If you are about to propose rewriting these files as TypeScript, read the head
of `jsconfig.json` first. That trade is a build step in the path of every
firmware image, in exchange for annotations that read a little better.

## What you need

**To change a page and see it:** an editor, a browser, and a box. Nothing
else; see "Running the pages against a box" below.

**To run the checks:** node, and the declaration files the type checker reads.
Fetch them once, from a clone of this repository and nothing else:

    sh test/web/fetch-types.sh

That puts preact, htm, preact-router and TypeScript under
`test/web/node_modules`, at the versions the box is built with. Without them
your editor cannot resolve `/vendor/preact.module.js` and paints errors that
are not there, and `test/web/check-web-types.sh` refuses to run rather than
stepping aside. One of the other checks wants node 22.15 or newer and says so
when it does not have it.

**The six runtime files under `vendor/` are a separate matter.** They are what
the browser loads, and a box already carries them, so serving the pages
against a box needs nothing here. They are only missing if you serve this
directory with no box behind it, which nothing here asks you to do. Nothing
under `test/web/node_modules` ever reaches a browser or a box.

## Running the pages against a box

The pages need something that answers them, and the honest something is a box.
Three ways, and the first needs nothing on the box:

**Serve the pages here, ask the box for the rest.**

    node test/web/serve.js <box-ip>

Then open <http://localhost:8080>. Files come off this tree; everything under
`/api/`, `/control/` and the other addresses the box owns is handed straight
to it, headers, status and session cookie included. Nothing is installed,
nothing is cached, and nothing on the box is touched. Edit a file, reload the
browser, that is the whole cycle. Add `:8081` or whatever port the box answers
on, and set `PORT` to move this end.

**Or let the box read your working copy.** `ni-web.conf` on the box has a
`docroot` key. Point it at a share of this directory and restart the server;
then the box serves your edits to every device in the house, the telephone
included. Good for looking at a page on a real phone, which a desktop browser
narrowed to 390 pixels only approximates.

**Or copy the pages onto the box** after each edit, into the docroot the image
installed. Crude, needs no setup beyond ssh, and worth knowing about when the
other two are not available.

You can also development a container that builds Neutrino and serves the
interface beside it. This is the way how NI-Web was developed.

## Types, and why the editor already knows them

The pages are checked with `tsc --checkJs --noEmit` against the JSDoc written
into them. `jsconfig.json` carries the rules, and it is named that rather than
`tsconfig.json` for the one difference between the names: **a directory
holding a `jsconfig.json` is a JavaScript project, and an editor opening a
file under it checks by these rules with nobody configuring anything.**

So completion and errors in your editor are the same ones `make check`
reports. That covers the page's own modules, Preact and the router, and the
box's routes: `test/web/types/api.d.ts` is written out of the server's own
OpenAPI document on every run, so an answer from the box has real members.

The rules are stricter than usual, and each one is argued in `jsconfig.json`.
The two that surprise people: `noUncheckedIndexedAccess` means `rows[0]` of a
page that may be empty is `undefined` and has to be handled, and
`noUnusedLocals` means an import left behind after a rewrite is an error, not
a warning. A parameter deliberately unused is spelt with a leading underscore.

**JSDoc here is not decoration.** Dropping a `@param` while shortening a
comment turns the tree red.

## The checks

`test/web/` holds them, one question each, and `make check` runs them all.
Each is a shell script you can run on its own with the source directory as its
argument; three of them want the build directory as a second.

They ask, among other things: do the imports resolve and does the document
stay root absolute; does every address the pages call exist in the box's
document; does every text exist in both languages; does a screen's stylesheet
stay off the class names the frame owns; and do the pages fit the byte budget
of the smallest box.

Some of them drive real logic without a browser, in the style of
`test/web/drag-cases.mjs`: the dragging gesture, the four roads a channel can
take to a player, the store's bookkeeping. If what you write is arithmetic,
that is where its test goes.

## Where things are

    app/            the interface
      runtime.js    the only file that names the modules under /vendor
      api.js        the only file that builds an address
      store.js      what has been read from the box, and who is watching it
      nav.js        the destinations, and what each of them holds
      router.js     the frame around a screen
      screens/      one directory per destination
      ui/           what more than one screen draws
      css/          tokens.css and shell.css, the frame's own two
    info/           a display page of its own, for pinning to a home screen
    vendor/         fetched, not committed
    swagger/        the API documentation page, fetched the same way
    rc/             photographs of the handsets, one per shape of remote

A screen owns its stylesheet and names it in `export const css`; the frame
loads it when the screen is first drawn.

## House rules worth knowing before the first patch

- **Comments say why, not what.** The tree is unusually talkative, and most of
  what is written down is a trap somebody already fell into, a measurement, or
  the second place that has to change with this one. Shorten freely; delete
  those three carefully.
- **Every displayed word lives in a `*.text.js` catalogue, in both languages.**
  A key only one language has fails the check.
- **`localStorage` is for conveniences of one browser** and nothing else, read
  and written in `try/catch`. Anything that must survive or be seen by anybody
  else belongs on the box.
- **The smallest box has 28 MB of flash for the whole image.** Everything here
  is weighed; `test/web/check-web-size.sh` says the figure.

## A trap that has cost several afternoons

A build directory configured without `--enable-unit-tests` says so, runs
nothing, and exits 0. A green run from such a tree is not a green run.
