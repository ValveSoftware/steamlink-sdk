// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// File Description:
//     Contains all the necessary functions for rendering the Welcome page on
//     Android.

cr.define('welcome', function() {
  'use strict';

  function initialize() {
    // Disable context menus.
    document.body.oncontextmenu = function(e) {
      e.preventDefault();
    }

    $('settings').onclick = function() {
      chrome.send('showSyncSettings');
    };

    var tosLink = $('tos-link');
    if (tosLink) {
      tosLink.onclick = function() {
        chrome.send('showTermsOfService');
      };
    }

    // Set visibility of terms of service footer.
    $('tos-footer').hidden = !loadTimeData.getBoolean('tosVisible');

    // Set the initial visibility for the sync footer.
    chrome.send('updateSyncFooterVisibility');
  }

  /**
   * Sets the visibility of the sync footer.
   * @param {boolean} isVisible Whether the sync footer should be visible.
   */
  function setSyncFooterVisible(isVisible) {
    $('sync-footer').hidden = !isVisible;
  }

  // Return an object with all of the exports.
  return {
    initialize: initialize,
    setSyncFooterVisible: setSyncFooterVisible,
  };
});

document.addEventListener('DOMContentLoaded', welcome.initialize);
