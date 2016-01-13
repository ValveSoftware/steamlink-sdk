// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

cr.define('options', function() {

  /**
   * Encapsulated handling of the keyboard overlay.
   * @constructor
   */
  function KeyboardOverlay() {
    options.SettingsDialog.call(this, 'keyboard-overlay',
        loadTimeData.getString('keyboardOverlayTitle'),
        'keyboard-overlay',
        $('keyboard-confirm'), $('keyboard-cancel'));
  }

  cr.addSingletonGetter(KeyboardOverlay);

  KeyboardOverlay.prototype = {
    __proto__: options.SettingsDialog.prototype,

    /**
     * Initializes the page. This method is called in initialize.
     */
    initializePage: function() {
      options.SettingsDialog.prototype.initializePage.call(this);

      $('languages-and-input-settings').onclick = function(e) {
        OptionsPage.navigateToPage('languages');
        chrome.send('coreOptionsUserMetricsAction',
                    ['Options_KeyboardShowLanguageSettings']);
      };

      $('keyboard-shortcuts').onclick = function(e) {
        chrome.send('showKeyboardShortcuts');
        chrome.send('coreOptionsUserMetricsAction',
                    ['Options_KeyboardShowKeyboardShortcuts']);
      };
    },

    /**
     * Show/hide the caps lock remapping section.
     * @private
     */
    showCapsLockOptions_: function(show) {
      $('caps-lock-remapping-section').hidden = !show;
    },

    /**
     * Show/hide the diamond key remapping section.
     * @private
     */
    showDiamondKeyOptions_: function(show) {
      $('diamond-key-remapping-section').hidden = !show;
    },
  };

  // Forward public APIs to private implementations.
  [
    'showCapsLockOptions',
    'showDiamondKeyOptions',
  ].forEach(function(name) {
    KeyboardOverlay[name] = function() {
      var instance = KeyboardOverlay.getInstance();
      return instance[name + '_'].apply(instance, arguments);
    };
  });

  // Export
  return {
    KeyboardOverlay: KeyboardOverlay
  };
});
