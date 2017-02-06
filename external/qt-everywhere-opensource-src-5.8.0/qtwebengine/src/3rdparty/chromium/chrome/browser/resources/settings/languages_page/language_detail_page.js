// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

/**
 * @fileoverview 'settings-language-detail-page' is a sub-page for editing
 * an individual language's settings.
 */
Polymer({
  is: 'settings-language-detail-page',

  properties: {
    /**
     * Preferences state.
     */
    prefs: {
      type: Object,
      notify: true,
    },

    /**
     * Read-only reference to the languages model provided by the
     * 'settings-languages' instance.
     * @type {!LanguagesModel|undefined}
     */
    languages: Object,

    /**
     * The language to display the details for.
     * @type {!LanguageState|undefined}
     */
    detail: Object,

    /** @private {!LanguageHelper} */
    languageHelper_: Object,
  },

  /** @override */
  created: function() {
     this.languageHelper_ = LanguageHelperImpl.getInstance();
  },

  /** @override */
  ready: function() {
    // In a CrOS multi-user session, the primary user controls the UI language.
    if (this.isSecondaryUser_()) {
      var indicator = this.$.policyIndicator;
      indicator.indicatorType = CrPolicyIndicatorType.PRIMARY_USER;
      indicator.controllingUser = loadTimeData.getString('primaryUserEmail');
    }

    // The UI language choice doesn't persist for guests.
    if (cr.isChromeOS &&
        (uiAccountTweaks.UIAccountTweaks.loggedInAsGuest() ||
         uiAccountTweaks.UIAccountTweaks.loggedInAsPublicAccount())) {
      this.$.languageSettings.hidden = true;
    }
  },

  /**
   * Checks whether the prospective UI language (the pref that indicates what
   * language to use in Chrome) matches the current language. This pref is only
   * on Chrome OS and Windows; we don't control the UI language elsewhere.
   * @param {string} languageCode The language code identifying a language.
   * @param {string} prospectiveUILanguage The prospective UI language.
   * @return {boolean} True if the given language matches the prospective UI
   *     pref (which may be different from the actual UI language).
   * @private
   */
  isProspectiveUILanguage_: function(languageCode, prospectiveUILanguage) {
    assert(cr.isChromeOS || cr.isWindows);
    return languageCode == this.languageHelper_.getProspectiveUILanguage();
  },

  /**
   * @param {string} languageCode The language code identifying a language.
   * @param {string} prospectiveUILanguage The prospective UI language.
   * @return {boolean} True if the the given language, the prospective UI
   *     language and the actual language all the same.
   * @private
   */
  isCurrentUILanguage_: function(languageCode, prospectiveUILanguage) {
    assert(cr.isChromeOS || cr.isWindows);
    return languageCode == prospectiveUILanguage &&
           languageCode == navigator.language;
  },

   /**
   * @param {string} languageCode The language code identifying a language.
   * @param {string} targetLanguageCode The default translate target language.
   * @return {boolean} True if the language code matches the target language.
   * @private
   */
  isTranslateDisabled_: function(languageCode, targetLanguageCode) {
    return this.languageHelper_.convertLanguageCodeForTranslate(languageCode) ==
        targetLanguageCode;
  },

   /**
   * @param {string} languageCode The language code identifying a language.
   * @param {string} prospectiveUILanguage The prospective UI language.
   * @return {boolean} True if the prospective UI language is set to
   *     |languageCode| but requires a restart to take effect.
   * @private
   */
  isRestartRequired_: function(languageCode, prospectiveUILanguage) {
    return prospectiveUILanguage == languageCode &&
           navigator.language != languageCode;
  },

  /**
   * @param {string} languageCode The language code identifying a language.
   * @param {string} prospectiveUILanguage The chosen UI language.
   * @return {boolean} True if the chosen language cannot currently be changed.
   * @private
   */
  isUILanguageChangeDisabled_: function(languageCode, prospectiveUILanguage) {
    // UI language setting belongs to the primary user.
    if (this.isSecondaryUser_())
      return true;
    // If this is both the actual and the prospective language, flipping the
    // toggle button to "off" makes no sense.
    return this.isCurrentUILanguage_(languageCode, prospectiveUILanguage);
  },

  /**
   * @return {boolean} True for a secondary user in a multi-profile session.
   * @private
   */
  isSecondaryUser_: function() {
    return cr.isChromeOS && loadTimeData.getBoolean('isSecondaryUser');
  },

  /**
   * @param {string} languageCode The language code identifying a language.
   * @param {boolean} translateEnabled Whether translate is enabled.
   * @return {boolean} True if the translate section should be hidden.
   * @private
   */
  shouldHideTranslate_: function(languageCode, translateEnabled) {
    // Translate server supports Chinese (Traditional) and Chinese (Simplified)
    // but not 'general' Chinese. To avoid ambiguity, hide the translate
    // checkbox when general Chinese is selected.
    return !translateEnabled || languageCode == 'zh';
  },

  /**
   * Handler for changes to the translate checkbox.
   * @param {!{target: !{checked: boolean}}} e
   * @private
   */
  onTranslateEnabledChange_: function(e) {
    if (e.target.checked)
      this.languageHelper_.enableTranslateLanguage(this.detail.language.code);
    else
      this.languageHelper_.disableTranslateLanguage(this.detail.language.code);
  },

  /**
   * Handler for changes to the UI language toggle button.
   * @param {!{target: !{checked: boolean}}} e
   * @private
   */
  onUILanguageChange_: function(e) {
    if (e.target.checked) {
      this.languageHelper_.setUILanguage(this.detail.language.code);
    } else {
      // Reset the chosen UI language to the actual UI language.
      this.languageHelper_.resetUILanguage();
    }
  },

  /**
   * Handler for the restart button.
   * @private
   */
  onRestartTap_: function() {
<if expr="chromeos">
    settings.LifetimeBrowserProxyImpl.getInstance().logOutAndRestart();
</if>
<if expr="not chromeos">
    settings.LifetimeBrowserProxyImpl.getInstance().restart();
</if>
  },
});
