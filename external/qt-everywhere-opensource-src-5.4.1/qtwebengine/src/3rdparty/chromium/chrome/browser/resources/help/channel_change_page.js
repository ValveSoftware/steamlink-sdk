// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

cr.define('help', function() {
  /**
   * Encapsulated handling of the channel change overlay.
   */
  function ChannelChangePage() {}

  cr.addSingletonGetter(ChannelChangePage);

  ChannelChangePage.prototype = {
    __proto__: help.HelpBasePage.prototype,

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
     * True iff the device is enterprise-managed.
     * @private
     */
    isEnterpriseManaged_: undefined,

    /**
     * List of the channels names, from the least stable to the most stable.
     * @private
     */
    channelList_: ['dev-channel', 'beta-channel', 'stable-channel'],

    /**
     * List of the possible ui states.
     * @private
     */
    uiClassTable_: ['selected-channel-requires-powerwash',
                    'selected-channel-requires-delayed-update',
                    'selected-channel-good',
                    'selected-channel-unstable'],

    /**
     * Perform initial setup.
     */
    initialize: function() {
      help.HelpBasePage.prototype.initialize.call(this, 'channel-change-page');

      var self = this;

      $('channel-change-page-cancel-button').onclick = function() {
        help.HelpPage.cancelOverlay();
      };

      var options = this.getAllChannelOptions_();
      for (var i = 0; i < options.length; i++) {
        var option = options[i];
        option.onclick = function() {
          self.updateUI_(this.value);
        };
      }

      $('channel-change-page-powerwash-button').onclick = function() {
        self.setChannel_(self.getSelectedOption_(), true);
        help.HelpPage.cancelOverlay();
      };

      $('channel-change-page-change-button').onclick = function() {
        self.setChannel_(self.getSelectedOption_(), false);
        help.HelpPage.cancelOverlay();
      };
    },

    onBeforeShow: function() {
      help.HelpBasePage.prototype.onBeforeShow.call(this);
      if (this.targetChannel_ != null)
        this.selectOption_(this.targetChannel_);
      else if (this.currentChannel_ != null)
        this.selectOption_(this.currentChannel_);
      var options = this.getAllChannelOptions_();
      for (var i = 0; i < options.length; i++) {
        var option = options[i];
        if (option.checked)
          option.focus();
      }
    },

    /**
     * Returns the list of all radio buttons responsible for channel selection.
     * @return {Array.<HTMLInputElement>} Array of radio buttons
     * @private
     */
    getAllChannelOptions_: function() {
      return $('channel-change-page').querySelectorAll('input[type="radio"]');
    },

    /**
     * Returns value of the selected option.
     * @return {string} Selected channel name or null, if neither
     *     option is selected.
     * @private
     */
    getSelectedOption_: function() {
      var options = this.getAllChannelOptions_();
      for (var i = 0; i < options.length; i++) {
        var option = options[i];
        if (option.checked)
          return option.value;
      }
      return null;
    },

    /**
     * Selects option for a given channel.
     * @param {string} channel Name of channel option that should be selected.
     * @private
     */
    selectOption_: function(channel) {
      var options = this.getAllChannelOptions_();
      for (var i = 0; i < options.length; i++) {
        var option = options[i];
        if (option.value == channel) {
          option.checked = true;
        }
      }
      this.updateUI_(channel);
    },

    /**
     * Updates UI according to selected channel.
     * @param {string} selectedChannel Selected channel
     * @private
     */
    updateUI_: function(selectedChannel) {
      var currentStability = this.channelList_.indexOf(this.currentChannel_);
      var newStability = this.channelList_.indexOf(selectedChannel);

      var newOverlayClass = null;

      if (selectedChannel == this.currentChannel_) {
        if (this.currentChannel_ != this.targetChannel_) {
          // Allow user to switch back to the current channel.
          newOverlayClass = 'selected-channel-good';
        }
      } else if (selectedChannel != this.targetChannel_) {
        // Selected channel isn't equal to the current and target channel.
        if (newStability > currentStability) {
          // More stable channel is selected. For customer devices
          // notify user about powerwash.
          if (this.isEnterpriseManaged_)
            newOverlayClass = 'selected-channel-requires-delayed-update';
          else
            newOverlayClass = 'selected-channel-requires-powerwash';
        } else if (selectedChannel == 'dev-channel') {
          // Warn user about unstable channel.
          newOverlayClass = 'selected-channel-unstable';
        } else {
          // Switching to the less stable channel.
          newOverlayClass = 'selected-channel-good';
        }
      }

      // Switch to the new UI state.
      for (var i = 0; i < this.uiClassTable_.length; i++)
          $('channel-change-page').classList.remove(this.uiClassTable_[i]);

      if (newOverlayClass)
        $('channel-change-page').classList.add(newOverlayClass);
    },

    /**
     * Sets the device target channel.
     * @param {string} channel The name of the target channel
     * @param {boolean} isPowerwashAllowed True iff powerwash is allowed
     * @private
     */
    setChannel_: function(channel, isPowerwashAllowed) {
      this.targetChannel_ = channel;
      this.updateUI_(channel);
      help.HelpPage.setChannel(channel, isPowerwashAllowed);
    },

    /**
     * Updates page UI according to device owhership policy.
     * @param {boolean} isEnterpriseManaged True if the device is
     *     enterprise managed
     * @private
     */
    updateIsEnterpriseManaged_: function(isEnterpriseManaged) {
      this.isEnterpriseManaged_ = isEnterpriseManaged;
      help.HelpPage.updateChannelChangePageContainerVisibility();
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
      this.currentChannel_ = channel;
      this.selectOption_(channel);
      help.HelpPage.updateChannelChangePageContainerVisibility();
    },

    /**
     * Updates name of the target channel, i.e. the name of the
     * channel the device is supposed to be in case of a pending
     * channel change.
     * @param {string} channel The name of the target channel
     * @private
     */
    updateTargetChannel_: function(channel) {
      if (this.channelList_.indexOf(channel) < 0)
        return;
      this.targetChannel_ = channel;
      help.HelpPage.updateChannelChangePageContainerVisibility();
    },

    /**
     * @return {boolean} True if the page is ready and can be
     *     displayed, false otherwise
     * @private
     */
    isPageReady_: function() {
      if (typeof this.isEnterpriseManaged_ == 'undefined')
        return false;
      if (!this.currentChannel_ || !this.targetChannel_)
        return false;
      return true;
    },
  };

  ChannelChangePage.updateIsEnterpriseManaged = function(isEnterpriseManaged) {
    ChannelChangePage.getInstance().updateIsEnterpriseManaged_(
        isEnterpriseManaged);
  };

  ChannelChangePage.updateCurrentChannel = function(channel) {
    ChannelChangePage.getInstance().updateCurrentChannel_(channel);
  };

  ChannelChangePage.updateTargetChannel = function(channel) {
    ChannelChangePage.getInstance().updateTargetChannel_(channel);
  };

  ChannelChangePage.isPageReady = function() {
    return ChannelChangePage.getInstance().isPageReady_();
  };

  // Export
  return {
    ChannelChangePage: ChannelChangePage
  };
});
