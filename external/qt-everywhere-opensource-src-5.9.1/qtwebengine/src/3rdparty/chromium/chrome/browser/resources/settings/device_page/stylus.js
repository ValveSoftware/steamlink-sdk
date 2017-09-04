// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

/**
 * @fileoverview
 * 'settings-stylus' is the settings subpage with stylus-specific settings.
 */

Polymer({
  is: 'settings-stylus',

  properties: {
    /** Preferences state. */
    prefs: {
      type: Object,
      notify: true,
    },
  }
});
