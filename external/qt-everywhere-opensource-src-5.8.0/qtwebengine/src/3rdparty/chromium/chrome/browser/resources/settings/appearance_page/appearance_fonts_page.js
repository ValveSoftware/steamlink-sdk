// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

(function() {
  'use strict';

  /**
   * This is the absolute difference maintained between standard and
   * fixed-width font sizes. http://crbug.com/91922.
   * @const @private {number}
   */
  var SIZE_DIFFERENCE_FIXED_STANDARD_ = 3;

  /** @const @private {!Array<number>} */
  var FONT_SIZE_RANGE_ = [
    9, 10, 11, 12, 13, 14, 15, 16, 17, 18, 20, 22, 24, 26, 28, 30, 32, 34, 36,
    40, 44, 48, 56, 64, 72,
  ];

  /** @const @private {!Array<number>} */
  var MINIMUM_FONT_SIZE_RANGE_ = [
    6, 7, 8, 9, 10, 11, 12, 13, 14, 15, 16, 17, 18, 20, 22, 24
  ];

  /**
   * 'settings-appearance-fonts-page' is the settings page containing appearance
   * settings.
   *
   * Example:
   *
   *   <settings-appearance-fonts-page prefs="{{prefs}}">
   *   </settings-appearance-fonts-page>
   */
  Polymer({
    is: 'settings-appearance-fonts-page',

    behaviors: [I18nBehavior, WebUIListenerBehavior],

    properties: {
      /** @private */
      advancedExtensionInstalled_: Boolean,

      /** @private */
      advancedExtensionSublabel_: String,

      /** @private */
      advancedExtensionUrl_: String,

      /** @private {!settings.FontsBrowserProxy} */
      browserProxy_: Object,

      /**
       * Common font sizes.
       * @private {!Array<number>}
       */
      fontSizeRange_: {
        readOnly: true,
        type: Array,
        value: FONT_SIZE_RANGE_,
      },

      /**
       * Reasonable, minimum font sizes.
       * @private {!Array<number>}
       */
      minimumFontSizeRange_: {
        readOnly: true,
        type: Array,
        value: MINIMUM_FONT_SIZE_RANGE_,
      },

      /**
       * Preferences state.
       */
      prefs: {
        type: Object,
        notify: true,
      },
    },

    observers: [
      'fontSizeChanged_(prefs.webkit.webprefs.default_font_size.value)',
    ],

    /** @override */
    created: function() {
      this.browserProxy_ = settings.FontsBrowserProxyImpl.getInstance();
    },

    /** @override */
    ready: function() {
      this.addWebUIListener('advanced-font-settings-installed',
          this.setAdvancedExtensionInstalled_.bind(this));
      this.browserProxy_.observeAdvancedFontExtensionAvailable();

      this.browserProxy_.fetchFontsData().then(
          this.setFontsData_.bind(this));
    },

    /** @private */
    openAdvancedExtension_: function() {
      if (this.advancedExtensionInstalled_)
        this.browserProxy_.openAdvancedFontSettings();
      else
        window.open(this.advancedExtensionUrl_);
    },

    /**
     * @param {boolean} isInstalled Whether the advanced font settings
     *     extension is installed.
     * @private
     */
    setAdvancedExtensionInstalled_: function(isInstalled) {
      this.advancedExtensionInstalled_ = isInstalled;
      this.advancedExtensionSublabel_ = this.i18n(isInstalled ?
          'openAdvancedFontSettings' : 'requiresWebStoreExtension');
    },

    /**
     * @param {!FontsData} response A list of fonts, encodings and the advanced
     *     font settings extension URL.
     * @private
     */
    setFontsData_: function(response) {
      var fontMenuOptions = [];
      for (var i = 0; i < response.fontList.length; ++i) {
        fontMenuOptions.push({
          value: response.fontList[i][0],
          name: response.fontList[i][1]
        });
      }
      this.$.standardFont.menuOptions = fontMenuOptions;
      this.$.serifFont.menuOptions = fontMenuOptions;
      this.$.sansSerifFont.menuOptions = fontMenuOptions;
      this.$.fixedFont.menuOptions = fontMenuOptions;

      var encodingMenuOptions = [];
      for (i = 0; i < response.encodingList.length; ++i) {
        encodingMenuOptions.push({
          value: response.encodingList[i][0],
          name: response.encodingList[i][1]
        });
      }
      this.$.encoding.menuOptions = encodingMenuOptions;
      this.advancedExtensionUrl_ = response.extensionUrl;
    },

    /**
     * @param {number} value The changed font size slider value.
     * @private
     */
    fontSizeChanged_: function(value) {
      // TODO(michaelpg): Whitelist this pref in prefs_utils.cc so it is
      // included in the <settings-prefs> getAllPrefs call, otherwise this path
      // is invalid and nothing happens. See crbug.com/612535.
      this.set('prefs.webkit.webprefs.default_fixed_font_size.value',
          value - SIZE_DIFFERENCE_FIXED_STANDARD_);
    },

    /**
     * Creates an html style value.
     * @param {number} fontSize The font size to use.
     * @param {string} fontFamily The name of the font family use.
     * @return {string}
     * @private
     */
    computeStyle_: function(fontSize, fontFamily) {
      return 'font-size: ' + fontSize + "px; font-family: '" + fontFamily +
          "';";
    },
  });
})();
