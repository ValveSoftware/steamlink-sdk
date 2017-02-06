// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

/**
 * @fileoverview
 * 'settings-downloads-page' is the settings page containing downloads
 * settings.
 *
 * Example:
 *
 *    <iron-animated-pages>
 *      <settings-downloads-page prefs="{{prefs}}">
 *      </settings-downloads-page>
 *      ... other pages ...
 *    </iron-animated-pages>
 */
Polymer({
  is: 'settings-downloads-page',

  properties: {
    /**
     * Preferences state.
     */
    prefs: {
      type: Object,
      notify: true,
    },
  },

  /** @private */
  selectDownloadLocation_: function() {
    chrome.send('selectDownloadLocation');
  },
});
