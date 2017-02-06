// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

/**
 * @fileoverview 'user-manager-dialog' is a modal dialog for the user manager.
 */
Polymer({
  is: 'user-manager-dialog',

  behaviors: [Polymer.PaperDialogBehavior],

  properties: {
    /** @override */
    noCancelOnOutsideClick: {
      type: Boolean,
      value: true
    },

    /** @override */
    withBackdrop: {
      type: Boolean,
      value: true
    },

    /**
     * The first node that can receive focus.
     * @type {!Node}
     */
    firstFocusableNode: {
      type: Object,
      value: function() { return this.$.close; },
      observer: 'firstFocusableNodeChanged_'
    },

    /**
     * The last node that can receive focus.
     * @type {!Node}
     */
    lastFocusableNode: {
      type: Object,
      value: function() { return this.$.close; },
      observer: 'lastFocusableNodeChanged_'
    }
  },

  /**
   * Returns the first and the last focusable elements in order to wrap the
   * focus for the dialog in Polymer.PaperDialogBehavior.
   * @override
   * @type {!Array<!Node>}
   */
  get _focusableNodes() {
    return [this.firstFocusableNode, this.lastFocusableNode];
  },

  /**
   * Observer for firstFocusableNode. Updates __firstFocusableNode in
   * Polymer.PaperDialogBehavior.
   */
  firstFocusableNodeChanged_: function(newValue) {
    this.__firstFocusableNode = newValue;
  },

  /**
   * Observer for lastFocusableNodeChanged_. Updates __lastFocusableNode in
   * Polymer.PaperDialogBehavior.
   */
  lastFocusableNodeChanged_: function(newValue) {
    this.__lastFocusableNode = newValue;
  }
});
