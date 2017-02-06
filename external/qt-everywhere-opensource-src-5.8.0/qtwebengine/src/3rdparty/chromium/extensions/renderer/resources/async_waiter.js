// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

define('async_waiter', [
    'mojo/public/js/support',
], function(supportModule) {
  /**
   * @module async_waiter
   */

  /**
   * @callback module:async_waiter.AsyncWaiter.Callback
   * @param {number} result The result of waiting.
   */

  /**
   * A waiter that waits for a handle to be ready for either reading or writing.
   * @param {!MojoHandle} handle The handle to wait on.
   * @param {number} signals The signals to wait for handle to be ready for.
   * @param {module:async_waiter.AsyncWaiter.Callback} callback The callback to
   *     call when handle is ready.
   * @constructor
   * @alias module:async_waiter.AsyncWaiter
   */
  function AsyncWaiter(handle, signals, callback) {
    /**
     * The handle to wait on.
     * @type {!MojoHandle}
     * @private
     */
    this.handle_ = handle;

    /**
     * The signals to wait for.
     * @type {number}
     * @private
     */
    this.signals_ = signals;

    /**
     * The callback to invoke when
     * |[handle_]{@link module:async_waiter.AsyncWaiter#handle_}| is ready.
     * @type {module:async_waiter.AsyncWaiter.Callback}
     * @private
     */
    this.callback_ = callback;
    this.id_ = null;
  }

  /**
   * Start waiting for the handle to be ready.
   * @throws Will throw if this is already waiting.
   */
  AsyncWaiter.prototype.start = function() {
    if (this.id_)
      throw new Error('Already started');
    this.id_ = supportModule.asyncWait(
        this.handle_, this.signals_, this.onHandleReady_.bind(this));
  };

  /**
   * Stop waiting for the handle to be ready.
   */
  AsyncWaiter.prototype.stop = function() {
    if (!this.id_)
      return;

    supportModule.cancelWait(this.id_);
    this.id_ = null;
  };

  /**
   * Returns whether this {@link AsyncWaiter} is waiting.
   * @return {boolean} Whether this AsyncWaiter is waiting.
   */
  AsyncWaiter.prototype.isWaiting = function() {
    return !!this.id_;
  };

  /**
   * Invoked when |[handle_]{@link module:async_waiter.AsyncWaiter#handle_}| is
   * ready.
   * @param {number} result The result of the wait.
   * @private
   */
  AsyncWaiter.prototype.onHandleReady_ = function(result) {
    this.id_ = null;
    this.callback_(result);
  };

  return {AsyncWaiter: AsyncWaiter};
});
