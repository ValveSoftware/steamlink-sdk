// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

cr.define('hotword', function() {
  'use strict';

  /**
   * Class used to manage always-on hotwording. Automatically starts hotwording
   * on startup, if always-on is enabled, and starts/stops hotwording at
   * appropriate times.
   * @param {!hotword.StateManager} stateManager
   * @constructor
   * @extends {hotword.BaseSessionManager}
   */
  function AlwaysOnManager(stateManager) {
    hotword.BaseSessionManager.call(this,
                                    stateManager,
                                    hotword.constants.SessionSource.ALWAYS);
  }

  AlwaysOnManager.prototype = {
    __proto__: hotword.BaseSessionManager.prototype,

    /** @override */
     enabled: function() {
       return this.stateManager.isAlwaysOnEnabled();
     },

    /** @override */
    updateListeners: function() {
      hotword.BaseSessionManager.prototype.updateListeners.call(this);
      if (this.enabled())
        this.startSession();
    }
  };

  return {
    AlwaysOnManager: AlwaysOnManager
  };
});
