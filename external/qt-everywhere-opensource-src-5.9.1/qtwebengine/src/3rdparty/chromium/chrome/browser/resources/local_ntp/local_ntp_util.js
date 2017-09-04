// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.


/**
 * @fileoverview Utilities for local_ntp.js.
 */


/**
 * A counter with a callback that gets executed on the 1-to-0 transition.
 *
 * @param {function()} callback The callback to be executed.
 * @constructor
 */
function Barrier(callback) {
  /** @private {function()} */
  this.callback_ = callback;

  /** @private {number} */
  this.count_ = 0;

  /** @private {boolean} */
  this.isCancelled_ = false;
}


/**
 * Increments count of the Barrier.
 */
Barrier.prototype.add = function() {
  ++this.count_;
};


/**
 * Decrements count of the Barrier, and executes callback on 1-to-0 transition.
 */
Barrier.prototype.remove = function() {
  if (this.count_ === 0)  // Guards against underflow.
    return;
  --this.count_;
  if (!this.isCancelled_ && this.count_ === 0)
    this.callback_();
};


/**
 * Deactivates the Barrier; the callback will never be executed.
 */
Barrier.prototype.cancel = function() {
  this.isCancelled_ = true;
};
