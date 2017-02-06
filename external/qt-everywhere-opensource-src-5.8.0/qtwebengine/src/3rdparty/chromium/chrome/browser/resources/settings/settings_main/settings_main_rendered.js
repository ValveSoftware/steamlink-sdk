// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

cr.define('settings.main', function() {
  /** @type {!Promise} */
  var promise;

  return {
    /** @return {!Promise} */
    get rendered() {
      return assert(promise, 'rendered promise must be set before accessing');
    },

    /** @param {!Promise} rendered */
    set rendered(rendered) {
      assert(!promise, 'settings.main.rendered should only be set once');
      promise = rendered;
    },
  };
});
