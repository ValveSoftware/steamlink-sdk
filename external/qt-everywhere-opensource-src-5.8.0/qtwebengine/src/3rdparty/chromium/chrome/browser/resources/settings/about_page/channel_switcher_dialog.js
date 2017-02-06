// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

/**
 * @fileoverview 'settings-channel-switcher-dialog' is a component allowing the
 * user to switch between release channels (dev, beta, stable). A
 * |target-channel-changed| event is fired if the user does select a different
 * release channel to notify parents of this dialog.
 */
Polymer({
  is: 'settings-channel-switcher-dialog',

  behaviors: [I18nBehavior],

  properties: {
    /** @private */
    browserChannelEnum_: {
      type: Object,
      value: BrowserChannel,
    },

    /** @private {!BrowserChannel} */
    currentChannel_: String,

    /**
     * Controls which of the two action buttons is visible.
     * @private {?{changeChannel: boolean, changeChannelAndPowerwash: boolean}}
     */
    shouldShowButtons_: {
      type: Object,
      value: null,
    },

    /** @private {?{title: string, description: string}} */
    warning_: {
      type: Object,
      value: null,
    },
  },

  /** @private {?settings.AboutPageBrowserProxy} */
  browserProxy_: null,

  /** @override */
  ready: function() {
    this.browserProxy_ = settings.AboutPageBrowserProxyImpl.getInstance();

    this.browserProxy_.getCurrentChannel().then(function(channel) {
      this.currentChannel_ = channel;
      // Pre-populate radio group with current channel.
      this.$$('paper-radio-group').select(channel);
    }.bind(this));
  },

  /** @override */
  attached: function() {
    this.$.dialog.open();
  },

  /** @private */
  onCancelTap_: function() {
    this.$.dialog.close();
  },

  /** @private */
  onChangeChannelTap_: function() {
    var selectedChannel = this.$$('paper-radio-group').selected;
    this.browserProxy_.setChannel(selectedChannel, false);
    this.$.dialog.close();
    this.fire('target-channel-changed', selectedChannel);
  },

  /** @private */
  onChangeChannelAndPowerwashTap_: function() {
    var selectedChannel = this.$$('paper-radio-group').selected;
    this.browserProxy_.setChannel(selectedChannel, true);
    this.$.dialog.close();
    this.fire('target-channel-changed', selectedChannel);
  },

  /**
   * @param {string} titleId Localized string ID for the title.
   * @param {string} descriptionId Localized string ID for the description.
   * @param {string=} opt_productNameId Localized string ID for the product
   *     name.
   * @private
   */
  updateWarning_: function(titleId, descriptionId, opt_productNameId) {
    this.warning_ = {
      title: this.i18n(titleId),
      description: opt_productNameId ?
          this.i18n(descriptionId, this.i18n(opt_productNameId)) :
          this.i18n(descriptionId),
    };
  },

  /**
   * @param {boolean} changeChannel Whether the changeChannel button sholud be
   *     visible.
   * @param {boolean} changeChannelAndPowerwash Whether the
   *     changeChannelAndPowerwash button should be visible.
   * @private
   */
  updateButtons_: function(changeChannel, changeChannelAndPowerwash) {
    if (changeChannel || changeChannelAndPowerwash) {
      // Ensure that at most one button is visible at any given time.
      assert(changeChannel != changeChannelAndPowerwash);
    }

    this.shouldShowButtons_ = {
      changeChannel: changeChannel,
      changeChannelAndPowerwash: changeChannelAndPowerwash,
    };
  },

  /** @private */
  onChannelSelectionChanged_: function() {
    var selectedChannel = this.$$('paper-radio-group').selected;

    if (selectedChannel == this.currentChannel_) {
      this.shouldShowButtons_ = null;
      this.warning_ = null;
      this.$.dialog.notifyResize();
      return;
    }

    if (settings.isTargetChannelMoreStable(
        this.currentChannel_, selectedChannel)) {
      if (loadTimeData.getBoolean('aboutEnterpriseManaged')) {
        this.updateWarning_(
            'aboutDelayedWarningTitle',
            'aboutDelayedWarningMessage',
            'aboutProductTitle');
        this.updateButtons_(true, false);
      } else {
        this.updateWarning_(
          'aboutPowerwashWarningTitle', 'aboutPowerwashWarningMessage');
        this.updateButtons_(false, true);
      }
    } else {
      this.updateWarning_(
        'aboutUnstableWarningTitle',
        'aboutUnstableWarningMessage',
        'aboutProductTitle');
      this.updateButtons_(true, false);
    }
    this.$.dialog.notifyResize();
  },

  /**
   * @return {boolean}
   * @private
   */
  shouldShowWarning_: function() {
    return this.warning_ !== null;
  },
});
