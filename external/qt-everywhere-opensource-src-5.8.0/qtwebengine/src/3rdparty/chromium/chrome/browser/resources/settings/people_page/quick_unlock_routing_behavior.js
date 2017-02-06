// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

/** @enum {string} */
var QuickUnlockScreen = {
  AUTHENTICATE: 'quick-unlock-authenticate',
  CHOOSE_METHOD: 'quick-unlock-choose-method',
  SETUP_PIN: 'quick-unlock-setup-pin'
};

/** @polymerBehavior */
var QuickUnlockRoutingBehavior = {
  properties: {
    currentRoute: {
      type: Object,
      notify: true,
    }
  },

  /**
   * Returns true if the given screen is active.
   * @param {!QuickUnlockScreen} screen
   * @return {!boolean}
   */
  isScreenActive: function(screen) {
    var subpage = this.currentRoute.subpage;
    return subpage.length > 0 && subpage[subpage.length - 1] == screen;
  },
};
