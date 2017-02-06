// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

/**
 * @fileoverview
 * 'settings-keyboard' is the settings subpage with keyboard settings.
 */

// TODO(michaelpg): The docs below are duplicates of settings_dropdown_menu,
// because we can't depend on settings_dropdown_menu in compiled_resources2.gyp
// withhout first converting settings_dropdown_menu to compiled_resources2.gyp.
// After the conversion, we should remove these.
/** @typedef {{name: string, value: (number|string)}} */
var DropdownMenuOption;
/** @typedef {!Array<!DropdownMenuOption>} */
var DropdownMenuOptionList;

Polymer({
  is: 'settings-keyboard',

  properties: {
    /** Preferences state. */
    prefs: {
      type: Object,
      notify: true,
    },

    /** The current active route. */
    currentRoute: {
      type: Object,
      notify: true,
    },

    /** @private Whether to show Caps Lock options. */
    showCapsLock_: Boolean,

    /** @private Whether to show diamond key options. */
    showDiamondKey_: Boolean,

    /** @private {!DropdownMenuOptionList} Menu items for key mapping. */
    keyMapTargets_: Object,

    /**
     * @private {!DropdownMenuOptionList} Menu items for key mapping, including
     * Caps Lock.
     */
    keyMapTargetsWithCapsLock_: Object,

    /**
     * Auto-repeat delays (in ms) for the corresponding slider values, from
     * long to short. The values were chosen to provide a large range while
     * giving several options near the defaults.
     * @private {!Array<number>}
     */
    autoRepeatDelays_: {
      type: Array,
      value: [2000, 1500, 1000, 500, 300, 200, 150],
      readOnly: true,
    },

    /**
     * Auto-repeat intervals (in ms) for the corresponding slider values, from
     * long to short. The slider itself is labeled "rate", the inverse of
     * interval, and goes from slow (long interval) to fast (short interval).
     * @private {!Array<number>}
     */
    autoRepeatIntervals_: {
      type: Array,
      value: [2000, 1000, 500, 300, 200, 100, 50, 30, 20],
      readOnly: true,
    },
  },

  /** @override */
  ready: function() {
    cr.addWebUIListener('show-keys-changed', this.onShowKeysChange_.bind(this));
    settings.DevicePageBrowserProxyImpl.getInstance().initializeKeyboard();
    this.setUpKeyMapTargets_();
  },

  /**
   * Initializes the dropdown menu options for remapping keys.
   * @private
   */
  setUpKeyMapTargets_: function() {
    this.keyMapTargets_ = [
      {value: 0, name: loadTimeData.getString('keyboardKeySearch')},
      {value: 1, name: loadTimeData.getString('keyboardKeyCtrl')},
      {value: 2, name: loadTimeData.getString('keyboardKeyAlt')},
      {value: 3, name: loadTimeData.getString('keyboardKeyDisabled')},
      {value: 5, name: loadTimeData.getString('keyboardKeyEscape')},
    ];

    var keyMapTargetsWithCapsLock = this.keyMapTargets_.slice();
    // Add Caps Lock, for keys allowed to be mapped to Caps Lock.
    keyMapTargetsWithCapsLock.splice(4, 0, {
      value: 4, name: loadTimeData.getString('keyboardKeyCapsLock'),
    });
    this.keyMapTargetsWithCapsLock_ = keyMapTargetsWithCapsLock;
  },

  /**
   * Handler for updating which keys to show.
   * @param {boolean} showCapsLock
   * @param {boolean} showDiamondKey
   * @private
   */
  onShowKeysChange_: function(showCapsLock, showDiamondKey) {
    this.showCapsLock_ = showCapsLock;
    this.showDiamondKey_ = showDiamondKey;
  },

  onShowKeyboardShortcutsOverlayTap_: function() {
    settings.DevicePageBrowserProxyImpl.getInstance()
        .showKeyboardShortcutsOverlay();
  },

  onShowLanguageInputTap_: function() {
    this.currentRoute = {
      page: 'advanced',
      section: 'languages',
      subpage: [],
    };
  },
});
