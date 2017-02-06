// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

/**
 * @fileoverview
 * A background script of the auth extension that bridges the communication
 * between the main and injected scripts.
 *
 * Here is an overview of the communication flow when SAML is being used:
 * 1. The main script sends the |startAuth| signal to this background script,
 *    indicating that the authentication flow has started and SAML pages may be
 *    loaded from now on.
 * 2. A script is injected into each SAML page. The injected script sends three
 *    main types of messages to this background script:
 *    a) A |pageLoaded| message is sent when the page has been loaded. This is
 *       forwarded to the main script as |onAuthPageLoaded|.
 *    b) If the SAML provider supports the credential passing API, the API calls
 *       are sent to this background script as |apiCall| messages. These
 *       messages are forwarded unmodified to the main script.
 *    c) The injected script scrapes passwords. They are sent to this background
 *       script in |updatePassword| messages. The main script can request a list
 *       of the scraped passwords by sending the |getScrapedPasswords| message.
 */

/**
 * BackgroundBridgeManager maintains an array of BackgroundBridge, indexed by
 * the associated tab id.
 */
function BackgroundBridgeManager() {
  this.bridges_ = {};
}

BackgroundBridgeManager.prototype = {
  CONTINUE_URL_BASE: 'chrome-extension://mfffpogegjflfpflabcdkioaeobkgjik' +
                     '/success.html',
  // Maps a tab id to its associated BackgroundBridge.
  bridges_: null,

  run: function() {
    chrome.runtime.onConnect.addListener(this.onConnect_.bind(this));

    chrome.webRequest.onBeforeRequest.addListener(
        function(details) {
          if (this.bridges_[details.tabId])
            return this.bridges_[details.tabId].onInsecureRequest(details.url);
        }.bind(this),
        {urls: ['http://*/*', 'file://*/*', 'ftp://*/*']},
        ['blocking']);

    chrome.webRequest.onBeforeSendHeaders.addListener(
        function(details) {
          if (this.bridges_[details.tabId])
            return this.bridges_[details.tabId].onBeforeSendHeaders(details);
          else
            return {requestHeaders: details.requestHeaders};
        }.bind(this),
        {urls: ['*://*/*'], types: ['sub_frame']},
        ['blocking', 'requestHeaders']);

    chrome.webRequest.onHeadersReceived.addListener(
        function(details) {
          if (this.bridges_[details.tabId])
            return this.bridges_[details.tabId].onHeadersReceived(details);
        }.bind(this),
        {urls: ['*://*/*'], types: ['sub_frame']},
        ['blocking', 'responseHeaders']);

    chrome.webRequest.onCompleted.addListener(
        function(details) {
          if (this.bridges_[details.tabId])
            this.bridges_[details.tabId].onCompleted(details);
        }.bind(this),
        {urls: ['*://*/*', this.CONTINUE_URL_BASE + '*'], types: ['sub_frame']},
        ['responseHeaders']);
  },

  onConnect_: function(port) {
    var tabId = this.getTabIdFromPort_(port);
    if (!this.bridges_[tabId])
      this.bridges_[tabId] = new BackgroundBridge(tabId);
    if (port.name == 'authMain') {
      this.bridges_[tabId].setupForAuthMain(port);
      port.onDisconnect.addListener(function() {
        delete this.bridges_[tabId];
      }.bind(this));
    } else if (port.name == 'injected') {
      this.bridges_[tabId].setupForInjected(port);
    } else {
      console.error('Unexpected connection, port.name=' + port.name);
    }
  },

  getTabIdFromPort_: function(port) {
    return port.sender.tab ? port.sender.tab.id : -1;
  }
};

/**
 * BackgroundBridge allows the main script and the injected script to
 * collaborate. It forwards credentials API calls to the main script and
 * maintains a list of scraped passwords.
 * @param {string} tabId The associated tab ID.
 */
function BackgroundBridge(tabId) {
  this.tabId_ = tabId;
  this.passwordStore_ = {};
}

