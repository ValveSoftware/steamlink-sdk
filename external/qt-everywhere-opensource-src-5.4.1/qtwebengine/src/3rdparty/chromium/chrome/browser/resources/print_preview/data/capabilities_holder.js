// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

cr.define('print_preview', function() {
  'use strict';

  /**
   * Mutable reference to a CDD object.
   * @constructor
   */
  function CapabilitiesHolder() {
    /**
     * Reference to the capabilities object.
     * @type {print_preview.Cdd}
     * @private
     */
    this.capabilities_ = null;
  };

  CapabilitiesHolder.prototype = {
    /** @return {print_preview.Cdd} The instance held by the holder. */
    get: function() {
      return this.capabilities_;
    },

    /** @param {!print_preview.Cdd} New instance to put into the holder. */
    set: function(capabilities) {
      this.capabilities_ = capabilities;
    }
  };

  // Export
  return {
    CapabilitiesHolder: CapabilitiesHolder
  };
});
