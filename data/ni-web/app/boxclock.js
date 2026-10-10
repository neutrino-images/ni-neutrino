/* The box's clock, as far as this page can tell it from the Date header of its
   answers. What the box judges as past or running it judges by its own clock,
   and the device this page runs on can be minutes away from it. */

// Before this the box has not set its clock yet.
const kEarliestMs = Date.UTC(2020, 0, 1);

// A slow answer says too little about when the box wrote its header.
const kLongestTripMs = 4000;

// The header counts whole seconds, so single readings scatter by one.
const kWeight = 0.25;

// Further off than this, the device's own clock was set in between.
const kJumpMs = 30000;

let offsetMs = 0;
let readings = 0;

/**
 * Takes one answer's Date header into the estimate.
 *
 * @param {string | null | undefined} header
 * @param {number} sentMs the device's clock when the request left
 * @param {number} receivedMs the device's clock when the answer arrived
 * @returns {boolean} whether it counted
 */
export function noteBoxDate(header, sentMs, receivedMs) {
	if (typeof header !== 'string' || header === '') {
		return false;
	}
	const said = Date.parse(header);
	const trip = receivedMs - sentMs;
	if (!Number.isFinite(said) || said < kEarliestMs || !(trip >= 0) || trip > kLongestTripMs) {
		return false;
	}
	// Written somewhere inside its second, and somewhere inside the trip.
	const sample = said + 500 - (sentMs + trip / 2);
	if (readings === 0 || Math.abs(sample - offsetMs) > kJumpMs) {
		offsetMs = sample;
	} else {
		offsetMs += (sample - offsetMs) * kWeight;
	}
	readings++;
	return true;
}

/**
 * @returns {number} milliseconds the box's clock is ahead of this device's
 */
export function boxOffsetMs() {
	return Math.round(offsetMs);
}

/**
 * @returns {number} the box's time in milliseconds since the epoch
 */
export function boxNow() {
	return Date.now() + Math.round(offsetMs);
}

/**
 * @returns {number} the box's time in whole seconds since the epoch
 */
export function boxNowSeconds() {
	return Math.floor(boxNow() / 1000);
}

// For the cases that hold the estimate, which start from a device with no readings.
export function forgetBoxDate() {
	offsetMs = 0;
	readings = 0;
}
