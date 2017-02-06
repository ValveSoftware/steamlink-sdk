// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

cr.define('hotword', function() {
  'use strict';

  /**
   * Class used to keep this extension alive. When started, this calls an
   * extension API on a regular basis which resets the event page keep-alive
   * timer.
   * @constructor
   */
  function KeepAlive() {
    this.timeoutId_ = null;
  }

  KeepAlive.prototype = {
    /**
     * Start the keep alive process. Safe to call multiple times.
     */
    start: function() {
      if (this.timeoutId_ == null)
        this.timeoutId_ = setTimeout(this.handleTimeout_.bind(this), 1000);
    },

    /**
     * Stops the keep alive process. Safe to call multiple times.
     */
    stop: function() {
      if (this.timeoutId_ != null) {
        clearTimeout(this.timeoutId_);
        this.timeoutId_ = null;
      }
    },

    /**
     * Handle the timer timeout. Calls an extension API and schedules the next
     * timeout.
     * @private
     */
    handleTimeout_: function() {
      // Dummy extensions API call used to keep this event page alive by
      // resetting the shutdown timer.
      chrome.runtime.getPlatformInfo(function(info) {});

      this.timeoutId_ = setTimeout(this.handleTimeout_.bind(this), 1000);
    }
  };

  return {
    KeepAlive: KeepAlive
  };
});
