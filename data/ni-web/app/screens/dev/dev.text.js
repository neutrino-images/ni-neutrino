// The words of the toolbox, beside it and nowhere else.
//
// One object per language, flat keys, one key per line, the value a whole
// sentence. This destination is only there on a box built with the API
// documentation, so these are words most boxes never draw at all; they are held
// to the same shape as every other catalogue so that nothing has to remember
// that.
export default {
	de: {
		'dev.api.about': 'Wofür',
		'dev.api.bare': 'Diese Box wurde ohne die Beschreibungen gebaut. Adressen, Gruppen und Rechtestufen stehen trotzdem alle hier; was fehlt, ist der Satz je Route und der Swagger-Leser, den es auf dieser Box nicht gibt.',
		'dev.api.document': 'Das Dokument selbst',
		'dev.api.find': 'Route suchen',
		'dev.api.group': 'Gruppe',
		'dev.api.group.all': 'Alle Gruppen',
		'dev.api.hits': '{count} von {total} Routen',
		'dev.api.lead': 'Jede Adresse, die dieser Server beantwortet, mit der Rechtestufe, die ein Aufrufer dafür mitbringen muss.',
		'dev.api.level': 'Stufe',
		'dev.api.level.all': 'Alle',
		'dev.api.level.public': 'öffentlich',
		'dev.api.level.read': 'lesen',
		'dev.api.level.system': 'System',
		'dev.api.level.unsaid': 'sagt das Dokument nicht',
		'dev.api.level.write': 'schreiben',
		'dev.api.levels': 'Was ein Aufrufer mitbringen muss',
		'dev.api.method': 'Methode',
		'dev.api.none': 'Keine Route passt zu dieser Suche und diesen Filtern.',
		'dev.api.path': 'Adresse',
		'dev.api.reader': 'Im Swagger-Leser öffnen',
		'dev.api.reader.hint': 'Dieselbe Quelle, ausführlich, mit Rümpfen und Antworten. Liegt neben dieser Seite auf der Box.',
		'dev.api.summary': '{count} Routen in {groups} Gruppen, alle aus einer Tabelle.',
		'dev.api.weight': 'Diese Seite lädt das ganze Dokument. Auf einer kleinen Box dauert das einen Moment.',
		'dev.events.away': 'keine Verbindung',
		'dev.events.clear': 'Leeren',
		'dev.events.connected': 'verbunden',
		'dev.events.denied': 'Diese Sitzung darf den Stream nicht lesen.',
		'dev.events.held': 'Die letzten {count} von höchstens {max}. Ältere fallen hinten heraus; mitgeschrieben wird nichts.'
		'dev.events.lead': 'Was die Box von sich aus meldet, mitgelesen. Derselbe Stream, an dem der Rahmen hängt; hier steht er roh.',
		'dev.events.waiting': 'Bisher hat die Box nichts gemeldet.',
	},
	en: {
		'dev.api.about': 'What for',
		'dev.api.bare': 'This box was built without the descriptions. Every address, group and level is still here; what is missing is the sentence per route and the Swagger reader, which this box does not carry.',
		'dev.api.document': 'The document itself',
		'dev.api.find': 'Find a route',
		'dev.api.group': 'Group',
		'dev.api.group.all': 'All groups',
		'dev.api.hits': '{count} of {total} routes',
		'dev.api.lead': 'Every address this server answers, with the level a caller has to carry to reach it.',
		'dev.api.level': 'Level',
		'dev.api.level.all': 'All',
		'dev.api.level.public': 'open to everybody',
		'dev.api.level.read': 'read',
		'dev.api.level.system': 'system',
		'dev.api.level.unsaid': 'the document does not say',
		'dev.api.level.write': 'write',
		'dev.api.levels': 'What a caller has to carry',
		'dev.api.method': 'Method',
		'dev.api.none': 'No route matches this search and these filters.',
		'dev.api.path': 'Address',
		'dev.api.reader': 'Open in the Swagger reader',
		'dev.api.reader.hint': 'The same source, at length, with bodies and answers. It sits beside this page on the box.',
		'dev.api.summary': '{count} routes in {groups} groups, all out of one table.',
		'dev.api.weight': 'This page loads the whole document. On a small box that takes a moment.',
		'dev.events.away': 'no connection',
		'dev.events.clear': 'Empty it',
		'dev.events.connected': 'connected',
		'dev.events.denied': 'This session may not read the stream.',
		'dev.events.held': 'The last {count} of at most {max}. Older ones fall off the end, and nothing is written down.'
		'dev.events.lead': 'What the box reports of its own accord, read along. The same stream the frame hangs on, here in the raw.',
		'dev.events.waiting': 'The box has reported nothing yet.',
	}
};
