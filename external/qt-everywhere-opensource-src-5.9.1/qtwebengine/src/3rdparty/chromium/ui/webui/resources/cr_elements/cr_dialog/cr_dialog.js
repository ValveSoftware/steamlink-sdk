// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

/**
 * @fileoverview 'cr-dialog' is a component for showing a modal dialog. If the
 * dialog is closed via close(), a 'close' event is fired. If the dialog is
 * canceled via cancel(), a 'cancel' event is fired followed by a 'close' event.
 * Additionally clients can inspect the dialog's |returnValue| property inside
 * the 'close' event listener to determine whether it was canceled or just
 * closed, where a truthy value means success, and a falsy value means it was
 * canceled.
 */
Polymer({
  is: 'cr-dialog',
  extends: 'dialog',

  properties: {
    /**
     * Alt-text for the dialog close button.
     */
    closeText: String,

    /**
     * True if the dialog should remain open on 'popstate' events. This is used
     * for navigable dialogs that have their separate navigation handling code.
     */
    ignorePopstate: {
      type: Boolean,
      value: false,
    },
  },

  /** @override */
  ready: function() {
    // If the active history entry changes (i.e. user clicks back button),
    // all open dialogs should be cancelled.
    window.addEventListener('popstate', function() {
      if (!this.ignorePopstate && this.open)
        this.cancel();
    }.bind(this));
  },

  cancel: function() {
    this.fire('cancel');
    HTMLDialogElement.prototype.close.call(this, '');
  },

  /**
   * @param {string=} opt_returnValue
   * @override
   */
  close: function(opt_returnValue) {
    HTMLDialogElement.prototype.close.call(this, 'success');
  },

  /** @return {!PaperIconButtonElement} */
  getCloseButton: function() {
    return this.$.close;
  },
});
