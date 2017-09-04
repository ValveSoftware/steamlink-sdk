// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

/**
 * @fileoverview
 * 'settings-manage-a11y-page' is the subpage with the accessibility
 * settings.
 */
Polymer({
  is: 'settings-manage-a11y-page',

  properties: {
    /**
     * Preferences state.
     */
    prefs: {
      type: Object,
      notify: true,
    },

    autoClickDelayOptions_: {
      readOnly: true,
      type: Array,
      value: function() {
        // These values correspond to the i18n values in settings_strings.grdp.
        // If these values get changed then those strings need to be changed as
        // well.
        return [
          {value: 600,
           name: loadTimeData.getString('delayBeforeClickExtremelyShort')},
          {value: 800,
           name: loadTimeData.getString('delayBeforeClickVeryShort')},
          {value: 1000,
           name: loadTimeData.getString('delayBeforeClickShort')},
          {value: 2000,
           name: loadTimeData.getString('delayBeforeClickLong')},
          {value: 4000,
           name: loadTimeData.getString('delayBeforeClickVeryLong')},
        ];
      },
    },

    /**
     * Whether to show experimental accessibility features.
     * @private {boolean}
     */
    showExperimentalFeatures_: {
      type: Boolean,
      value: function() {
        return loadTimeData.getBoolean('showExperimentalA11yFeatures');
      },
    },
  },

  /** @private */
  onChromeVoxSettingsTap_: function() {
    chrome.send('showChromeVoxSettings');
  },

  /** @private */
  onDisplayTap_: function() {
    settings.navigateTo(settings.Route.DISPLAY);
  },

  /** @private */
  onAppearanceTap_: function() {
    settings.navigateTo(settings.Route.APPEARANCE);
  },

  /** @private */
  onKeyboardTap_: function() {
    settings.navigateTo(settings.Route.KEYBOARD);
  },

  /** @private */
  onMouseTap_: function() {
    settings.navigateTo(settings.Route.POINTERS);
  },

  /** @private */
  onMoreFeaturesTap_: function() {
    window.open(
        'https://chrome.google.com/webstore/category/collection/accessibility');
  },
});
