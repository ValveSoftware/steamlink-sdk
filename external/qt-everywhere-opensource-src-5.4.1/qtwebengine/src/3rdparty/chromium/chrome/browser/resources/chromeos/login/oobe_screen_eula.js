// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

/**
 * @fileoverview Oobe eula screen implementation.
 */

login.createScreen('EulaScreen', 'eula', function() {
  return {
    /** @override */
    decorate: function() {
      $('eula-chrome-credits-link').hidden = true;
      $('eula-chromeos-credits-link').hidden = true;
      $('stats-help-link').addEventListener('click', function(event) {
        chrome.send('eulaOnLearnMore');
      });
      $('installation-settings-link').addEventListener(
          'click', function(event) {
            chrome.send('eulaOnInstallationSettingsPopupOpened');
            $('popup-overlay').hidden = false;
            $('installation-settings-ok-button').focus();
          });
      $('installation-settings-ok-button').addEventListener(
          'click', function(event) {
            $('popup-overlay').hidden = true;
          });
      // Do not allow focus leaving the overlay.
      $('popup-overlay').addEventListener('focusout', function(event) {
        // WebKit does not allow immediate focus return.
        window.setTimeout(function() {
          // TODO(ivankr): focus cycling.
          $('installation-settings-ok-button').focus();
        }, 0);
        event.preventDefault();
      });
    },

    /**
     * Event handler that is invoked when 'chrome://terms' is loaded.
     */
    onFrameLoad: function() {
      $('accept-button').disabled = false;
      $('eula').classList.remove('eula-loading');
      // Initially, the back button is focused and the accept button is
      // disabled.
      // Move the focus to the accept button now but only if the user has not
      // moved the focus anywhere in the meantime.
      if (!$('back-button').blurred)
        $('accept-button').focus();
    },

    /**
     * Event handler that is invoked just before the screen is shown.
     * @param {object} data Screen init payload.
     */
    onBeforeShow: function() {
      $('eula').classList.add('eula-loading');
      $('cros-eula-frame').onload = this.onFrameLoad;
      $('accept-button').disabled = true;
      $('cros-eula-frame').src = 'chrome://terms';
    },

    /**
     * Header text of the screen.
     * @type {string}
     */
    get header() {
      return loadTimeData.getString('eulaScreenTitle');
    },

    /**
     * Buttons in oobe wizard's button strip.
     * @type {Array} Array of Buttons.
     */
    get buttons() {
      var buttons = [];

      var backButton = this.ownerDocument.createElement('button');
      backButton.id = 'back-button';
      backButton.textContent = loadTimeData.getString('back');
      backButton.addEventListener('click', function(e) {
        chrome.send('eulaOnExit', [false, $('usage-stats').checked]);
        e.stopPropagation();
      });
      buttons.push(backButton);

      var acceptButton = this.ownerDocument.createElement('button');
      acceptButton.id = 'accept-button';
      acceptButton.disabled = true;
      acceptButton.classList.add('preserve-disabled-state');
      acceptButton.textContent = loadTimeData.getString('acceptAgreement');
      acceptButton.addEventListener('click', function(e) {
        $('eula').classList.add('loading');  // Mark EULA screen busy.
        chrome.send('eulaOnExit', [true, $('usage-stats').checked]);
        e.stopPropagation();
      });
      buttons.push(acceptButton);

      return buttons;
    },

    /**
     * Returns a control which should receive an initial focus.
     */
    get defaultControl() {
      return $('accept-button').disabled ? $('back-button') :
                                           $('accept-button');
    },

    enableKeyboardFlow: function() {
      $('eula-chrome-credits-link').hidden = false;
      $('eula-chromeos-credits-link').hidden = false;
      $('eula-chrome-credits-link').addEventListener('click',
          function(event) {
            chrome.send('eulaOnChromeCredits');
          });
      $('eula-chromeos-credits-link').addEventListener('click',
          function(event) {
            chrome.send('eulaOnChromeOSCredits');
          });
    },

    /**
     * Updates localized content of the screen that is not updated via template.
     */
    updateLocalizedContent: function() {
      // Force iframes to refresh. It's only available method because we have
      // no access to iframe.contentWindow.
      if ($('cros-eula-frame').src) {
        $('cros-eula-frame').src = $('cros-eula-frame').src;
      }
      if ($('oem-eula-frame').src) {
        $('oem-eula-frame').src = $('oem-eula-frame').src;
      }
    }
  };
});

