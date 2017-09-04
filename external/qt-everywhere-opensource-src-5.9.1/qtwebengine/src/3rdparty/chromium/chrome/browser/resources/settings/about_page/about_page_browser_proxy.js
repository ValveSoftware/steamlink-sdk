// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

/**
 * @fileoverview A helper object used from the "About" section to interact with
 * the browser.
 */

<if expr="chromeos">
/**
 * @typedef {{
 *   text: string,
 *   url: string,
 * }}
 */
var RegulatoryInfo;

/**
 * @typedef {{
 *   arcVersion: string,
 *   osFirmware: string,
 *   osVersion: string,
 * }}
 */
var VersionInfo;

/**
 * Enumeration of all possible browser channels.
 * @enum {string}
 */
var BrowserChannel = {
  BETA: 'beta-channel',
  DEV: 'dev-channel',
  STABLE: 'stable-channel',
};
</if>

/**
 * Enumeration of all possible update statuses. The string literals must match
 * the ones defined at |AboutHandler::UpdateStatusToString|.
 * @enum {string}
 */
var UpdateStatus = {
  CHECKING: 'checking',
  UPDATING: 'updating',
  NEARLY_UPDATED: 'nearly_updated',
  UPDATED: 'updated',
  FAILED: 'failed',
  DISABLED: 'disabled',
  DISABLED_BY_ADMIN: 'disabled_by_admin',
};

/**
 * @typedef {{
 *   status: !UpdateStatus,
 *   progress: (number|undefined),
 *   message: (string|undefined),
 * }}
 */
var UpdateStatusChangedEvent;

cr.define('settings', function() {
  /**
   * @param {!BrowserChannel} channel
   * @return {string}
   */
  function browserChannelToI18nId(channel) {
    switch (channel) {
      case BrowserChannel.BETA: return 'aboutChannelBeta';
      case BrowserChannel.DEV: return 'aboutChannelDev';
      case BrowserChannel.STABLE: return 'aboutChannelStable';
    }

    assertNotReached();
  }

  /**
   * @param {!BrowserChannel} currentChannel
   * @param {!BrowserChannel} targetChannel
   * @return {boolean} Whether the target channel is more stable than the
   *     current channel.
   */
  function isTargetChannelMoreStable(currentChannel, targetChannel) {
    // List of channels in increasing stability order.
    var channelList = [
      BrowserChannel.DEV,
      BrowserChannel.BETA,
      BrowserChannel.STABLE,
    ];
    var currentIndex = channelList.indexOf(currentChannel);
    var targetIndex = channelList.indexOf(targetChannel);
    return currentIndex < targetIndex;
  }

  /** @interface */
  function AboutPageBrowserProxy() {}

  AboutPageBrowserProxy.prototype = {
    /**
     * Indicates to the browser that the page is ready.
     */
    pageReady: function() {},

    /**
     * Request update status from the browser. It results in one or more
     * 'update-status-changed' WebUI events.
     */
    refreshUpdateStatus: function() {},

    /** Opens the help page. */
    openHelpPage: function() {},

<if expr="_google_chrome">
    /**
     * Opens the feedback dialog.
     */
    openFeedbackDialog: function() {},
</if>

<if expr="chromeos">
    /**
     * Checks for available update and applies if it exists.
     */
    requestUpdate: function() {},

    /**
     * @param {!BrowserChannel} channel
     * @param {boolean} isPowerwashAllowed
     */
    setChannel: function(channel, isPowerwashAllowed) {},

    /** @return {!Promise<!BrowserChannel>} */
    getCurrentChannel: function() {},

    /** @return {!Promise<!BrowserChannel>} */
    getTargetChannel: function() {},

    /** @return {!Promise<!VersionInfo>} */
    getVersionInfo: function() {},

    /** @return {!Promise<?RegulatoryInfo>} */
    getRegulatoryInfo: function() {},
</if>
  };

  /**
   * @implements {settings.AboutPageBrowserProxy}
   * @constructor
   */
  function AboutPageBrowserProxyImpl() {}
  cr.addSingletonGetter(AboutPageBrowserProxyImpl);

  AboutPageBrowserProxyImpl.prototype = {
    /** @override */
    pageReady: function() {
      chrome.send('aboutPageReady');
    },

    /** @override */
    refreshUpdateStatus: function() {
      chrome.send('refreshUpdateStatus');
    },

    /** @override */
    openHelpPage: function() {
      chrome.send('openHelpPage');
    },

<if expr="_google_chrome">
    /** @override */
    openFeedbackDialog: function() {
      chrome.send('openFeedbackDialog');
    },
</if>

<if expr="chromeos">
    /** @override */
    requestUpdate: function() {
      chrome.send('requestUpdate');
    },

    /** @override */
    setChannel: function(channel, isPowerwashAllowed) {
      chrome.send('setChannel', [channel, isPowerwashAllowed]);
    },

    /** @override */
    getCurrentChannel: function() {
      return cr.sendWithPromise('getCurrentChannel');
    },

    /** @override */
    getTargetChannel: function() {
      return cr.sendWithPromise('getTargetChannel');
    },

    /** @override */
    getVersionInfo: function() {
      return cr.sendWithPromise('getVersionInfo');
    },

    /** @override */
    getRegulatoryInfo: function() {
      return cr.sendWithPromise('getRegulatoryInfo');
    }
</if>
  };

  return {
    AboutPageBrowserProxy: AboutPageBrowserProxy,
    AboutPageBrowserProxyImpl: AboutPageBrowserProxyImpl,
    browserChannelToI18nId: browserChannelToI18nId,
    isTargetChannelMoreStable: isTargetChannelMoreStable,
  };
});
