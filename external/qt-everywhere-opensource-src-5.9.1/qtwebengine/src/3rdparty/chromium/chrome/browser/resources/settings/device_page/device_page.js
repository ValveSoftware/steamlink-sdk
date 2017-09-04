// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

/**
 * @fileoverview 'settings-device-page' is the settings page for device and
 * peripheral settings.
 */
Polymer({
  is: 'settings-device-page',

  behaviors: [
    I18nBehavior,
    WebUIListenerBehavior,
    settings.RouteObserverBehavior,
  ],

  properties: {
    /** Preferences state. */
    prefs: {
      type: Object,
      notify: true,
    },

    /** @private */
    hasMouse_: {
      type: Boolean,
      value: false,
    },

    /** @private */
    hasTouchpad_: {
      type: Boolean,
      value: false,
    },

    /** @private */
    stylusAllowed_: {
      type: Boolean,
      value: function() {
        return loadTimeData.getBoolean('stylusAllowed');
      },
      readOnly: true,
    },

    /** @private */
    showStorageManager_: {
      type: Boolean,
      value: function() {
        return loadTimeData.getBoolean('showStorageManager');
      },
      readOnly: true,
    },
  },

  observers: [
    'pointersChanged_(hasMouse_, hasTouchpad_)',
  ],

  ready: function() {
    cr.addWebUIListener('has-mouse-changed', this.set.bind(this, 'hasMouse_'));
    cr.addWebUIListener(
        'has-touchpad-changed', this.set.bind(this, 'hasTouchpad_'));
    settings.DevicePageBrowserProxyImpl.getInstance().initializePointers();
  },

  /**
   * @return {string}
   * @private
   */
  getPointersTitle_: function() {
    if (this.hasMouse_ && this.hasTouchpad_)
      return this.i18n('mouseAndTouchpadTitle');
    if (this.hasMouse_)
      return this.i18n('mouseTitle');
    if (this.hasTouchpad_)
      return this.i18n('touchpadTitle');
    return '';
  },

  /**
   * @return {string}
   * @private
   */
  getPointersIcon_: function() {
    if (this.hasMouse_)
      return 'settings:mouse';
    if (this.hasTouchpad_)
      return 'settings:touch-app';
    return '';
  },

  /**
   * Handler for tapping the mouse and touchpad settings menu item.
   * @private
   */
  onPointersTap_: function() {
    settings.navigateTo(settings.Route.POINTERS);
  },

  /**
   * Handler for tapping the Keyboard settings menu item.
   * @private
   */
  onKeyboardTap_: function() {
    settings.navigateTo(settings.Route.KEYBOARD);
  },

  /**
   * Handler for tapping the Keyboard settings menu item.
   * @private
   */
  onStylusTap_: function() {
    settings.navigateTo(settings.Route.STYLUS);
  },

  /**
   * Handler for tapping the Display settings menu item.
   * @private
   */
  onDisplayTap_: function() {
    settings.navigateTo(settings.Route.DISPLAY);
  },

  /**
   * Handler for tapping the Storage settings menu item.
   * @private
   */
  onStorageTap_: function() {
    settings.navigateTo(settings.Route.STORAGE);
  },

  /** @protected */
  currentRouteChanged: function() {
    this.checkPointerSubpage_();
  },

  /**
   * @param {boolean} hasMouse
   * @param {boolean} hasTouchpad
   * @private
   */
  pointersChanged_: function(hasMouse, hasTouchpad) {
    this.$.pointersRow.hidden = !hasMouse && !hasTouchpad;
    this.checkPointerSubpage_();
  },

  /**
   * Leaves the pointer subpage if all pointing devices are detached.
   * @private
   */
  checkPointerSubpage_: function() {
    if (!this.hasMouse_ && !this.hasTouchpad_ &&
        settings.getCurrentRoute() == settings.Route.POINTERS) {
      settings.navigateTo(settings.Route.DEVICE);
    }
  },
});
