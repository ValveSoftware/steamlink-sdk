// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

/**
 * @fileoverview Provides a countdown-based timer.
 */
'use strict';

/**
 * A countdown timer.
 * @interface
 */
function Countdown() {}

/**
 * Sets a new timeout for this timer.
 * @param {number} timeoutMillis how long, in milliseconds, the countdown lasts.
 * @param {Function=} cb called back when the countdown expires.
 * @return {boolean} whether the timeout could be set.
 */
Countdown.prototype.setTimeout = function(timeoutMillis, cb) {};

/** Clears this timer's timeout. */
Countdown.prototype.clearTimeout = function() {};

/**
 * @return {number} how many milliseconds are remaining until the timer expires.
 */
Countdown.prototype.millisecondsUntilExpired = function() {};

/** @return {boolean} whether the timer has expired. */
Countdown.prototype.expired = function() {};

/**
 * Constructs a new clone of this timer, while overriding its callback.
 * @param {Function=} cb callback for new timer.
 * @return {!Countdown} new clone.
 */
Countdown.prototype.clone = function(cb) {};

/**
 * Constructs a new timer.  The timer has a very limited resolution, and does
 * not attempt to be millisecond accurate. Its intended use is as a
 * low-precision timer that pauses while debugging.
 * @param {number=} timeoutMillis how long, in milliseconds, the countdown
 *     lasts.
 * @param {Function=} cb called back when the countdown expires.
 * @constructor
 * @implements {Countdown}
 */
function CountdownTimer(timeoutMillis, cb) {
  this.remainingMillis = 0;
  this.setTimeout(timeoutMillis || 0, cb);
}

/** Timer interval */
CountdownTimer.TIMER_INTERVAL_MILLIS = 200;

/**
 * Sets a new timeout for this timer. Only possible if the timer is not
 * currently active.
 * @param {number} timeoutMillis how long, in milliseconds, the countdown lasts.
 * @param {Function=} cb called back when the countdown expires.
 * @return {boolean} whether the timeout could be set.
 */
CountdownTimer.prototype.setTimeout = function(timeoutMillis, cb) {
  if (this.timeoutId)
    return false;
  if (!timeoutMillis || timeoutMillis < 0)
    return false;
  this.remainingMillis = timeoutMillis;
  this.cb = cb;
  if (this.remainingMillis > CountdownTimer.TIMER_INTERVAL_MILLIS) {
    this.timeoutId =
        window.setInterval(this.timerTick.bind(this),
            CountdownTimer.TIMER_INTERVAL_MILLIS);
  } else {
    // Set a one-shot timer for the last interval.
    this.timeoutId =
        window.setTimeout(this.timerTick.bind(this), this.remainingMillis);
  }
  return true;
};

/** Clears this timer's timeout. */
CountdownTimer.prototype.clearTimeout = function() {
  if (this.timeoutId) {
    window.clearTimeout(this.timeoutId);
    this.timeoutId = undefined;
  }
};

/**
 * @return {number} how many milliseconds are remaining until the timer expires.
 */
CountdownTimer.prototype.millisecondsUntilExpired = function() {
  return this.remainingMillis > 0 ? this.remainingMillis : 0;
};

/** @return {boolean} whether the timer has expired. */
CountdownTimer.prototype.expired = function() {
  return this.remainingMillis <= 0;
};

/**
 * Constructs a new clone of this timer, while overriding its callback.
 * @param {Function=} cb callback for new timer.
 * @return {!Countdown} new clone.
 */
CountdownTimer.prototype.clone = function(cb) {
  return new CountdownTimer(this.remainingMillis, cb);
};

/** Timer callback. */
CountdownTimer.prototype.timerTick = function() {
  this.remainingMillis -= CountdownTimer.TIMER_INTERVAL_MILLIS;
  if (this.expired()) {
    window.clearTimeout(this.timeoutId);
    this.timeoutId = undefined;
    if (this.cb) {
      this.cb();
    }
  }
};
