// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

/**
 * @fileoverview
 * 'settings-lock-screen' allows the user to change how they unlock their
 * device.
 *
 * Example:
 *
 * <settings-lock-screen
 *   prefs="{{prefs}}">
 * </settings-lock-screen>
 */

Polymer({
  is: 'settings-lock-screen',

  behaviors: [I18nBehavior, LockStateBehavior, settings.RouteObserverBehavior],

  properties: {
    /** Preferences state. */
    prefs: {
      type: Object
    },

    /**
     * setModes_ is a partially applied function that stores the previously
     * entered password. It's defined only when the user has already entered a
     * valid password.
     *
     * @type {Object|undefined}
     * @private
     */
    setModes_: {
      type: Object,
      observer: 'onSetModesChanged_'
    },
  },

  /** selectedUnlockType is defined in LockStateBehavior. */
  observers: ['selectedUnlockTypeChanged_(selectedUnlockType)'],

  /** @override */
  attached: function() {
    // currentRouteChanged is not called during the initial navigation. If the
    // user navigates directly to the lockScreen page, we still want to show the
    // password prompt page.
    this.currentRouteChanged();
  },

  /**
   * Overridden from settings.RouteObserverBehavior.
   * @protected
   */
  currentRouteChanged: function() {
    if (settings.getCurrentRoute() == settings.Route.LOCK_SCREEN &&
        !this.setModes_) {
      this.$.passwordPrompt.open();
    } else {
      // If the user navigated away from the lock screen settings page they will
      // have to re-enter their password.
      this.setModes_ = undefined;
    }
  },

  /**
   * Called when the unlock type has changed.
   * @param {!string} selected The current unlock type.
   * @private
   */
  selectedUnlockTypeChanged_: function(selected) {
    if (selected == LockScreenUnlockType.VALUE_PENDING)
      return;

    if (selected != LockScreenUnlockType.PIN_PASSWORD && this.setModes_) {
      this.setModes_.call(null, [], [], function(didSet) {
        assert(didSet, 'Failed to clear quick unlock modes');
      });
    }
  },

  /** @private */
  onSetModesChanged_: function() {
    if (settings.getCurrentRoute() == settings.Route.LOCK_SCREEN &&
        !this.setModes_) {
      this.$.setupPin.close();
      this.$.passwordPrompt.open();
    }
  },

  /** @private */
  onPasswordClosed_: function() {
    if (!this.setModes_)
      settings.navigateTo(settings.Route.PEOPLE);
  },

  /** @private */
  onPinSetupDone_: function() {
    this.$.setupPin.close();
  },

  /**
   * Returns true if the setup pin section should be shown.
   * @param {!string} selectedUnlockType The current unlock type. Used to let
   *     Polymer know about the dependency.
   * @private
   */
  showConfigurePinButton_: function(selectedUnlockType) {
    return selectedUnlockType === LockScreenUnlockType.PIN_PASSWORD;
  },

  /** @private */
  getSetupPinText_: function() {
    if (this.hasPin)
      return this.i18n('lockScreenChangePinButton');
    return this.i18n('lockScreenSetupPinButton');
  },

  /** @private */
  onConfigurePin_: function() {
    this.$.setupPin.open();
  },
});
