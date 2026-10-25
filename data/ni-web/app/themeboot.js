// Sets the theme and the face before the first paint. A module script runs after the
// first paint, which would flash the wrong scheme or the wrong face for someone who
// chose the other one. Keys and values are those of app/ui/theme.js and
// app/ui/font.js.
try {
	var saved = window.localStorage.getItem('ni-web.theme');
	if (saved === 'light' || saved === 'dark')
		document.documentElement.setAttribute('data-theme', saved);
	if (window.localStorage.getItem('ni-web.font') === 'system')
		document.documentElement.setAttribute('data-font', 'system');
} catch (e) {
	// No site data: the system scheme and the shipped face apply.
}
