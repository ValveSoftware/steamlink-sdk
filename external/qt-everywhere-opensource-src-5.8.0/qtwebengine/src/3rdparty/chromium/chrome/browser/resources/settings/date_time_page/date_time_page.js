// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

/**
 * @fileoverview
 * 'settings-date-time-page' is the settings page containing date-time
 * settings.
 *
 * Example:
 *
 *    <core-animated-pages>
 *      <settings-date-time-page prefs="{{prefs}}">
 *      </settings-date-time-page>
 *      ... other pages ...
 *    </core-animated-pages>
 */
Polymer({
  is: 'settings-date-time-page',

  properties: {
    /**
     * Preferences state.
     */
    prefs: {
      type: Object,
      notify: true
    },
  },
});
