// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

/**
 * @fileoverview
 * Contains utilities that help identify the current way that the lock screen
 * will be displayed.
 */

/** @enum {string} */
var LockScreenUnlockType = {
  VALUE_PENDING: 'value_pending',
  PASSWORD: 'password',
  PIN_PASSWORD: 'pin+password'
};

/** @polymerBehavior */
var LockStateBehavior = {
  properties: {
    /**
     * The currently selected unlock type.
     * @type {!LockScreenUnlockType}
     */
    selectedUnlockType: {
      type: String,
      notify: true,
      value: LockScreenUnlockType.VALUE_PENDING
    },

    /**
     * True/false if there is a PIN set; undefined if the computation is still
     * pending. This is a separate value from selectedUnlockType because the UI
     * can change the selectedUnlockType before setting up a PIN.
     * @type {boolean|undefined}
     */
    hasPin: {
      type: Boolean,
      notify: true
    },

    /**
     * Interface for chrome.quickUnlockPrivate calls. May be overriden by tests.
     * @private
     */
    quickUnlockPrivate_: {
      type: Object,
      value: chrome.quickUnlockPrivate
    },
  },

  /** @override */
  attached: function() {
    this.boundOnActiveModesChanged_ = this.updateUnlockType_.bind(this);
    this.quickUnlockPrivate_.onActiveModesChanged.addListener(
        this.boundOnActiveModesChanged_);

    this.updateUnlockType_();
  },

  /** @override */
  detached: function() {
    this.quickUnlockPrivate_.onActiveModesChanged.removeListener(
        this.boundOnActiveModesChanged_);
  },

  /**
   * Updates the selected unlock type radio group. This function will get called
   * after preferences are initialized, after the quick unlock mode has been
   * changed, and after the lockscreen preference has changed.
   *
   * @private
   */
  updateUnlockType_: function() {
    this.quickUnlockPrivate_.getActiveModes(function(modes) {
      if (modes.includes(chrome.quickUnlockPrivate.QuickUnlockMode.PIN)) {
        this.hasPin = true;
        this.selectedUnlockType = LockScreenUnlockType.PIN_PASSWORD;
      } else {
        this.hasPin = false;
        this.selectedUnlockType = LockScreenUnlockType.PASSWORD;
      }
    }.bind(this));
  },
};
