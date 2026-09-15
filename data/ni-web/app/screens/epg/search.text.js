// No sentence here for a query that is too short, and that is the point: the
// floor of two characters is enforced one layer below the endpoint and comes back as
// a refusal that says so, so the answer is drawn. A sentence written here would be a
// second place the same rule is stated.
export default {
	de: {
		'epg.search.head': 'Sendungen im gespeicherten Programm suchen, über alle Sender hinweg.',
		'epg.search.query': 'Suchbegriff',
		'epg.search.hint': 'Gesucht wird im Titel und in beiden Texten einer Sendung.',
		'epg.search.begins': 'Von',
		'epg.search.ends': 'Bis',
		'epg.search.limit': 'Höchstens',
		'epg.search.submit': 'Suchen',
		'epg.search.idle': 'Noch nichts gesucht',
		'epg.search.idle.try': 'Ein Wort aus dem Titel reicht. Probier „Tagesschau“, „Tatort“ oder „Doku“.',
		'epg.search.empty': 'Nichts gefunden',
		'epg.search.empty.try': 'In diesem Zeitraum steht nichts mit diesem Wort. Ein kürzeres Wort oder ein weiterer Zeitraum findet mehr.',
		'epg.search.found': '{count} Treffer',
		'epg.search.truncated': 'Die Box hat die Liste an der Obergrenze abgeschnitten. Ein engerer Zeitraum zeigt den Rest.',
		'epg.search.results': 'Treffer'
	},
	en: {
		'epg.search.head': 'Find programmes in the guide the box holds, across every channel.',
		'epg.search.query': 'Search for',
		'epg.search.hint': 'The name and both texts of an event are searched.',
		'epg.search.begins': 'From',
		'epg.search.ends': 'To',
		'epg.search.limit': 'At most',
		'epg.search.submit': 'Search',
		'epg.search.idle': 'Nothing searched for yet',
		'epg.search.idle.try': 'One word out of the title is enough. Try "news", "film" or "documentary".',
		'epg.search.empty': 'Nothing found',
		'epg.search.empty.try': 'Nothing in this window carries that word. A shorter word or a wider window finds more.',
		'epg.search.found': '{count} hits',
		'epg.search.truncated': 'The box cut the list at its ceiling. A narrower window shows the rest.',
		'epg.search.results': 'Hits'
	}
};
