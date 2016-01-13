// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

/**
 * @fileoverview Oobe update screen implementation.
 */

login.createScreen('UpdateScreen', 'update', function() {
  return {
    EXTERNAL_API: [
      'enableUpdateCancel',
      'setUpdateProgress',
      'showEstimatedTimeLeft',
      'setEstimatedTimeLeft',
      'showProgressMessage',
      'setProgressMessage',
      'setUpdateMessage',
      'showUpdateCurtain'
    ],

    /**
     * Header text of the screen.
     * @type {string}
     */
    get header() {
      return loadTimeData.getString('updateScreenTitle');
    },

    /**
     * Cancels the screen.
     */
    cancel: function() {
      // It's safe to act on the accelerator even if it's disabled on official
      // builds, since Chrome will just ignore the message in that case.
      var updateCancelHint = $('update-cancel-hint').firstElementChild;
      updateCancelHint.textContent =
          loadTimeData.getString('cancelledUpdateMessage');
      chrome.send('cancelUpdate');
    },

    /**
     * Makes 'press Escape to cancel update' hint visible.
     */
    enableUpdateCancel: function() {
      $('update-cancel-hint').hidden = false;
    },

    /**
     * Sets update's progress bar value.
     * @param {number} progress Percentage of the progress bar.
     */
    setUpdateProgress: function(progress) {
      $('update-progress-bar').value = progress;
    },

    /**
     * Shows or hides downloading ETA message.
     * @param {boolean} visible Are ETA message visible?
     */
    showEstimatedTimeLeft: function(visible) {
      $('progress-message').hidden = visible;
      $('estimated-time-left').hidden = !visible;
    },

    /**
     * Sets estimated time left until download will complete.
     * @param {number} seconds Time left in seconds.
     */
    setEstimatedTimeLeft: function(seconds) {
      var minutes = Math.ceil(seconds / 60);
      var message = '';
      if (minutes > 60) {
        message = loadTimeData.getString('downloadingTimeLeftLong');
      } else if (minutes > 55) {
        message = loadTimeData.getString('downloadingTimeLeftStatusOneHour');
      } else if (minutes > 20) {
        message = loadTimeData.getStringF('downloadingTimeLeftStatusMinutes',
                                          Math.ceil(minutes / 5) * 5);
      } else if (minutes > 1) {
        message = loadTimeData.getStringF('downloadingTimeLeftStatusMinutes',
                                          minutes);
      } else {
        message = loadTimeData.getString('downloadingTimeLeftSmall');
      }
      $('estimated-time-left').textContent =
        loadTimeData.getStringF('downloading', message);
    },

    /**
     * Shows or hides info message below progress bar.
     * @param {boolean} visible Are message visible?
     */
    showProgressMessage: function(visible) {
      $('estimated-time-left').hidden = visible;
      $('progress-message').hidden = !visible;
    },

    /**
     * Sets message below progress bar.
     * @param {string} message Message that should be shown.
     */
    setProgressMessage: function(message) {
      $('progress-message').innerText = message;
    },

    /**
     * Sets update message, which is shown above the progress bar.
     * @param {text} message Message which is shown by the label.
     */
    setUpdateMessage: function(message) {
      $('update-upper-label').textContent = message;
    },

    /**
     * Shows or hides update curtain.
     * @param {boolean} visible Are curtains visible?
     */
    showUpdateCurtain: function(visible) {
      $('update-screen-curtain').hidden = !visible;
      $('update-screen-main').hidden = visible;
    }
  };
});
