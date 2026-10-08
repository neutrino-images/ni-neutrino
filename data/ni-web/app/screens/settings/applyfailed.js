/* The box took a value and could not put it in force.
 *
 * Told over the stream, and only to the session that wrote, so the line is always about
 * something the person in front of this page did. It names the settings by the words of
 * the box's declaration, read when the event arrives and not kept: a box dependent label
 * is the box's to change.
 */
import * as events from '../../events.js';
import * as store from '../../store.js';
import { rowsOf } from './model.js';
import { applyFailedText } from './refusal.js';

/**
 * @param {(message: string, kind?: string) => unknown} show where the line goes, the page's toast
 * @returns {() => void} stops the watching
 */
export function watchApplyFailed(show) {
	return events.on('setting-apply-failed', function (event) {
		store.load('GET', '/api/v1/settings/schema', {}).then(function (schema) {
			return rowsOf(/** @type {{ items?: unknown, value_lists?: unknown }} */ (schema));
		}, function () {
			return [];
		}).then(function (rows) {
			/** @type {Record<string, string>} */
			const names = {};
			for (const row of rows)
				names[row.id] = row.label;
			show(applyFailedText(event, function (key) { return names[key] || key; }), 'bad');
		});
	});
}
