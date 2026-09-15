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
		'webtv.starting': 'Der Sender wird geholt.',
		'webtv.playing': 'Läuft.',
		'webtv.unplayable.title': 'Dieser Sender läuft nicht im Browser',
		'webtv.unplayable.body': 'Die Box hat den Stream geholt, dieser Browser kann ihn nicht abspielen. Die Adresse unten lässt sich in einem Player wie VLC öffnen.',
		'webtv.address.label': 'Adresse für einen Player',
		'webtv.retry': 'Noch einmal versuchen',
		'webtv.label': 'Web-TV: {name}',
		'webtv.name.unknown': 'unbenannter Sender',
	},
	en: {
		'webtv.starting': 'Fetching the channel.',
		'webtv.playing': 'Playing.',
		'webtv.unplayable.title': 'This channel does not play in the browser',
		'webtv.unplayable.body': 'The box fetched the stream and this browser cannot decode it. The address below opens in a player such as VLC.',
		'webtv.address.label': 'Address for a player',
		'webtv.retry': 'Try again',
		'webtv.label': 'Web TV: {name}',
		'webtv.name.unknown': 'unnamed channel',
	},
};
