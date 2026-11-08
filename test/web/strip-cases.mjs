// Read by check-web-strip.sh: holds data/ni-web/minify.sh, which hands the
// JavaScript and CSS of the page to esbuild, to two things.
//
// First, a handful of cases a minifier gets wrong, run in a sandbox before
// and after: the results have to be the same. Among them the ones a lexer
// trips over (comment marks inside nested templates, regular expressions,
// statements without a semicolon), the licence comment, and the script being
// called like cp.
//
// Second, every file the install rules hand to it. esbuild rewrites syntax
// (joins declarations, turns if into &&), so a module cannot be compared
// token for token. What the box needs to
// stay the same is held instead: the module parses without an error, it
// imports the same names from the same addresses and exports the same names,
// every html`` template carries exactly the markup it had, and no comment is
// left but a licence. A stylesheet keeps every class, id, custom property and
// keyframes name it had. Every other file is copied byte for byte.
// Usage: node strip-cases.mjs <top source dir> <typescript module dir>
import { createRequire } from 'node:module';
import { spawnSync } from 'node:child_process';
import fs from 'node:fs';
import path from 'node:path';
import vm from 'node:vm';

const [SRC, TSDIR] = process.argv.slice(2);
const ts = createRequire(import.meta.url)(TSDIR);
const K = ts.SyntaxKind;
const WEB = path.join(SRC, 'data/ni-web');
const MINIFY = path.join(WEB, 'minify.sh');
const ESBUILD = process.env.ESBUILD;

let failed = 0;
let checked = 0;
const fail = (msg) => { failed++; console.error('strip-cases: ' + msg); };
const tmpdir = () => fs.mkdtempSync(path.join(fs.realpathSync('/tmp'), 'minify-'));

function run(src, dst) {
	return spawnSync('sh', [MINIFY, ESBUILD, src, dst], { encoding: 'utf8' });
}

function minify(file, quiet) {
	const tmp = tmpdir();
	const out = path.join(tmp, path.basename(file));
	try {
		const r = run(file, out);
		if (r.status !== 0) throw new Error(r.stderr.trim() || 'minify.sh failed on ' + file);
		if (r.stderr.trim() && !quiet) console.error(`strip-cases: ${path.relative(WEB, file)}: ${r.stderr.trim()}`);
		return fs.readFileSync(out, 'utf8');
	} finally { fs.rmSync(tmp, { recursive: true, force: true }); }
}

function minifyText(text, ext) {
	const tmp = tmpdir();
	const f = path.join(tmp, 'case' + ext);
	fs.writeFileSync(f, text);
	try { return minify(f, true); } finally { fs.rmSync(tmp, { recursive: true, force: true }); }
}

// What a module offers and asks for, and the markup it draws.
function surface(text) {
	const sf = ts.createSourceFile('x.js', text, ts.ScriptTarget.Latest, true, ts.ScriptKind.JS);
	const imports = [];
	const exports = [];
	const templates = [];
	let comments = false;
	const name = (n) => (n.kind === K.StringLiteral ? n.text : n.getText(sf));
	const visit = (node) => {
		if (node.kind >= K.FirstJSDocNode && node.kind <= K.LastJSDocNode) return;
		if (ts.isImportDeclaration(node)) {
			const from = node.moduleSpecifier.text;
			const c = node.importClause;
			if (!c) imports.push(`${from}|`);
			else {
				if (c.name) imports.push(`${from}|default`);
				const b = c.namedBindings;
				if (b && ts.isNamespaceImport(b)) imports.push(`${from}|*`);
				if (b && ts.isNamedImports(b))
					b.elements.forEach((e) => imports.push(`${from}|${name(e.propertyName || e.name)}`));
			}
		} else if (ts.isCallExpression(node) && node.expression.kind === K.ImportKeyword) {
			const a = node.arguments[0];
			imports.push(`import(${a && ts.isStringLiteralLike(a) ? a.text : '?'})`);
		} else if (ts.isExportDeclaration(node)) {
			const from = node.moduleSpecifier ? node.moduleSpecifier.text : '';
			if (!node.exportClause) exports.push(`*from ${from}`);
			else if (ts.isNamespaceExport(node.exportClause)) exports.push(`${name(node.exportClause.name)} from ${from}`);
			else node.exportClause.elements.forEach((e) => exports.push(name(e.name) + (from ? ` from ${from}` : '')));
		} else if (ts.isExportAssignment(node)) exports.push('default');
		else if (node.modifiers && node.modifiers.some((m) => m.kind === K.ExportKeyword)) {
			const isDefault = node.modifiers.some((m) => m.kind === K.DefaultKeyword);
			if (isDefault) exports.push('default');
			else if (ts.isVariableStatement(node))
				node.declarationList.declarations.forEach((d) => exports.push(d.name.getText(sf)));
			else if (node.name) exports.push(node.name.getText(sf));
		}
		if (ts.isTaggedTemplateExpression(node)) {
			const t = node.template;
			const pieces = ts.isNoSubstitutionTemplateLiteral(t)
				? [t.rawText]
				: [t.head.rawText, ...t.templateSpans.map((s) => s.literal.rawText)];
			templates.push(pieces.join('\u0000${}\u0000'));
		}
		const kids = node.getChildren(sf);
		if (kids.length === 0) {
			const r = ts.getLeadingCommentRanges(text, node.pos) || [];
			if (r.some((c) => !/^\/\*!|@license|@preserve/.test(text.slice(c.pos, c.end)))) comments = true;
		}
		kids.forEach(visit);
	};
	visit(sf);
	return { errors: sf.parseDiagnostics.length, imports: imports.sort(), exports: exports.sort(), templates: templates.sort(), comments };
}

