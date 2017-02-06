// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

cr.define('hotword', function() {
  'use strict';

  /**
   * Class used to manage the interaction between hotwording and the launcher
   * (app list).
   * @param {!hotword.StateManager} stateManager
   * @constructor
   * @extends {hotword.BaseSessionManager}
   */
  function LauncherManager(stateManager) {
    hotword.BaseSessionManager.call(this,
                                    stateManager,
                                    hotword.constants.SessionSource.LAUNCHER);
  }

  LauncherManager.prototype = {
    __proto__: hotword.BaseSessionManager.prototype,

    /** @override */
    enabled: function() {
      return this.stateManager.isSometimesOnEnabled();
    },

    /** @override */
    onSessionStop: function() {
      chrome.hotwordPrivate.setHotwordSessionState(false, function() {});
    }
  };

  return {
    LauncherManager: LauncherManager
  };
});
