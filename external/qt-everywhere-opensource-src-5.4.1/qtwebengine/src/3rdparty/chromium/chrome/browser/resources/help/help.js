// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

<include src="../uber/uber_utils.js">

cr.define('help', function() {
  /**
   * Encapsulated handling of the help page.
   */
  function HelpPage() {}

  cr.addSingletonGetter(HelpPage);

  HelpPage.prototype = {
    __proto__: help.HelpBasePage.prototype,

    /**
     * True if after update powerwash button should be displayed.
     * @private
     */
    powerwashAfterUpdate_: false,

    /**
     * List of the channels names.
     * @private
     */
    channelList_: ['dev-channel', 'beta-channel', 'stable-channel'],

    /**
     * Bubble for error messages and notifications.
     * @private
     */
    bubble_: null,

    /**
     * Name of the channel the device is currently on.
     * @private
     */
    currentChannel_: null,

    /**
     * Name of the channel the device is supposed to be on.
     * @private
     */
    targetChannel_: null,

    /**
     * Perform initial setup.
     */
    initialize: function() {
      help.HelpBasePage.prototype.initialize.call(this, 'help-page');

      var self = this;

      uber.onContentFrameLoaded();

      // Set the title.
      uber.setTitle(loadTimeData.getString('aboutTitle'));

      $('product-license').innerHTML = loadTimeData.getString('productLicense');
      if (cr.isChromeOS) {
        $('product-os-license').innerHTML =
            loadTimeData.getString('productOsLicense');
      }

      var productTOS = $('product-tos');
      if (productTOS)
        productTOS.innerHTML = loadTimeData.getString('productTOS');

      $('get-help').onclick = function() {
        chrome.send('openHelpPage');
      };
<if expr="_google_chrome">
      $('report-issue').onclick = function() {
        chrome.send('openFeedbackDialog');
      };
</if>

      this.maybeSetOnClick_($('more-info-expander'),
          this.toggleMoreInfo_.bind(this));

      this.maybeSetOnClick_($('promote'), function() {
        chrome.send('promoteUpdater');
      });
      this.maybeSetOnClick_($('relaunch'), function() {
        chrome.send('relaunchNow');
      });
      if (cr.isChromeOS) {
        this.maybeSetOnClick_($('relaunch-and-powerwash'), function() {
          chrome.send('relaunchAndPowerwash');
        });

        this.channelTable_ = {
          'stable-channel': {
              'name': loadTimeData.getString('stable'),
              'label': loadTimeData.getString('currentChannelStable'),
          },
          'beta-channel': {
              'name': loadTimeData.getString('beta'),
              'label': loadTimeData.getString('currentChannelBeta')
          },
          'dev-channel': {
              'name': loadTimeData.getString('dev'),
              'label': loadTimeData.getString('currentChannelDev')
          }
        };
      }

      var channelChanger = $('channel-changer');
      if (channelChanger) {
        channelChanger.onchange = function(event) {
          self.setChannel_(event.target.value, false);
        };
      }

      if (cr.isChromeOS) {
        help.ChannelChangePage.getInstance().initialize();
        this.registerOverlay(help.ChannelChangePage.getInstance());

        cr.ui.overlay.setupOverlay($('overlay-container'));
        cr.ui.overlay.globalInitialization();
        $('overlay-container').addEventListener('cancelOverlay', function() {
          self.closeOverlay();
        });
        $('change-channel').onclick = function() {
          self.showOverlay('channel-change-page');
        };

        var channelChangeDisallowedError = document.createElement('div');
        channelChangeDisallowedError.className = 'channel-change-error-bubble';

        var channelChangeDisallowedIcon = document.createElement('div');
        channelChangeDisallowedIcon.classList.add('help-page-icon-large');
        channelChangeDisallowedIcon.classList.add('channel-change-error-icon');
        channelChangeDisallowedError.appendChild(channelChangeDisallowedIcon);

        var channelChangeDisallowedText = document.createElement('div');
        channelChangeDisallowedText.className = 'channel-change-error-text';
        channelChangeDisallowedText.textContent =
            loadTimeData.getString('channelChangeDisallowedMessage');
        channelChangeDisallowedError.appendChild(channelChangeDisallowedText);

        $('channel-change-disallowed-icon').onclick = function() {
            self.showBubble_(channelChangeDisallowedError,
                             $('help-container'),
                             $('channel-change-disallowed-icon'),
                             cr.ui.ArrowLocation.TOP_END);
        };
      }

      cr.ui.FocusManager.disableMouseFocusOnButtons();
      help.HelpFocusManager.getInstance().initialize();

      // Attempt to update.
      chrome.send('onPageLoaded');
    },

    /**
     * Shows the bubble.
     * @param {HTMLDivElement} content The content of the bubble.
     * @param {HTMLElement} target The element at which the bubble points.
     * @param {HTMLElement} domSibling The element after which the bubble is
     *     added to the DOM.
     * @param {cr.ui.ArrowLocation} location The arrow location.
     * @private
     */
    showBubble_: function(content, domSibling, target, location) {
      if (!cr.isChromeOS)
        return;
      this.hideBubble_();
      var bubble = new cr.ui.AutoCloseBubble;
      bubble.anchorNode = target;
      bubble.domSibling = domSibling;
      bubble.arrowLocation = location;
      bubble.content = content;
      bubble.show();
      this.bubble_ = bubble;
    },

    /**
     * Hides the bubble.
     * @private
     */
    hideBubble_: function() {
      if (!cr.isChromeOS)
        return;
      if (this.bubble_)
        this.bubble_.hide();
    },

    /**
     * Toggles the visible state of the 'More Info' section.
     * @private
     */
    toggleMoreInfo_: function() {
      var moreInfo = $('more-info-container');
      var visible = moreInfo.className == 'visible';
      moreInfo.className = visible ? '' : 'visible';
      moreInfo.style.height = visible ? '' : moreInfo.scrollHeight + 'px';
      moreInfo.addEventListener('webkitTransitionEnd', function(event) {
        $('more-info-expander').textContent = visible ?
            loadTimeData.getString('showMoreInfo') :
            loadTimeData.getString('hideMoreInfo');
      });
    },

    /**
     * Assigns |method| to the onclick property of |el| if |el| exists.
     * @private
     */
    maybeSetOnClick_: function(el, method) {
      if (el)
        el.onclick = method;
    },

    /**
     * @private
     */
    setUpdateImage_: function(state) {
      $('update-status-icon').className = 'help-page-icon ' + state;
    },

    /**
     * @return {boolean} True, if new channel switcher UI is used,
     *    false otherwise.
     * @private
     */
    isNewChannelSwitcherUI_: function() {
      return !loadTimeData.valueExists('disableNewChannelSwitcherUI');
    },

    /**
     * @return {boolean} True if target and current channels are not
     *     null and not equals
     * @private
     */
    channelsDiffer_: function() {
      var current = this.currentChannel_;
      var target = this.targetChannel_;
      return (current != null && target != null && current != target);
    },

    /**
     * @private
     */
    setUpdateStatus_: function(status, message) {
      if (cr.isMac &&
          $('update-status-message') &&
          $('update-status-message').hidden) {
        // Chrome has reached the end of the line on this system. The
        // update-obsolete-system message is displayed. No other auto-update
        // status should be displayed.
        return;
      }

      var channel = this.targetChannel_;
      if (status == 'checking') {
        this.setUpdateImage_('working');
        $('update-status-message').innerHTML =
            loadTimeData.getString('updateCheckStarted');
      } else if (status == 'updating') {
        this.setUpdateImage_('working');
        if (this.channelsDiffer_()) {
          $('update-status-message').innerHTML =
              loadTimeData.getStringF('updatingChannelSwitch',
                                      this.channelTable_[channel].label);
        } else {
          $('update-status-message').innerHTML =
              loadTimeData.getStringF('updating');
        }
      } else if (status == 'nearly_updated') {
        this.setUpdateImage_('up-to-date');
        if (this.channelsDiffer_()) {
          $('update-status-message').innerHTML =
              loadTimeData.getString('successfulChannelSwitch');
        } else {
          $('update-status-message').innerHTML =
              loadTimeData.getString('updateAlmostDone');
        }
      } else if (status == 'updated') {
        this.setUpdateImage_('up-to-date');
        $('update-status-message').innerHTML =
            loadTimeData.getString('upToDate');
      } else if (status == 'failed') {
        this.setUpdateImage_('failed');
        $('update-status-message').innerHTML = message;
      }

      // Following invariant must be established at the end of this function:
      // { ~$('relaunch_and_powerwash').hidden -> $('relaunch').hidden }
      var relaunchAndPowerwashHidden = true;
      if ($('relaunch-and-powerwash')) {
        // It's allowed to do powerwash only for customer devices,
        // when user explicitly decides to update to a more stable
        // channel.
        relaunchAndPowerwashHidden =
            !this.powerwashAfterUpdate_ || status != 'nearly_updated';
        $('relaunch-and-powerwash').hidden = relaunchAndPowerwashHidden;
      }

      var container = $('update-status-container');
      if (container) {
        container.hidden = status == 'disabled';
        $('relaunch').hidden =
            (status != 'nearly_updated') || !relaunchAndPowerwashHidden;

        if (!cr.isMac)
          $('update-percentage').hidden = status != 'updating';
      }
    },

    /**
     * @private
     */
    setProgress_: function(progress) {
      $('update-percentage').innerHTML = progress + '%';
    },

    /**
     * @private
     */
    setAllowedConnectionTypesMsg_: function(message) {
      $('allowed-connection-types-message').innerText = message;
    },

    /**
     * @private
     */
    showAllowedConnectionTypesMsg_: function(visible) {
      $('allowed-connection-types-message').hidden = !visible;
    },

    /**
     * @private
     */
    setPromotionState_: function(state) {
      if (state == 'hidden') {
        $('promote').hidden = true;
      } else if (state == 'enabled') {
        $('promote').disabled = false;
        $('promote').hidden = false;
      } else if (state == 'disabled') {
        $('promote').disabled = true;
        $('promote').hidden = false;
      }
    },

    /**
     * @private
     */
    setObsoleteSystem_: function(obsolete) {
      if (cr.isMac && $('update-obsolete-system-container')) {
        $('update-obsolete-system-container').hidden = !obsolete;
      }
    },

    /**
     * @private
     */
    setObsoleteSystemEndOfTheLine_: function(endOfTheLine) {
      if (cr.isMac &&
          $('update-obsolete-system-container') &&
          !$('update-obsolete-system-container').hidden &&
          $('update-status-message')) {
        $('update-status-message').hidden = endOfTheLine;
        if (endOfTheLine) {
          this.setUpdateImage_('failed');
        }
      }
    },

    /**
     * @private
     */
    setOSVersion_: function(version) {
      if (!cr.isChromeOS)
        console.error('OS version unsupported on non-CrOS');

      $('os-version').parentNode.hidden = (version == '');
      $('os-version').textContent = version;
    },

    /**
     * @private
     */
    setOSFirmware_: function(firmware) {
      if (!cr.isChromeOS)
        console.error('OS firmware unsupported on non-CrOS');

      $('firmware').parentNode.hidden = (firmware == '');
      $('firmware').textContent = firmware;
    },

    /**
     * Updates name of the current channel, i.e. the name of the
     * channel the device is currently on.
     * @param {string} channel The name of the current channel
     * @private
     */
    updateCurrentChannel_: function(channel) {
      if (this.channelList_.indexOf(channel) < 0)
        return;
      $('current-channel').textContent = loadTimeData.getStringF(
          'currentChannel', this.channelTable_[channel].label);
      this.currentChannel_ = channel;
      help.ChannelChangePage.updateCurrentChannel(channel);
    },

    /**
     * |enabled| is true if the release channel can be enabled.
     * @private
     */
    updateEnableReleaseChannel_: function(enabled) {
      this.updateChannelChangerContainerVisibility_(enabled);
      $('change-channel').disabled = !enabled;
      $('channel-change-disallowed-icon').hidden = enabled;
    },

    /**
     * Sets the device target channel.
     * @param {string} channel The name of the target channel
     * @param {boolean} isPowerwashAllowed True iff powerwash is allowed
     * @private
     */
    setChannel_: function(channel, isPowerwashAllowed) {
      this.powerwashAfterUpdate_ = isPowerwashAllowed;
      this.targetChannel_ = channel;
      chrome.send('setChannel', [channel, isPowerwashAllowed]);
      $('channel-change-confirmation').hidden = false;
      $('channel-change-confirmation').textContent = loadTimeData.getStringF(
          'channel-changed', this.channelTable_[channel].name);
    },

    /**
     * Sets the value of the "Build Date" field of the "More Info" section.
     * @param {string} buildDate The date of the build.
     * @private
     */
    setBuildDate_: function(buildDate) {
      $('build-date-container').classList.remove('empty');
      $('build-date').textContent = buildDate;
    },

    /**
     * Updates channel-change-page-container visibility according to
     * internal state.
     * @private
     */
    updateChannelChangePageContainerVisibility_: function() {
      if (!this.isNewChannelSwitcherUI_()) {
        $('channel-change-page-container').hidden = true;
        return;
      }
      $('channel-change-page-container').hidden =
          !help.ChannelChangePage.isPageReady();
    },

    /**
     * Updates channel-changer dropdown visibility if |visible| is
     * true and new channel switcher UI is disallowed.
     * @param {boolean} visible True if channel-changer should be
     *     displayed, false otherwise.
     * @private
     */
    updateChannelChangerContainerVisibility_: function(visible) {
      if (this.isNewChannelSwitcherUI_()) {
        $('channel-changer').hidden = true;
        return;
      }
      $('channel-changer').hidden = !visible;
    },
  };

  HelpPage.setUpdateStatus = function(status, message) {
    HelpPage.getInstance().setUpdateStatus_(status, message);
  };

  HelpPage.setProgress = function(progress) {
    HelpPage.getInstance().setProgress_(progress);
  };

  HelpPage.setAndShowAllowedConnectionTypesMsg = function(message) {
    HelpPage.getInstance().setAllowedConnectionTypesMsg_(message);
    HelpPage.getInstance().showAllowedConnectionTypesMsg_(true);
  };

  HelpPage.showAllowedConnectionTypesMsg = function(visible) {
    HelpPage.getInstance().showAllowedConnectionTypesMsg_(visible);
  };

  HelpPage.setPromotionState = function(state) {
    HelpPage.getInstance().setPromotionState_(state);
  };

  HelpPage.setObsoleteSystem = function(obsolete) {
    HelpPage.getInstance().setObsoleteSystem_(obsolete);
  };

  HelpPage.setObsoleteSystemEndOfTheLine = function(endOfTheLine) {
    HelpPage.getInstance().setObsoleteSystemEndOfTheLine_(endOfTheLine);
  };

  HelpPage.setOSVersion = function(version) {
    HelpPage.getInstance().setOSVersion_(version);
  };

  HelpPage.setOSFirmware = function(firmware) {
    HelpPage.getInstance().setOSFirmware_(firmware);
  };

  HelpPage.showOverlay = function(name) {
    HelpPage.getInstance().showOverlay(name);
  };

  HelpPage.cancelOverlay = function() {
    HelpPage.getInstance().closeOverlay();
  };

  HelpPage.getTopmostVisiblePage = function() {
    return HelpPage.getInstance().getTopmostVisiblePage();
  };

  HelpPage.updateIsEnterpriseManaged = function(isEnterpriseManaged) {
    if (!cr.isChromeOS)
      return;
    help.ChannelChangePage.updateIsEnterpriseManaged(isEnterpriseManaged);
  };

  HelpPage.updateCurrentChannel = function(channel) {
    if (!cr.isChromeOS)
      return;
    HelpPage.getInstance().updateCurrentChannel_(channel);
  };

  HelpPage.updateTargetChannel = function(channel) {
    if (!cr.isChromeOS)
      return;
    help.ChannelChangePage.updateTargetChannel(channel);
  };

  HelpPage.updateEnableReleaseChannel = function(enabled) {
    HelpPage.getInstance().updateEnableReleaseChannel_(enabled);
  };

  HelpPage.setChannel = function(channel, isPowerwashAllowed) {
    HelpPage.getInstance().setChannel_(channel, isPowerwashAllowed);
  };

  HelpPage.setBuildDate = function(buildDate) {
    HelpPage.getInstance().setBuildDate_(buildDate);
  };

  HelpPage.updateChannelChangePageContainerVisibility = function() {
    HelpPage.getInstance().updateChannelChangePageContainerVisibility_();
  };

  // Export
  return {
    HelpPage: HelpPage
  };
});

/**
 * onload listener to initialize the HelpPage.
 */
window.onload = function() {
  help.HelpPage.getInstance().initialize();
};
