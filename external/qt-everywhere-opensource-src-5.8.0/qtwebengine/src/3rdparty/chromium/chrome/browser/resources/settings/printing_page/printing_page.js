// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

Polymer({
  is: 'settings-printing-page',

  properties: {
    prefs: {
      type: Object,
      notify: true,
    },
  },

  /** @private */
  onManageTap_: function() {
    window.open(loadTimeData.getString('devicesUrl'));
  },
});