BackgroundBridge.prototype = {
  // The associated tab ID. Only used for debugging now.
  tabId: null,

  // The initial URL loaded in the gaia iframe.  We only want to handle
  // onCompleted() for the frame that loaded this URL.
  initialFrameUrlWithoutParams: null,

  // On process onCompleted() requests that come from this frame Id.
  frameId: -1,

  isDesktopFlow_: false,

  // Whether the extension is loaded in a constrained window.
  // Set from main auth script.
  isConstrainedWindow_: null,

  // Email of the newly authenticated user based on the gaia response header
  // 'google-accounts-signin'.
  email_: null,

  // Gaia Id of the newly authenticated user based on the gaia response
  // header 'google-accounts-signin'.
  gaiaId_: null,

  // Session index of the newly authenticated user based on the gaia response
  // header 'google-accounts-signin'.
  sessionIndex_: null,

  // Gaia URL base that is set from main auth script.
  gaiaUrl_: null,

  // Whether to abort the authentication flow and show an error messagen when
  // content served over an unencrypted connection is detected.
  blockInsecureContent_: false,

  // Whether auth flow has started. It is used as a signal of whether the
  // injected script should scrape passwords.
  authStarted_: false,

  // Whether SAML flow is going.
  isSAML_: false,

  passwordStore_: null,

  channelMain_: null,
  channelInjected_: null,

  /**
   * Sets up the communication channel with the main script.
   */
  setupForAuthMain: function(port) {
    this.channelMain_ = new Channel();
    this.channelMain_.init(port);

    // Registers for desktop related messages.
    this.channelMain_.registerMessage(
        'initDesktopFlow', this.onInitDesktopFlow_.bind(this));

    // Registers for SAML related messages.
    this.channelMain_.registerMessage(
        'setGaiaUrl', this.onSetGaiaUrl_.bind(this));
    this.channelMain_.registerMessage(
        'setBlockInsecureContent', this.onSetBlockInsecureContent_.bind(this));
    this.channelMain_.registerMessage(
        'resetAuth', this.onResetAuth_.bind(this));
    this.channelMain_.registerMessage(
        'startAuth', this.onAuthStarted_.bind(this));
    this.channelMain_.registerMessage(
        'getScrapedPasswords',
        this.onGetScrapedPasswords_.bind(this));
    this.channelMain_.registerMessage(
        'apiResponse', this.onAPIResponse_.bind(this));

    this.channelMain_.send({
      'name': 'channelConnected'
    });
  },

  /**
   * Sets up the communication channel with the injected script.
   */
  setupForInjected: function(port) {
    this.channelInjected_ = new Channel();
    this.channelInjected_.init(port);

    this.channelInjected_.registerMessage(
        'apiCall', this.onAPICall_.bind(this));
    this.channelInjected_.registerMessage(
        'updatePassword', this.onUpdatePassword_.bind(this));
    this.channelInjected_.registerMessage(
        'pageLoaded', this.onPageLoaded_.bind(this));
    this.channelInjected_.registerMessage(
        'getSAMLFlag', this.onGetSAMLFlag_.bind(this));
  },

  /**
   * Handler for 'initDesktopFlow' signal sent from the main script.
   * Only called in desktop mode.
   */
  onInitDesktopFlow_: function(msg) {
    this.isDesktopFlow_ = true;
    this.gaiaUrl_ = msg.gaiaUrl;
    this.isConstrainedWindow_ = msg.isConstrainedWindow;
    this.initialFrameUrlWithoutParams = msg.initialFrameUrlWithoutParams;
  },

  /**
   * Handler for webRequest.onCompleted. It 1) detects loading of continue URL
   * and notifies the main script of signin completion; 2) detects if the
   * current page could be loaded in a constrained window and signals the main
   * script of switching to full tab if necessary.
   */
  onCompleted: function(details) {
    // Only monitors requests in the gaia frame.  The gaia frame is the one
    // where the initial frame URL completes.
    if (details.url.lastIndexOf(
            this.initialFrameUrlWithoutParams, 0) == 0) {
      this.frameId = details.frameId;
    }
    if (this.frameId == -1) {
      // If for some reason the frameId could not be set above, just make sure
      // the frame is more than two levels deep (since the gaia frame is at
      // least three levels deep).
      if (details.parentFrameId <= 0)
        return;
    } else if (details.frameId != this.frameId) {
      return;
    }

    if (details.url.lastIndexOf(backgroundBridgeManager.CONTINUE_URL_BASE, 0) ==
        0) {
      var skipForNow = false;
      if (details.url.indexOf('ntp=1') >= 0)
        skipForNow = true;

      // TOOD(guohui): For desktop SAML flow, show password confirmation UI.
      var passwords = this.onGetScrapedPasswords_();
      var msg = {
        'name': 'completeLogin',
        'email': this.email_,
        'gaiaId': this.gaiaId_,
        'password': passwords[0],
        'sessionIndex': this.sessionIndex_,
        'skipForNow': skipForNow
      };
      this.channelMain_.send(msg);
    } else if (this.isConstrainedWindow_) {
      // The header google-accounts-embedded is only set on gaia domain.
      if (this.gaiaUrl_ && details.url.lastIndexOf(this.gaiaUrl_) == 0) {
        var headers = details.responseHeaders;
        for (var i = 0; headers && i < headers.length; ++i) {
          if (headers[i].name.toLowerCase() == 'google-accounts-embedded')
            return;
        }
      }
      var msg = {
        'name': 'switchToFullTab',
        'url': details.url
      };
      this.channelMain_.send(msg);
    }
  },

  /**
   * Handler for webRequest.onBeforeRequest, invoked when content served over an
   * unencrypted connection is detected. Determines whether the request should
   * be blocked and if so, signals that an error message needs to be shown.
   * @param {string} url The URL that was blocked.
   * @return {!Object} Decision whether to block the request.
   */
  onInsecureRequest: function(url) {
    if (!this.blockInsecureContent_)
      return {};
    this.channelMain_.send({name: 'onInsecureContentBlocked', url: url});
    return {cancel: true};
  },

  /**
   * Handler or webRequest.onHeadersReceived. It reads the authenticated user
   * email from google-accounts-signin-header.
   * @return {!Object} Modified request headers.
   */
  onHeadersReceived: function(details) {
    var headers = details.responseHeaders;

    if (this.gaiaUrl_ && details.url.lastIndexOf(this.gaiaUrl_) == 0) {
      for (var i = 0; headers && i < headers.length; ++i) {
        if (headers[i].name.toLowerCase() == 'google-accounts-signin') {
          var headerValues = headers[i].value.toLowerCase().split(',');
          var signinDetails = {};
          headerValues.forEach(function(e) {
            var pair = e.split('=');
            signinDetails[pair[0].trim()] = pair[1].trim();
          });
          // Remove "" around.
          this.email_ = signinDetails['email'].slice(1, -1);
          this.gaiaId_ = signinDetails['obfuscatedid'].slice(1, -1);
          this.sessionIndex_ = signinDetails['sessionindex'];
          break;
        }
      }
    }

    if (!this.isDesktopFlow_) {
      // Check whether GAIA headers indicating the start or end of a SAML
      // redirect are present. If so, synthesize cookies to mark these points.
      for (var i = 0; headers && i < headers.length; ++i) {
        if (headers[i].name.toLowerCase() == 'google-accounts-saml') {
          var action = headers[i].value.toLowerCase();
          if (action == 'start') {
            this.isSAML_ = true;
            // GAIA is redirecting to a SAML IdP. Any cookies contained in the
            // current |headers| were set by GAIA. Any cookies set in future
            // requests will be coming from the IdP. Append a cookie to the
            // current |headers| that marks the point at which the redirect
            // occurred.
            headers.push({name: 'Set-Cookie',
                          value: 'google-accounts-saml-start=now'});
            return {responseHeaders: headers};
          } else if (action == 'end') {
            this.isSAML_ = false;
            // The SAML IdP has redirected back to GAIA. Add a cookie that marks
            // the point at which the redirect occurred occurred. It is
            // important that this cookie be prepended to the current |headers|
            // because any cookies contained in the |headers| were already set
            // by GAIA, not the IdP. Due to limitations in the webRequest API,
            // it is not trivial to prepend a cookie:
            //
            // The webRequest API only allows for deleting and appending
            // headers. To prepend a cookie (C), three steps are needed:
            // 1) Delete any headers that set cookies (e.g., A, B).
            // 2) Append a header which sets the cookie (C).
            // 3) Append the original headers (A, B).
            //
            // Due to a further limitation of the webRequest API, it is not
            // possible to delete a header in step 1) and append an identical
            // header in step 3). To work around this, a trailing semicolon is
            // added to each header before appending it. Trailing semicolons are
            // ignored by Chrome in cookie headers, causing the modified headers
            // to actually set the original cookies.
            var otherHeaders = [];
            var cookies = [{name: 'Set-Cookie',
                            value: 'google-accounts-saml-end=now'}];
            for (var j = 0; j < headers.length; ++j) {
              if (headers[j].name.toLowerCase().indexOf('set-cookie') == 0) {
                var header = headers[j];
                header.value += ';';
                cookies.push(header);
              } else {
                otherHeaders.push(headers[j]);
              }
            }
            return {responseHeaders: otherHeaders.concat(cookies)};
          }
        }
      }
    }

    return {};
  },

  /**
   * Handler for webRequest.onBeforeSendHeaders.
   * @return {!Object} Modified request headers.
   */
  onBeforeSendHeaders: function(details) {
    if (!this.isDesktopFlow_ && this.gaiaUrl_ &&
        details.url.indexOf(this.gaiaUrl_) == 0) {
      details.requestHeaders.push({
        name: 'X-Cros-Auth-Ext-Support',
        value: 'SAML'
      });
    }
    return {requestHeaders: details.requestHeaders};
  },

  /**
   * Handler for 'setGaiaUrl' signal sent from the main script.
   */
  onSetGaiaUrl_: function(msg) {
    this.gaiaUrl_ = msg.gaiaUrl;
  },

  /**
   * Handler for 'setBlockInsecureContent' signal sent from the main script.
   */
  onSetBlockInsecureContent_: function(msg) {
    this.blockInsecureContent_ = msg.blockInsecureContent;
  },

  /**
   * Handler for 'resetAuth' signal sent from the main script.
   */
  onResetAuth_: function() {
    this.authStarted_ = false;
    this.passwordStore_ = {};
    this.isSAML_ = false;
  },

  /**
   * Handler for 'authStarted' signal sent from the main script.
   */
  onAuthStarted_: function() {
    this.authStarted_ = true;
    this.passwordStore_ = {};
    this.isSAML_ = false;
  },

  /**
   * Handler for 'getScrapedPasswords' request sent from the main script.
   * @return {Array<string>} The array with de-duped scraped passwords.
   */
  onGetScrapedPasswords_: function() {
    var passwords = {};
    for (var property in this.passwordStore_) {
      passwords[this.passwordStore_[property]] = true;
    }
    return Object.keys(passwords);
  },

  /**
   * Handler for 'apiResponse' signal sent from the main script. Passes on the
   * |msg| to the injected script.
   */
  onAPIResponse_: function(msg) {
    this.channelInjected_.send(msg);
  },

  onAPICall_: function(msg) {
    this.channelMain_.send(msg);
  },

  onUpdatePassword_: function(msg) {
    if (!this.authStarted_)
      return;

    this.passwordStore_[msg.id] = msg.password;
  },

  onPageLoaded_: function(msg) {
    if (this.channelMain_)
      this.channelMain_.send({name: 'onAuthPageLoaded',
                              url: msg.url,
                              isSAMLPage: this.isSAML_});
  },

  onGetSAMLFlag_: function(msg) {
    return this.isSAML_;
  }
};

var backgroundBridgeManager = new BackgroundBridgeManager();
backgroundBridgeManager.run();
