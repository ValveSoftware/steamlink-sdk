// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

/**
 * @fileoverview
 * 'settings-subpage' shows a subpage beneath a subheader. The header contains
 * the subpage title and a back icon. The back icon fires an event which
 * is caught by settings-animated-pages, so it requires no separate handling.
 */

Polymer({
  is: 'settings-subpage',

  behaviors: [
    // TODO(michaelpg): phase out NeonAnimatableBehavior.
    Polymer.NeonAnimatableBehavior,
    Polymer.IronResizableBehavior,
  ],

  properties: {
    pageTitle: String,
  },

  /** @private */
  onTapBack_: function() {
    // Event is caught by settings-animated-pages.
    this.fire('subpage-back');
  },
});
