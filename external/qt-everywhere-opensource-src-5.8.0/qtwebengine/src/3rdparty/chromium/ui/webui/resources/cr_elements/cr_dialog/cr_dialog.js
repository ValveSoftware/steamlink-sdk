// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

/**
 * @fileoverview 'cr-dialog' is a component for showing a modal dialog.
 */
Polymer({
  is: 'cr-dialog',

  properties: {
    /** @override */
    noCancelOnOutsideClick: {
      type: Boolean,
      value: true,
    },

    /** @override */
    noCancelOnEscKey: {
      type: Boolean,
      value: false,
    },

    /**
     * @type {!Element}
     * @override
     */
    sizingTarget: {
      type: Element,
      value: function() {
        return assert(this.$$('.body-container'));
      },
    },

    /** @override */
    withBackdrop: {
      type: Boolean,
      value: true,
    },
  },

  behaviors: [Polymer.PaperDialogBehavior],

  /** @return {!PaperIconButtonElement} */
  getCloseButton: function() {
    return this.$.close;
  },
});
