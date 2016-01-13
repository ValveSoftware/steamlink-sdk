// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

/**
 * @fileoverview App launcher start page implementation.
 */

<include src="recommended_apps.js"/>
<include src="speech_manager.js"/>

cr.define('appList.startPage', function() {
  'use strict';

  var speechManager = null;

  /**
   * Creates a StartPage object.
   * @constructor
   * @extends {HTMLDivElement}
   */
  var StartPage = cr.ui.define('div');

  StartPage.prototype = {
    __proto__: HTMLDivElement.prototype,

    /**
     * Instance of the recommended apps card.
     * @type {appsList.startPage.RecommendedApps}
     * @private
     */
    recommendedApps_: null,

    /** @override */
    decorate: function() {
      this.recommendedApps_ = new appList.startPage.RecommendedApps();
      this.appendChild(this.recommendedApps_);
    },

    /**
     * Sets the recommended apps.
     * @param {!Array.<!{appId: string,
     *                   iconUrl: string,
     *                   textTitle: string}>} apps An array of app info
     *     dictionary to be displayed in the AppItemView.
     */
    setRecommendedApps: function(apps) {
      this.recommendedApps_.setApps(apps);
    }
  };

  /**
   * Initialize the page.
   */
  function initialize() {
    StartPage.decorate($('start-page'));
    speechManager = new speech.SpeechManager();
    chrome.send('initialize');
  }

  /**
   * Sets the recommended apps.
   * @param {Array.<Object>} apps An array of app info dictionary.
   */
  function setRecommendedApps(apps) {
    $('start-page').setRecommendedApps(apps);
  }

  /**
   * Invoked when the hotword plugin availability is changed.
   *
   * @param {boolean} enabled Whether the plugin is enabled or not.
   */
  function setHotwordEnabled(enabled) {
    speechManager.setHotwordEnabled(enabled);
  }

  /**
   * Sets the architecture of NaCl module to be loaded for hotword.
   * @param {string} arch The architecture.
   */
  function setNaclArch(arch) {
    speechManager.setNaclArch(arch);
  }

  /**
   * Invoked when the app-list bubble is shown.
   *
   * @param {boolean} hotwordEnabled Whether the hotword is enabled or not.
   */
  function onAppListShown(hotwordEnabled) {
    speechManager.onShown(hotwordEnabled);
  }

  /**
   * Invoked when the app-list bubble is hidden.
   */
  function onAppListHidden() {
    speechManager.onHidden();
  }

  /**
   * Invoked when the user explicitly wants to toggle the speech recognition
   * state.
   */
  function toggleSpeechRecognition() {
    speechManager.toggleSpeechRecognition();
  }

  return {
    initialize: initialize,
    setRecommendedApps: setRecommendedApps,
    setHotwordEnabled: setHotwordEnabled,
    setNaclArch: setNaclArch,
    onAppListShown: onAppListShown,
    onAppListHidden: onAppListHidden,
    toggleSpeechRecognition: toggleSpeechRecognition
  };
});

document.addEventListener('contextmenu', function(e) { e.preventDefault(); });
document.addEventListener('DOMContentLoaded', appList.startPage.initialize);
