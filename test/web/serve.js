// The pages off this tree, the box's answers from the box.
//
// WHAT IT IS FOR. Working on these pages needs a server that answers them,
// and the honest one is a box. There are three ways to have both, and this is
// the one that changes nothing on the box: it serves data/ni-web from here and
// hands everything the pages ask of the box straight on to it.
//
// The pages call the box at root absolute addresses (/api/v1/..., /control/...,
// and that is deliberate, see data/ni-web/index.html). A page served from this
// machine therefore asks this machine, and this passes those asks on. Nothing
// is rewritten and nothing is cached: what the box answered is what the browser
// gets, headers and status included.
//
// No dependency, nothing installed, plain node. It is under test/ because
// nothing under test/ reaches a box, and because it is the same kind of thing
// as openapi-types.js beside it: a tool this tree carries for whoever works on
// it.
//
//   node test/web/serve.js 192.168.1.69          the box, port 80
//   node test/web/serve.js 192.168.1.69:8081     a box answering elsewhere
//   PORT=9000 node test/web/serve.js <box>       serve on another port here
//
// Then open http://localhost:8080 and edit the files in data/ni-web. A reload
// in the browser is the whole of the edit cycle; there is nothing to rebuild.
//
// WHAT IT DOES NOT DO. It does not know the box's password. Signing in happens
// in the browser as it does anywhere, the session cookie travels through here
// like every other header, and a request the box refuses is refused here too.
'use strict';

const http = require('http');
const fs = require('fs');
const path = require('path');

const box = process.argv[2];
if (!box) {
	process.stderr.write('usage: node test/web/serve.js <box>[:port]\n');
	process.exit(2);
}
const boxHost = box.split(':')[0];
const boxPort = Number(box.split(':')[1] || 80);
const here = Number(process.env.PORT || 8080);
const root = path.resolve(__dirname, '..', '..', 'data', 'ni-web');

/* What the box answers and this does not. Everything else is a file of this
   tree; a name that is neither is a 404 from here, which is what it would be
   on a box. */
const toBox = ['/api/', '/control/', '/y/', '/fb/', '/logos/', '/images/'];

/* Said rather than guessed from the name, because a wrong type here is a page
   the browser refuses to run and an hour spent looking at the wrong half. */
const types = {
	'.html': 'text/html; charset=utf-8',
	'.js': 'text/javascript; charset=utf-8',
	'.mjs': 'text/javascript; charset=utf-8',
	'.css': 'text/css; charset=utf-8',
	'.json': 'application/json; charset=utf-8',
	'.svg': 'image/svg+xml',
	'.png': 'image/png',
	'.webp': 'image/webp',
	'.ico': 'image/x-icon',
	'.woff2': 'font/woff2',
	'.map': 'application/json; charset=utf-8',
};

function pass(req, res) {
	const out = http.request({
		host: boxHost,
		port: boxPort,
		method: req.method,
		path: req.url,
		headers: Object.assign({}, req.headers, { host: boxHost + ':' + boxPort }),
	}, function (answer) {
		res.writeHead(answer.statusCode || 502, answer.headers);
		answer.pipe(res);
	});
	out.on('error', function (e) {
		res.writeHead(502, { 'Content-Type': 'text/plain; charset=utf-8' });
		res.end('the box at ' + boxHost + ':' + boxPort + ' did not answer: ' + e.message + '\n');
	});
	req.pipe(out);
}

function serve(req, res) {
	/* The interface is one page: every address inside it is answered by
	   index.html and the router works out which screen that is. The same rule
	   the box's own static server follows (src/httpd/apppaths.cpp). */
	let name = req.url.split('?')[0];
	if (name === '/' || name === '') {
		name = '/index.html';
	}
	const full = path.join(root, path.normalize(name).replace(/^(\.\.[/\\])+/, ''));
	if (full.indexOf(root) !== 0) {
		res.writeHead(403).end();
		return;
	}
	fs.stat(full, function (bad, st) {
		if (bad || !st.isFile()) {
			if (name.indexOf('.') === -1) {
				serveFile(path.join(root, 'index.html'), res);
				return;
			}
			res.writeHead(404, { 'Content-Type': 'text/plain; charset=utf-8' });
			res.end('not in this tree: ' + name + '\n');
			return;
		}
		serveFile(full, res);
	});
}

function serveFile(full, res) {
	const type = types[path.extname(full)] || 'application/octet-stream';
	// Never cached. The point of this is that a reload shows the edit.
	res.writeHead(200, { 'Content-Type': type, 'Cache-Control': 'no-store' });
	fs.createReadStream(full).pipe(res);
}

http.createServer(function (req, res) {
	for (const prefix of toBox) {
		if (req.url.indexOf(prefix) === 0) {
			pass(req, res);
			return;
		}
	}
	serve(req, res);
}).listen(here, function () {
	process.stdout.write('serve.js: ' + root + ' on http://localhost:' + here
		+ ', asking ' + boxHost + ':' + boxPort + ' for the rest\n');
});
