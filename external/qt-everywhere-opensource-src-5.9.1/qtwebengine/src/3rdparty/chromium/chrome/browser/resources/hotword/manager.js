// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

(function() {
  'use strict';

  /**
   * @fileoverview This extension provides hotword triggering capabilites to
   * Chrome.
   *
   * This extension contains all the JavaScript for loading and managing the
   * hotword detector. The hotword detector and language model data will be
   * provided by a shared module loaded from the web store.
   *
   * IMPORTANT! Whenever adding new events, the extension version number MUST be
   * incremented.
   */

  // Hotwording state.
  var stateManager = new hotword.StateManager();
  var pageAudioManager = new hotword.PageAudioManager(stateManager);
  var alwaysOnManager = new hotword.AlwaysOnManager(stateManager);
  var launcherManager = new hotword.LauncherManager(stateManager);
  var trainingManager = new hotword.TrainingManager(stateManager);

  // Detect when hotword settings have changed.
  chrome.hotwordPrivate.onEnabledChanged.addListener(function() {
    stateManager.updateStatus();
  });

  // Detect a request to delete the speaker model.
  chrome.hotwordPrivate.onDeleteSpeakerModel.addListener(function() {
    hotword.TrainingManager.handleDeleteSpeakerModel();
  });

  // Detect a request for the speaker model existence.
  chrome.hotwordPrivate.onSpeakerModelExists.addListener(function() {
    hotword.TrainingManager.handleSpeakerModelExists();
  });

  // Detect when the shared module containing the NaCL module and language model
  // is installed.
  chrome.management.onInstalled.addListener(function(info) {
    if (info.id == hotword.constants.SHARED_MODULE_ID) {
      hotword.debug('Shared module installed, reloading extension.');
      chrome.runtime.reload();
    }
  });
}());
