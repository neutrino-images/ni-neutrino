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
		'info.asking': 'Die Box wird gefragt.',
		'info.standby': 'Die Box ist im Standby.',
		'info.nothing': 'Die Box spielt gerade nichts.',
		'info.denied': 'Von hier aus gibt die Box ohne Anmeldung nichts her.',
		'info.away': 'Die Box antwortet nicht.',
		'info.noevent': 'Zu diesem Kanal steht nichts im Programm.',
		'info.left': 'noch {time}',
	},
	en: {
		'info.asking': 'Asking the box.',
		'info.standby': 'The box is in standby.',
		'info.nothing': 'The box is playing nothing.',
		'info.denied': 'From here the box gives nothing away without signing in.',
		'info.away': 'The box is not answering.',
		'info.noevent': 'Nothing stands in the guide for this channel.',
		'info.left': '{time} left',
	},
};
