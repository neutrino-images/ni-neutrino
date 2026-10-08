// Sets the theme before the first paint. A module script runs after the first
// paint, which would flash the wrong scheme for someone who chose the other one.
// Key and values are those of app/ui/theme.js.
try {
	var saved = window.localStorage.getItem('ni-web.theme');
	if (saved === 'light' || saved === 'dark')
		document.documentElement.setAttribute('data-theme', saved);
} catch (e) {
	// No site data: the system scheme applies.
}
