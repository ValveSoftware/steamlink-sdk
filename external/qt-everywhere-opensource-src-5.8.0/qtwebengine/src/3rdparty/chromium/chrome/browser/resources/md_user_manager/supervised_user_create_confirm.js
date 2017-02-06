// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

/**
 * @fileoverview 'supervised-user-create-confirm' is a page that is displayed
 * upon successful creation of a supervised user. It contains information for
 * the custodian on where to configure browsing restrictions as well as how to
 * exit and childlock their profile.
 */
(function() {
/**
 * Maximum length of the supervised user profile name or custodian's username.
 * @const {number}
 */
var MAX_NAME_LENGTH = 50;

Polymer({
  is: 'supervised-user-create-confirm',

  behaviors: [
    I18nBehavior
  ],

  properties: {
    /**
     * Profile Info of the supervised user that is passed to the page.
     * @type {?ProfileInfo}
     */
    profileInfo: {
      type: Object,
      value: function() { return null; }
    },

    /** @private {!signin.ProfileBrowserProxy} */
    browserProxy_: Object
  },

  listeners: {
    'tap': 'onTap_'
  },

  /** @override */
  created: function() {
    this.browserProxy_ = signin.ProfileBrowserProxyImpl.getInstance();
  },

  /**
   * Handles tap events from dynamically created links in the
   * supervisedUserCreatedText i18n string.
   * @param {!Event} event
   * @private
   */
  onTap_: function(event) {
    var element = Polymer.dom(event).rootTarget;

    // Handle the tap event only if the target is a '<a>' element.
    if (element.nodeName == 'A') {
      this.browserProxy_.openUrlInLastActiveProfileBrowser(element.href);
      event.preventDefault();
    }
  },

  /**
   * Returns the shortened profile name or empty string if |profileInfo| is
   * null.
   * @param {?ProfileInfo} profileInfo
   * @return {string}
   * @private
   */
  elideProfileName_: function(profileInfo) {
    var name = profileInfo ? profileInfo.name : '';
    return elide(name, MAX_NAME_LENGTH);
  },

  /**
   * Returns the shortened custodian username or empty string if |profileInfo|
   * is null.
   * @param {?ProfileInfo} profileInfo
   * @return {string}
   * @private
   */
  elideCustodianUsername_: function(profileInfo) {
    var name = profileInfo ? profileInfo.custodianUsername : '';
    return elide(name, MAX_NAME_LENGTH);
  },

  /**
   * Computed binding returning the text of the title section.
   * @param {?ProfileInfo} profileInfo
   * @return {string}
   * @private
   */
  titleText_: function(profileInfo) {
    return this.i18n('supervisedUserCreatedTitle',
                     this.elideProfileName_(profileInfo));
  },

  /**
   * Computed binding returning the sanitized confirmation HTML message that is
   * safe to set as innerHTML.
   * @param {?ProfileInfo} profileInfo
   * @return {string}
   * @private
   */
  confirmationMessage_: function(profileInfo) {
    return this.i18n('supervisedUserCreatedText',
                     this.elideProfileName_(profileInfo),
                     this.elideCustodianUsername_(profileInfo));
  },

  /**
   * Computed binding returning the text of the 'Switch To User' button.
   * @param {?ProfileInfo} profileInfo
   * @return {string}
   * @private
   */
  switchUserText_: function(profileInfo) {
    return this.i18n('supervisedUserCreatedSwitch',
                     this.elideProfileName_(profileInfo));
  },

  /**
   * Handler for the 'Ok' button tap event.
   * @param {!Event} event
   * @private
   */
  onOkTap_: function(event) {
    // Event is caught by user-manager-pages.
    this.fire('change-page', {page: 'user-pods-page'});
  },

  /**
   * Handler for the 'Switch To User' button tap event.
   * @param {!Event} event
   * @private
   */
  onSwitchUserTap_: function(event) {
    this.browserProxy_.switchToProfile(this.profileInfo.filePath);
    // Event is caught by user-manager-pages.
    this.fire('change-page', {page: 'user-pods-page'});
  }
});
})();
