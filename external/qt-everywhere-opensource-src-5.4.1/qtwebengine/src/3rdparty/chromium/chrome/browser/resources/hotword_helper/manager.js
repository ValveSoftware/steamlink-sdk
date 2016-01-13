// Copyright (c) 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

'use strict';

/**
 * @fileoverview This extension manages communications between Chrome,
 * Google.com pages and the Chrome Hotword extension.
 *
 * This helper extension is required due to the depoyment plan for Chrome M34:
 *
 *  - The hotword extension will be distributed as an externally loaded
 *      component extension.
 *  - Settings for enabling and disabling the hotword extension has moved to
 *      Chrome settings.
 *  - Newtab page is served via chrome://newtab/
 *
 */



/** @constructor */
var OptInManager = function() {};


/**
 * @const {string}
 * @private
 */
OptInManager.HOTWORD_EXTENSION_ID_ = 'bepbmhgboaologfdajaanbcjmnhjmhfn';


/**
 * Commands sent from the page to this content script.
 * @enum {string}
 */
OptInManager.CommandFromPage = {
  // User has explicitly clicked 'no'.
  CLICKED_NO_OPTIN: 'hcno',
  // User has opted in.
  CLICKED_OPTIN: 'hco',
  // Audio logging is opted in.
  AUDIO_LOGGING_ON: 'alon',
  // Audio logging is opted out.
  AUDIO_LOGGING_OFF: 'aloff',
  // User visited an eligible page.
  PAGE_WAKEUP: 'wu'
};


/**
 * @param {Tab} tab Tab to inject.
 * @param {function(HotwordStatus)} sendResponse Callback function to respond
 *     to sender.
 * @param {HotwordStatus} hotwordStatus Status of the hotword extension.
 * @private
 */
OptInManager.prototype.injectTab_ = function(
    tab, sendResponse, hotwordStatus) {
  if (tab.incognito || !hotwordStatus.available) {
    sendResponse({'doNotShowOptinMessage': true});
    return;
  }

  if (!hotwordStatus.enabledSet) {
    chrome.tabs.executeScript(tab.id, {'file': 'optin_client.js'});
    sendResponse(hotwordStatus);
    return;
  }

  if (hotwordStatus.enabled)
    chrome.tabs.executeScript(tab.id, {'file': 'audio_client.js'});
  sendResponse({'doNotShowOptinMessage': true});
};


/**
 * Handles messages from the helper content script.
 * @param {*} request Message from the sender.
 * @param {MessageSender} sender Information about the sender.
 * @param {function(HotwordStatus)} sendResponse Callback function to respond
 *     to sender.
 * @return {boolean} Whether to maintain the port open to call sendResponse.
 * @private
 */
OptInManager.prototype.handleMessage_ = function(
    request, sender, sendResponse) {
  switch (request.type) {
    case OptInManager.CommandFromPage.PAGE_WAKEUP:
      if (((sender.tab && this.isEligibleUrl(sender.tab.url)) ||
          sender.id == OptInManager.HOTWORD_EXTENSION_ID_) &&
          chrome.hotwordPrivate && chrome.hotwordPrivate.getStatus) {
        chrome.hotwordPrivate.getStatus(
            this.injectTab_.bind(this, request.tab || sender.tab,
                                 sendResponse));
        return true;
      }
      break;
    case OptInManager.CommandFromPage.CLICKED_OPTIN:
      if (chrome.hotwordPrivate && chrome.hotwordPrivate.setEnabled &&
          chrome.hotwordPrivate.getStatus) {
        chrome.hotwordPrivate.setEnabled(true);
        chrome.hotwordPrivate.getStatus(
            this.injectTab_.bind(this, sender.tab, sendResponse));
        return true;
      }
      break;
    // User has explicitly clicked 'no thanks'.
    case OptInManager.CommandFromPage.CLICKED_NO_OPTIN:
      if (chrome.hotwordPrivate && chrome.hotwordPrivate.setEnabled) {
        chrome.hotwordPrivate.setEnabled(false);
      }
      break;
    // Information regarding the audio logging preference was sent.
    case OptInManager.CommandFromPage.AUDIO_LOGGING_ON:
      if (chrome.hotwordPrivate &&
          chrome.hotwordPrivate.setAudioLoggingEnabled) {
        chrome.hotwordPrivate.setAudioLoggingEnabled(true);
      }
      break;
    case OptInManager.CommandFromPage.AUDIO_LOGGING_OFF:
      if (chrome.hotwordPrivate &&
          chrome.hotwordPrivate.setAudioLoggingEnabled) {
        chrome.hotwordPrivate.setAudioLoggingEnabled(false);
      }
      break;
    default:
      break;
  }
  return false;
};

/**
 * Helper function to test URLs as being valid for running the
 * hotwording extension. It's used by isEligibleUrl to make that
 * function clearer.
 * @param {string} url URL to check.
 * @param {string} base Base URL to compare against..
 * @return {boolean} True if url is an eligible hotword URL.
 */
OptInManager.prototype.checkEligibleUrl = function(url, base) {
  if (!url)
    return false;

  if (url === base ||
      url === base + '/' ||
      url.indexOf(base + '/_/chrome/newtab?') === 0 ||  // Appcache NTP.
      url.indexOf(base + '/?') === 0 ||
      url.indexOf(base + '/#') === 0 ||
      url.indexOf(base + '/webhp') === 0 ||
      url.indexOf(base + '/search') === 0) {
    return true;
  }
  return false;

};

/**
 * Determines if a URL is eligible for hotwording. For now, the
 * valid pages are the Google HP and SERP (this will include the NTP).
 * @param {string} url URL to check.
 * @return {boolean} True if url is an eligible hotword URL.
 */
OptInManager.prototype.isEligibleUrl = function(url) {
  if (!url)
    return false;

  // More URLs will be added in the future so leaving this as an array.
  var baseUrls = [
    'chrome://newtab'
  ];
  var baseGoogleUrls = [
    'https://www.google.',
    'https://encrypted.google.'
  ];
  var tlds = [
    'com',
    'co.uk',
    'de',
    'fr',
    'ru'
  ];

  // Check URLs which do not have locale-based TLDs first.
  if (this.checkEligibleUrl(url, baseUrls[0]))
    return true;

  // Check URLs with each type of local-based TLD.
  for (var i = 0; i < baseGoogleUrls.length; i++) {
    for (var j = 0; j < tlds.length; j++) {
      var base = baseGoogleUrls[i] + tlds[j];
      if (this.checkEligibleUrl(url, base))
        return true;
    }
  }
  return false;
};


/**
 * Initializes the extension.
 */
OptInManager.prototype.initialize = function() {
  // TODO(rlp): Possibly remove the next line. It's proably not used, but
  // leaving for now to be safe. We should remove it once all messsage
  // relaying is removed form the content scripts.
  chrome.runtime.onMessage.addListener(this.handleMessage_.bind(this));
  chrome.runtime.onMessageExternal.addListener(
      this.handleMessage_.bind(this));
};


new OptInManager().initialize();
