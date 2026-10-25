// The words this display draws, in the two languages the interface is written
// in. There are few of them because a display says almost nothing: the channel
// and the programme are the box's own words, and the five marks along the top
// are abbreviations that read the same in both languages.
//
// The shape is read by a check and not by a person: one object per language,
// one key per line, the value a whole sentence with {name} in it where
// something is filled in.
export default {
	de: {
		'info.asking': 'Die Box wird gefragt',
		'info.away': 'Die Box antwortet nicht',
		'info.denied': 'Kein Zugriff ohne Anmeldung',
		'info.left': 'noch {time}',
		'info.noevent': 'Keine Programmdaten',
		'info.nothing': 'Die Box spielt gerade nichts',
		'info.standby': 'Die Box ist im Standby',
	},
	en: {
		'info.asking': 'Asking the box',
		'info.away': 'The box is not answering',
		'info.denied': 'No access without signing in',
		'info.left': '{time} left',
		'info.noevent': 'No guide data',
		'info.nothing': 'The box is playing nothing',
		'info.standby': 'The box is in standby',
	},
};
