// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

cr.define('hotword', function() {
  'use strict';

  /**
   * Class used to manage the interaction between hotwording and the
   * NTP/google.com. Injects a content script to interact with NTP/google.com
   * and updates the global hotwording state based on interaction with those
   * pages.
   * @param {!hotword.StateManager} stateManager
   * @constructor
   */
  function PageAudioManager(stateManager) {
    /**
     * Manager of global hotwording state.
     * @private {!hotword.StateManager}
     */
    this.stateManager_ = stateManager;

    /**
     * Mapping between tab ID and port that is connected from the injected
     * content script.
     * @private {!Object<number, Port>}
     */
    this.portMap_ = {};

    /**
     * Chrome event listeners. Saved so that they can be de-registered when
     * hotwording is disabled.
     */
    this.connectListener_ = this.handleConnect_.bind(this);
    this.tabCreatedListener_ = this.handleCreatedTab_.bind(this);
    this.tabUpdatedListener_ = this.handleUpdatedTab_.bind(this);
    this.tabActivatedListener_ = this.handleActivatedTab_.bind(this);
    this.microphoneStateChangedListener_ =
        this.handleMicrophoneStateChanged_.bind(this);
    this.windowFocusChangedListener_ = this.handleChangedWindow_.bind(this);
    this.messageListener_ = this.handleMessageFromPage_.bind(this);

    // Need to setup listeners on startup, otherwise events that caused the
    // event page to start up, will be lost.
    this.setupListeners_();

    this.stateManager_.onStatusChanged.addListener(function() {
      this.updateListeners_();
      this.updateTabState_();
    }.bind(this));
  };

  var CommandToPage = hotword.constants.CommandToPage;
  var CommandFromPage = hotword.constants.CommandFromPage;

  PageAudioManager.prototype = {
    /**
     * Helper function to test if a URL path is eligible for hotwording.
     * @param {!string} url URL to check.
     * @param {!string} base Base URL to compare against..
     * @return {boolean} True if url is an eligible hotword URL.
     * @private
     */
    checkUrlPathIsEligible_: function(url, base) {
      if (url == base ||
          url == base + '/' ||
          url.indexOf(base + '/_/chrome/newtab?') == 0 ||  // Appcache NTP.
          url.indexOf(base + '/?') == 0 ||
          url.indexOf(base + '/#') == 0 ||
          url.indexOf(base + '/webhp') == 0 ||
          url.indexOf(base + '/search') == 0 ||
          url.indexOf(base + '/imghp') == 0) {
        return true;
      }
      return false;
    },

    /**
     * Determines if a URL is eligible for hotwording. For now, the valid pages
     * are the Google HP and SERP (this will include the NTP).
     * @param {!string} url URL to check.
     * @return {boolean} True if url is an eligible hotword URL.
     * @private
     */
    isEligibleUrl_: function(url) {
      if (!url)
        return false;

      var baseGoogleUrls = [
        'https://encrypted.google.',
        'https://images.google.',
        'https://www.google.'
      ];
      // TODO(amistry): Get this list from a file in the shared module instead.
      var tlds = [
        'at',
        'ca',
        'com',
        'com.au',
        'com.mx',
        'com.br',
        'co.jp',
        'co.kr',
        'co.nz',
        'co.uk',
        'co.za',
        'de',
        'es',
        'fr',
        'it',
        'ru'
      ];

      // Check for the new tab page first.
      if (this.checkUrlPathIsEligible_(url, 'chrome://newtab'))
        return true;

      // Check URLs with each type of local-based TLD.
      for (var i = 0; i < baseGoogleUrls.length; i++) {
        for (var j = 0; j < tlds.length; j++) {
          var base = baseGoogleUrls[i] + tlds[j];
          if (this.checkUrlPathIsEligible_(url, base))
            return true;
        }
      }
      return false;
    },

    /**
     * Locates the current active tab in the current focused window and
     * performs a callback with the tab as the parameter.
     * @param {function(?Tab)} callback Function to call with the
     *     active tab or null if not found. The function's |this| will be set to
     *     this object.
     * @private
     */
    findCurrentTab_: function(callback) {
      chrome.windows.getAll(
          {'populate': true},
          function(windows) {
            for (var i = 0; i < windows.length; ++i) {
              if (!windows[i].focused)
                continue;

              for (var j = 0; j < windows[i].tabs.length; ++j) {
                var tab = windows[i].tabs[j];
                if (tab.active) {
                  callback.call(this, tab);
                  return;
                }
              }
            }
            callback.call(this, null);
          }.bind(this));
    },

    /**
     * This function is called when a tab is activated (comes into focus).
     * @param {Tab} tab Current active tab.
     * @private
     */
    activateTab_: function(tab) {
      if (!tab) {
        this.stopHotwording_();
        return;
      }
      if (tab.id in this.portMap_) {
        this.startHotwordingIfEligible_();
        return;
      }
      this.stopHotwording_();
      this.prepareTab_(tab);
    },

    /**
     * Prepare a new or updated tab by injecting the content script.
     * @param {!Tab} tab Newly updated or created tab.
     * @private
     */
    prepareTab_: function(tab) {
      if (!this.isEligibleUrl_(tab.url))
        return;

      chrome.tabs.executeScript(
          tab.id,
          {'file': 'audio_client.js'},
          function(results) {
            if (chrome.runtime.lastError) {
              // Ignore this error. For new tab pages, even though the URL is
              // reported to be chrome://newtab/, the actual URL is a
              // country-specific google domain. Since we don't have permission
              // to inject on every page, an error will happen when the user is
              // in an unsupported country.
              //
              // The property still needs to be accessed so that the error
              // condition is cleared. If it isn't, exectureScript will log an
              // error the next time it is called.
            }
          });
    },

    /**
     * Updates hotwording state based on the state of current tabs/windows.
     * @private
     */
    updateTabState_: function() {
      this.findCurrentTab_(this.activateTab_);
    },

    /**
     * Handles a newly created tab.
     * @param {!Tab} tab Newly created tab.
     * @private
     */
    handleCreatedTab_: function(tab) {
      this.prepareTab_(tab);
    },

    /**
     * Handles an updated tab.
     * @param {number} tabId Id of the updated tab.
     * @param {{status: string}} info Change info of the tab.
     * @param {!Tab} tab Updated tab.
     * @private
     */
    handleUpdatedTab_: function(tabId, info, tab) {
      // Chrome fires multiple update events: undefined, loading and completed.
      // We perform content injection on loading state.
      if (info['status'] != 'loading')
        return;

      this.prepareTab_(tab);
    },

    /**
     * Handles a tab that has just become active.
     * @param {{tabId: number}} info Information about the activated tab.
     * @private
     */
    handleActivatedTab_: function(info) {
      this.updateTabState_();
    },

    /**
     * Handles the microphone state changing.
     * @param {boolean} enabled Whether the microphone is now enabled.
     * @private
     */
    handleMicrophoneStateChanged_: function(enabled) {
      if (enabled) {
        this.updateTabState_();
        return;
      }

      this.stopHotwording_();
    },

    /**
     * Handles a change in Chrome windows.
     * Note: this does not always trigger in Linux.
     * @param {number} windowId Id of newly focused window.
     * @private
     */
    handleChangedWindow_: function(windowId) {
      this.updateTabState_();
    },

    /**
     * Handles a content script attempting to connect.
     * @param {!Port} port Communications port from the client.
     * @private
     */
    handleConnect_: function(port) {
      if (port.name != hotword.constants.CLIENT_PORT_NAME)
        return;

      var tab = /** @type {!Tab} */(port.sender.tab);
      // An existing port from the same tab might already exist. But that port
      // may be from the previous page, so just overwrite the port.
      this.portMap_[tab.id] = port;
      port.onDisconnect.addListener(function() {
        this.handleClientDisconnect_(port);
      }.bind(this));
      port.onMessage.addListener(function(msg) {
        this.handleMessage_(msg, port.sender, port.postMessage);
      }.bind(this));
    },

    /**
     * Handles a client content script disconnect.
     * @param {Port} port Disconnected port.
     * @private
     */
    handleClientDisconnect_: function(port) {
      var tabId = port.sender.tab.id;
      if (tabId in this.portMap_ && this.portMap_[tabId] == port) {
        // Due to a race between port disconnection and tabs.onUpdated messages,
        // the port could have changed.
        delete this.portMap_[port.sender.tab.id];
      }
      this.stopHotwordingIfIneligibleTab_();
    },

    /**
     * Disconnect all connected clients.
     * @private
     */
    disconnectAllClients_: function() {
      for (var id in this.portMap_) {
        var port = this.portMap_[id];
        port.disconnect();
        delete this.portMap_[id];
      }
    },

    /**
     * Sends a command to the client content script on an eligible tab.
     * @param {hotword.constants.CommandToPage} command Command to send.
     * @param {number} tabId Id of the target tab.
     * @private
     */
    sendClient_: function(command, tabId) {
      if (tabId in this.portMap_) {
        var message = {};
        message[hotword.constants.COMMAND_FIELD_NAME] = command;
        this.portMap_[tabId].postMessage(message);
      }
    },

    /**
     * Sends a command to all connected clients.
     * @param {hotword.constants.CommandToPage} command Command to send.
     * @private
     */
    sendAllClients_: function(command) {
      for (var idStr in this.portMap_) {
        var id = parseInt(idStr, 10);
        assert(!isNaN(id), 'Tab ID is not a number: ' + idStr);
        this.sendClient_(command, id);
      }
    },

    /**
     * Handles a hotword trigger. Sends a trigger message to the currently
     * active tab.
     * @private
     */
    hotwordTriggered_: function() {
      this.findCurrentTab_(function(tab) {
        if (tab)
          this.sendClient_(CommandToPage.HOTWORD_VOICE_TRIGGER, tab.id);
      });
    },

    /**
     * Starts hotwording.
     * @private
     */
    startHotwording_: function() {
      this.stateManager_.startSession(
          hotword.constants.SessionSource.NTP,
          function() {
            this.sendAllClients_(CommandToPage.HOTWORD_STARTED);
          }.bind(this),
          this.hotwordTriggered_.bind(this));
    },

    /**
     * Starts hotwording if the currently active tab is eligible for hotwording
     * (e.g. google.com).
     * @private
     */
    startHotwordingIfEligible_: function() {
      this.findCurrentTab_(function(tab) {
        if (!tab) {
          this.stopHotwording_();
          return;
        }
        if (this.isEligibleUrl_(tab.url))
          this.startHotwording_();
      });
    },

    /**
     * Stops hotwording.
     * @private
     */
    stopHotwording_: function() {
      this.stateManager_.stopSession(hotword.constants.SessionSource.NTP);
      this.sendAllClients_(CommandToPage.HOTWORD_ENDED);
    },

    /**
     * Stops hotwording if the currently active tab is not eligible for
     * hotwording (i.e. google.com).
     * @private
     */
    stopHotwordingIfIneligibleTab_: function() {
      this.findCurrentTab_(function(tab) {
        if (!tab) {
          this.stopHotwording_();
          return;
        }
        if (!this.isEligibleUrl_(tab.url))
          this.stopHotwording_();
      });
    },

    /**
     * Handles a message from the content script injected into the page.
     * @param {!Object} request Request from the content script.
     * @param {!MessageSender} sender Message sender.
     * @param {!function(Object)} sendResponse Function for sending a response.
     * @private
     */
    handleMessage_: function(request, sender, sendResponse) {
      switch (request[hotword.constants.COMMAND_FIELD_NAME]) {
        // TODO(amistry): Handle other messages such as CLICKED_RESUME and
        // CLICKED_RESTART, if necessary.
        case CommandFromPage.SPEECH_START:
          this.stopHotwording_();
          break;
        case CommandFromPage.SPEECH_END:
        case CommandFromPage.SPEECH_RESET:
          this.startHotwording_();
          break;
      }
    },


    /**
     * Handles a message directly from the NTP/HP/SERP.
     * @param {!Object} request Message from the sender.
     * @param {!MessageSender} sender Information about the sender.
     * @param {!function(HotwordStatus)} sendResponse Callback to respond
     *     to sender.
     * @return {boolean} Whether to maintain the port open to call sendResponse.
     * @private
     */
    handleMessageFromPage_: function(request, sender, sendResponse) {
      switch (request.type) {
        case CommandFromPage.PAGE_WAKEUP:
          if (sender.tab && this.isEligibleUrl_(sender.tab.url)) {
            chrome.hotwordPrivate.getStatus(
                true /* getOptionalFields */,
                this.statusDone_.bind(
                    this,
                    request.tab || sender.tab || {incognito: true},
                    sendResponse));
            return true;
          }

          // Do not show the opt-in promo for ineligible urls.
          this.sendResponse_({'doNotShowOptinMessage': true}, sendResponse);
          break;
        case CommandFromPage.CLICKED_OPTIN:
          chrome.hotwordPrivate.setEnabled(true);
          break;
        // User has explicitly clicked 'no thanks'.
        case CommandFromPage.CLICKED_NO_OPTIN:
          chrome.hotwordPrivate.setEnabled(false);
          break;
      }
      return false;
    },

    /**
     * Sends a message directly to the sending page.
     * @param {!HotwordStatus} response The response to send to the sender.
     * @param {!function(HotwordStatus)} sendResponse Callback to respond
     *     to sender.
     * @private
     */
    sendResponse_: function(response, sendResponse) {
      try {
        sendResponse(response);
      } catch (err) {
        // Suppress the exception thrown by sendResponse() when the page doesn't
        // specify a response callback in the call to
        // chrome.runtime.sendMessage().
        // Unfortunately, there doesn't appear to be a way to detect one-way
        // messages without explicitly saying in the message itself. This
        // message is defined as a constant in
        // extensions/renderer/messaging_bindings.cc
        if (err.message == 'Attempting to use a disconnected port object')
          return;
        throw err;
      }
    },

    /**
     * Sends the response to the tab.
     * @param {Tab} tab The tab that the request was sent from.
     * @param {function(HotwordStatus)} sendResponse Callback function to
     *     respond to sender.
     * @param {HotwordStatus} hotwordStatus Status of the hotword extension.
     * @private
     */
    statusDone_: function(tab, sendResponse, hotwordStatus) {
      var response = {'doNotShowOptinMessage': true};

      // If always-on is available, then we do not show the promo, as the promo
      // only works with the sometimes-on pref.
      if (!tab.incognito && hotwordStatus.available &&
          !hotwordStatus.enabledSet && !hotwordStatus.alwaysOnAvailable) {
        response = hotwordStatus;
      }

      this.sendResponse_(response, sendResponse);
    },

    /**
     * Set up event listeners.
     * @private
     */
    setupListeners_: function() {
      if (chrome.runtime.onConnect.hasListener(this.connectListener_))
        return;

      chrome.runtime.onConnect.addListener(this.connectListener_);
      chrome.tabs.onCreated.addListener(this.tabCreatedListener_);
      chrome.tabs.onUpdated.addListener(this.tabUpdatedListener_);
      chrome.tabs.onActivated.addListener(this.tabActivatedListener_);
      chrome.windows.onFocusChanged.addListener(
          this.windowFocusChangedListener_);
      chrome.hotwordPrivate.onMicrophoneStateChanged.addListener(
          this.microphoneStateChangedListener_);
      if (chrome.runtime.onMessage.hasListener(this.messageListener_))
        return;
      chrome.runtime.onMessageExternal.addListener(
          this.messageListener_);
    },

    /**
     * Remove event listeners.
     * @private
     */
    removeListeners_: function() {
      chrome.runtime.onConnect.removeListener(this.connectListener_);
      chrome.tabs.onCreated.removeListener(this.tabCreatedListener_);
      chrome.tabs.onUpdated.removeListener(this.tabUpdatedListener_);
      chrome.tabs.onActivated.removeListener(this.tabActivatedListener_);
      chrome.windows.onFocusChanged.removeListener(
          this.windowFocusChangedListener_);
      chrome.hotwordPrivate.onMicrophoneStateChanged.removeListener(
          this.microphoneStateChangedListener_);
      // Don't remove the Message listener, as we want them listening all
      // the time,
    },

    /**
     * Update event listeners based on the current hotwording state.
     * @private
     */
    updateListeners_: function() {
      if (this.stateManager_.isSometimesOnEnabled()) {
        this.setupListeners_();
      } else {
        this.removeListeners_();
        this.stopHotwording_();
        this.disconnectAllClients_();
      }
    }
  };

  return {
    PageAudioManager: PageAudioManager
  };
});
