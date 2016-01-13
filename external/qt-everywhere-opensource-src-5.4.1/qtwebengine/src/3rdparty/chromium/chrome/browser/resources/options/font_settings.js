// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

cr.define('options', function() {

  var OptionsPage = options.OptionsPage;

  /**
   * FontSettings class
   * Encapsulated handling of the 'Fonts and Encoding' page.
   * @class
   */
  function FontSettings() {
    OptionsPage.call(this,
                     'fonts',
                     loadTimeData.getString('fontSettingsPageTabTitle'),
                     'font-settings');
  }

  cr.addSingletonGetter(FontSettings);

  FontSettings.prototype = {
    __proto__: OptionsPage.prototype,

    /**
     * Initialize the page.
     */
    initializePage: function() {
      OptionsPage.prototype.initializePage.call(this);

      var standardFontRange = $('standard-font-size');
      standardFontRange.valueMap = [9, 10, 11, 12, 13, 14, 15, 16, 17, 18, 20,
          22, 24, 26, 28, 30, 32, 34, 36, 40, 44, 48, 56, 64, 72];
      standardFontRange.addEventListener(
          'change', this.standardRangeChanged_.bind(this, standardFontRange));
      standardFontRange.addEventListener(
          'input', this.standardRangeChanged_.bind(this, standardFontRange));
      standardFontRange.customChangeHandler =
          this.standardFontSizeChanged_.bind(standardFontRange);

      var minimumFontRange = $('minimum-font-size');
      minimumFontRange.valueMap = [6, 7, 8, 9, 10, 11, 12, 13, 14, 15, 16, 17,
          18, 20, 22, 24];
      minimumFontRange.addEventListener(
          'change', this.minimumRangeChanged_.bind(this, minimumFontRange));
      minimumFontRange.addEventListener(
          'input', this.minimumRangeChanged_.bind(this, minimumFontRange));
      minimumFontRange.customChangeHandler =
          this.minimumFontSizeChanged_.bind(minimumFontRange);

      var placeholder = loadTimeData.getString('fontSettingsPlaceholder');
      var elements = [$('standard-font-family'), $('serif-font-family'),
                      $('sans-serif-font-family'), $('fixed-font-family'),
                      $('font-encoding')];
      elements.forEach(function(el) {
        el.appendChild(new Option(placeholder));
        el.setDisabled('noFontsAvailable', true);
      });

      $('font-settings-confirm').onclick = function() {
        OptionsPage.closeOverlay();
      };

      $('advanced-font-settings-options').onclick = function() {
        chrome.send('openAdvancedFontSettingsOptions');
      };
    },

    /**
     * Called by the options page when this page has been shown.
     */
    didShowPage: function() {
      // The fonts list may be large so we only load it when this page is
      // loaded for the first time.  This makes opening the options window
      // faster and consume less memory if the user never opens the fonts
      // dialog.
      if (!this.hasShown) {
        chrome.send('fetchFontsData');
        this.hasShown = true;
      }
    },

    /**
     * Handler that is called when the user changes the position of the standard
     * font size slider. This allows the UI to show a preview of the change
     * before the slider has been released and the associated prefs updated.
     * @param {Element} el The slider input element.
     * @param {Event} event Change event.
     * @private
     */
    standardRangeChanged_: function(el, event) {
      var size = el.mapPositionToPref(el.value);
      var fontSampleEl = $('standard-font-sample');
      this.setUpFontSample_(fontSampleEl, size, fontSampleEl.style.fontFamily,
                            true);

      fontSampleEl = $('serif-font-sample');
      this.setUpFontSample_(fontSampleEl, size, fontSampleEl.style.fontFamily,
                            true);

      fontSampleEl = $('sans-serif-font-sample');
      this.setUpFontSample_(fontSampleEl, size, fontSampleEl.style.fontFamily,
                            true);

      fontSampleEl = $('fixed-font-sample');
      this.setUpFontSample_(fontSampleEl,
                            size - OptionsPage.SIZE_DIFFERENCE_FIXED_STANDARD,
                            fontSampleEl.style.fontFamily, false);
    },

    /**
     * Sets the 'default_fixed_font_size' preference when the user changes the
     * standard font size.
     * @param {Event} event Change event.
     * @private
     */
    standardFontSizeChanged_: function(event) {
      var size = this.mapPositionToPref(this.value);
      Preferences.setIntegerPref(
        'webkit.webprefs.default_fixed_font_size',
        size - OptionsPage.SIZE_DIFFERENCE_FIXED_STANDARD, true);
      return false;
    },

    /**
     * Handler that is called when the user changes the position of the minimum
     * font size slider. This allows the UI to show a preview of the change
     * before the slider has been released and the associated prefs updated.
     * @param {Element} el The slider input element.
     * @param {Event} event Change event.
     * @private
     */
    minimumRangeChanged_: function(el, event) {
      var size = el.mapPositionToPref(el.value);
      var fontSampleEl = $('minimum-font-sample');
      this.setUpFontSample_(fontSampleEl, size, fontSampleEl.style.fontFamily,
                            true);
    },

    /**
     * Sets the 'minimum_logical_font_size' preference when the user changes the
     * minimum font size.
     * @param {Event} event Change event.
     * @private
     */
    minimumFontSizeChanged_: function(event) {
      var size = this.mapPositionToPref(this.value);
      Preferences.setIntegerPref(
        'webkit.webprefs.minimum_logical_font_size', size, true);
      return false;
    },

    /**
     * Sets the text, font size and font family of the sample text.
     * @param {Element} el The div containing the sample text.
     * @param {number} size The font size of the sample text.
     * @param {string} font The font family of the sample text.
     * @param {bool} showSize True if the font size should appear in the sample.
     * @private
     */
    setUpFontSample_: function(el, size, font, showSize) {
      var prefix = showSize ? (size + ': ') : '';
      el.textContent = prefix +
          loadTimeData.getString('fontSettingsLoremIpsum');
      el.style.fontSize = size + 'px';
      if (font)
        el.style.fontFamily = font;
    },

    /**
     * Populates a select list and selects the specified item.
     * @param {Element} element The select element to populate.
     * @param {Array} items The array of items from which to populate.
     * @param {string} selectedValue The selected item.
     * @private
     */
    populateSelect_: function(element, items, selectedValue) {
      // Remove any existing content.
      element.textContent = '';

      // Insert new child nodes into select element.
      var value, text, selected, option;
      for (var i = 0; i < items.length; i++) {
        value = items[i][0];
        text = items[i][1];
        dir = items[i][2];
        if (text) {
          selected = value == selectedValue;
          var option = new Option(text, value, false, selected);
          option.dir = dir;
          element.appendChild(option);
        } else {
          element.appendChild(document.createElement('hr'));
        }
      }

      element.setDisabled('noFontsAvailable', false);
    }
  };

  // Chrome callbacks
  FontSettings.setFontsData = function(fonts, encodings, selectedValues) {
    FontSettings.getInstance().populateSelect_($('standard-font-family'), fonts,
                                               selectedValues[0]);
    FontSettings.getInstance().populateSelect_($('serif-font-family'), fonts,
                                               selectedValues[1]);
    FontSettings.getInstance().populateSelect_($('sans-serif-font-family'),
                                               fonts, selectedValues[2]);
    FontSettings.getInstance().populateSelect_($('fixed-font-family'), fonts,
                                               selectedValues[3]);
    FontSettings.getInstance().populateSelect_($('font-encoding'), encodings,
                                               selectedValues[4]);
  };

  FontSettings.setUpStandardFontSample = function(font, size) {
    FontSettings.getInstance().setUpFontSample_($('standard-font-sample'), size,
                                                font, true);
  };

  FontSettings.setUpSerifFontSample = function(font, size) {
    FontSettings.getInstance().setUpFontSample_($('serif-font-sample'), size,
                                                font, true);
  };

  FontSettings.setUpSansSerifFontSample = function(font, size) {
    FontSettings.getInstance().setUpFontSample_($('sans-serif-font-sample'),
                                                size, font, true);
  };

  FontSettings.setUpFixedFontSample = function(font, size) {
    FontSettings.getInstance().setUpFontSample_($('fixed-font-sample'),
                                                size, font, false);
  };

  FontSettings.setUpMinimumFontSample = function(size) {
    // If size is less than 6, represent it as six in the sample to account
    // for the minimum logical font size.
    if (size < 6)
      size = 6;
    FontSettings.getInstance().setUpFontSample_($('minimum-font-sample'), size,
                                                null, true);
  };

  /**
   * @param {boolean} available Whether the Advanced Font Settings Extension
   *     is installed and enabled.
   */
  FontSettings.notifyAdvancedFontSettingsAvailability = function(available) {
    $('advanced-font-settings-install').hidden = available;
    $('advanced-font-settings-options').hidden = !available;
  };

  // Export
  return {
    FontSettings: FontSettings
  };
});

