// The words the web television block draws, in the two languages the
// interface is written in.
//
// Beside the block and not in the frame's catalogue, for the reason every
// other part of the page keeps its own: nothing here is edited by two people
// at once. Flat keys, one per line, the value a whole sentence, and no HTML
// entity: these are drawn as text and an entity would land on the screen
// spelled out.
export default {
	de: {
		'webtv.address.label': 'Adresse für einen Player',
		'webtv.label': 'Web-TV: {name}',
		'webtv.name.unknown': 'unbenannter Sender',
		'webtv.playing': 'Läuft.',
		'webtv.retry': 'Noch einmal versuchen',
		'webtv.starting': 'Der Sender wird geholt.',
		'webtv.unplayable.body': 'Die Box hat den Stream geholt, dieser Browser kann ihn nicht abspielen. Die Adresse unten lässt sich in einem Player wie VLC öffnen.',
		'webtv.unplayable.title': 'Dieser Sender läuft nicht im Browser',
	},
	en: {
		'webtv.address.label': 'Address for a player',
		'webtv.label': 'Web TV: {name}',
		'webtv.name.unknown': 'unnamed channel',
		'webtv.playing': 'Playing.',
		'webtv.retry': 'Try again',
		'webtv.starting': 'Fetching the channel.',
		'webtv.unplayable.body': 'The box fetched the stream and this browser cannot decode it. The address below opens in a player such as VLC.',
		'webtv.unplayable.title': 'This channel does not play in the browser',
	},
};