function sameJs(name, before, after) {
	checked++;
	const a = surface(before);
	const b = surface(after);
	if (b.errors) return fail(`${name}: ${b.errors} parse errors after minifying (${a.errors} before)`);
	for (const what of ['imports', 'exports', 'templates']) {
		const x = a[what].join('\n');
		const y = b[what].join('\n');
		if (x !== y) {
			const lost = a[what].find((v) => !b[what].includes(v)) ?? b[what].find((v) => !a[what].includes(v));
			return fail(`${name}: ${what} differ, e.g. ${JSON.stringify(lost).slice(0, 160)}`);
		}
	}
	if (b.comments) fail(`${name}: still carries a comment`);
}

function cssNames(text) {
	const bare = text.replace(/\/\*[\s\S]*?\*\//g, '').replace(/"(?:\\.|[^"\\])*"|'(?:\\.|[^'\\])*'/g, '""')
		.replace(/url\([^)]*\)/g, 'url()');
	const set = (re) => [...new Set([...bare.matchAll(re)].map((m) => m[1]))].sort().join(' ');
	return {
		classes: set(/\.(-?[_a-zA-Z][\w-]*)/g),
		ids: set(/#(-?[_a-zA-Z][\w-]*)(?=[^;{}]*\{)/g),
		properties: set(/(--[\w-]+)/g),
		keyframes: set(/@keyframes\s+([\w-]+)/g),
	};
}

function sameCss(name, before, after) {
	checked++;
	const a = cssNames(before);
	const b = cssNames(after);
	for (const what of Object.keys(a))
		if (a[what] !== b[what]) return fail(`${name}: ${what} differ: ${a[what].slice(0, 120)} / ${b[what].slice(0, 120)}`);
	if (/\/\*(?!!)/.test(after.replace(/"(?:\\.|[^"\\])*"|'(?:\\.|[^'\\])*'/g, '""'))) fail(`${name}: still carries a comment`);
}

// The cases, run as classic scripts before and after: out is what they leave.
function runScript(code) {
	const ctx = { out: undefined };
	vm.runInNewContext(code, ctx, { timeout: 1000 });
	return JSON.stringify(ctx.out);
}
const tag = 'function html(s, ...v) { return s.raw.join("|") + "#" + v.join(","); }\n';
const cases = [
	['comment marks in nested templates', tag + 'var b = true, c = 1; out = html`<a href="//x">${b ? html`<i>//y /* z */ ${c}</i>` : null}</a>`;'],
	['indentation in a template stays', tag + 'out = html`<p>\n\t\t${1}\n\t</p>`;'],
	['regular expressions', 'out = [/\\/\\/[/*]/.test("//"), "a/b".replace(/[/]/, "-"), 4 / 2 / 1];'],
	['statements without a semicolon', 'var a = 1\nvar b = a\nb++\n++a\nvar f = function () { return\n1 }\nout = [a, b, f()]'],
	['signs and divisions', 'var a = 1, b = 2, c = 3; out = [a + +b, a - -b, a - --b, b / /c/.source.length];'],
	['umlauts stay as they are', 'out = "Größe";'],
	['names stay as they are', 'function keyOf(method, url) { var joined = method + " " + url; return joined; }\nout = keyOf("GET", "/x");'],
];
for (const [name, code] of cases) {
	checked++;
	let min;
	try { min = minifyText(code, '.js'); } catch (e) { fail(`case "${name}": ${e.message}`); continue; }
	const a = runScript(code);
	let b;
	try { b = runScript(min); } catch (e) { fail(`case "${name}": the minified code throws: ${e.message}`); continue; }
	if (a !== b) fail(`case "${name}": ${a} before, ${b} after`);
	if (name.startsWith('umlauts') && !min.includes('Größe')) fail(`case "${name}": ${JSON.stringify(min)}`);
	if (name.startsWith('indentation') && !min.includes('<p>\n\t\t${')) fail(`case "${name}": ${JSON.stringify(min)}`);
	if (name.startsWith('names') && !/function keyOf\(method,url\)/.test(min)) fail(`case "${name}": ${JSON.stringify(min)}`);
}
{
	const min = minifyText('/*! kept */\n/* @license kept too */\n// gone\nexport const a = 1;\n', '.js');
	checked++;
	if (!min.includes('/*! kept */') || !min.includes('@license kept too') || min.includes('gone'))
		fail(`case "licence kept, comments gone": ${JSON.stringify(min)}`);
}
{
	// esbuild writes nothing newer than the floor the type checker holds the
	// pages to
	const target = (fs.readFileSync(path.join(WEB, 'jsconfig.json'), 'utf8').match(/"target"\s*:\s*"([^"]+)"/) || [])[1];
	checked++;
	const code = fs.readFileSync(MINIFY, 'utf8').split('\n').filter((l) => !/^\s*#/.test(l)).join('\n');
	if (!target || !code.includes(`--target=${target.toLowerCase()}`))
		fail(`case "target": minify.sh does not hand esbuild --target=${String(target).toLowerCase()} from jsconfig.json`);
}
{
	// Called like cp by the install rules: the target holds the result and
	// nothing is left beside it; a file esbuild refuses leaves the target as it
	// was; anything else is copied byte for byte.
	const tmp = tmpdir();
	const src = path.join(tmp, 'in.js');
	const dst = path.join(tmp, 'out.js');
	fs.writeFileSync(src, 'export const a = 1; // gone\n');
	fs.writeFileSync(dst, 'old');
	const r = run(src, dst);
	checked++;
	if (r.status !== 0 || fs.readFileSync(dst, 'utf8') !== 'export const a=1;\n')
		fail(`case "source and target": got ${JSON.stringify(fs.readFileSync(dst, 'utf8'))}`);
	checked++;
	if (fs.readdirSync(tmp).sort().join(' ') !== 'in.js out.js') fail('case "source and target": left a file behind');
	fs.writeFileSync(src, 'export const = ;\n');
	const bad = run(src, dst);
	checked++;
	if (bad.status === 0 || fs.readFileSync(dst, 'utf8') !== 'export const a=1;\n' || fs.readdirSync(tmp).length !== 2)
		fail('case "source and target": a file esbuild refuses changed the target or left a file behind');
	const bin = path.join(tmp, 'x.png');
	const binOut = path.join(tmp, 'y.png');
	const bytes = Buffer.from([0x89, 0x50, 0x4e, 0x47, 0x2f, 0x2a, 0x00, 0xff]);
	fs.writeFileSync(bin, bytes);
	checked++;
	if (run(bin, binOut).status !== 0 || !fs.readFileSync(binOut).equals(bytes)) fail('case "other files pass byte for byte": changed');
	fs.rmSync(tmp, { recursive: true, force: true });
}

// Every file the install rules hand to it.
const walked = ['app', 'info', 'ai'];
const files = ['index.html', 'swagger/index.html', 'fonts/ubuntu.css'];
const walk = (d) => fs.readdirSync(path.join(WEB, d), { withFileTypes: true }).forEach((e) => {
	const rel = path.join(d, e.name);
	if (e.isDirectory()) walk(rel);
	else if (!/^Makefile/.test(e.name)) files.push(rel);
});
walked.forEach(walk);
let before = 0;
let after = 0;
for (const rel of files) {
	const file = path.join(WEB, rel);
	const raw = fs.readFileSync(file);
	let out;
	try { out = minify(file); } catch (e) { fail(`${rel}: ${e.message}`); continue; }
	before += raw.length;
	after += Buffer.byteLength(out);
	if (/\.(js|mjs)$/.test(rel)) sameJs(rel, raw.toString('utf8'), out);
	else if (rel.endsWith('.css')) sameCss(rel, raw.toString('utf8'), out);
	else {
		checked++;
		if (!Buffer.from(out).equals(raw) && !(out === raw.toString('utf8'))) fail(`${rel}: not copied as it is`);
	}
}

if (failed) {
	console.error(`strip-cases: ${failed} of ${checked} failed`);
	process.exit(1);
}
console.log(`strip-cases: ${checked} checked, ${files.length} files hold what they held, ${before} -> ${after} bytes`);
