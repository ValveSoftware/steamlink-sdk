// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

/**
 * @fileoverview Device reset screen implementation.
 */

login.createScreen('ResetScreen', 'reset', function() {
  return {

    EXTERNAL_API: [
      'updateViewOnRollbackCall'
    ],

    /** @override */
    decorate: function() {
      $('reset-powerwash-help-link-on-rollback').addEventListener(
          'click', function(event) {
        chrome.send('resetOnLearnMore');
      });
      $('powerwash-help-link').addEventListener(
          'click', function(event) {
        chrome.send('resetOnLearnMore');
      });
    },

    /**
     * Header text of the screen.
     * @type {string}
     */
    get header() {
      return loadTimeData.getString('resetScreenTitle');
    },

    /**
     * Buttons in oobe wizard's button strip.
     * @type {array} Array of Buttons.
     */
    get buttons() {
      var buttons = [];
      var resetButton = this.ownerDocument.createElement('button');
      resetButton.id = 'reset-button';
      resetButton.textContent = '';
      resetButton.addEventListener('click', function(e) {
        if ($('reset').needRestart)
          chrome.send('restartOnReset', [$('reset-rollback-checkbox').checked]);
        else
          chrome.send('powerwashOnReset',
                      [$('reset-rollback-checkbox').checked]);
        e.stopPropagation();
      });
      buttons.push(resetButton);

      var cancelButton = this.ownerDocument.createElement('button');
      cancelButton.id = 'reset-cancel-button';
      cancelButton.textContent = loadTimeData.getString('cancelButton');
      cancelButton.addEventListener('click', function(e) {
        chrome.send('cancelOnReset');
        e.stopPropagation();
      });
      buttons.push(cancelButton);

      return buttons;
    },

    /**
     * Returns a control which should receive an initial focus.
     */
    get defaultControl() {
      return $('reset-button');
    },

    /**
     * Cancels the reset and drops the user back to the login screen.
     */
    cancel: function() {
      chrome.send('cancelOnReset');
    },

    /**
     * Event handler that is invoked just before the screen in shown.
     * @param {Object} data Screen init payload.
     */
    onBeforeShow: function(data) {
      if (data === undefined)
        return;
      this.classList.remove('revert-promise');
      if ('showRestartMsg' in data)
        this.setRestartMsg_(data['showRestartMsg']);
      if ('showRollbackOption' in data)
        this.setRollbackAvailable_(data['showRollbackOption']);
      if ('simpleConfirm' in data) {
        this.isConfirmational = data['simpleConfirm'];
        this.confirmRollback = false;
      }
      if ('rollbackConfirm' in data) {
        this.isConfirmational = data['rollbackConfirm'];
        this.confirmRollback = true;
      }

      if (this.isConfirmational) {
        // Exec after reboot initiated by reset screen.
        // Confirmational form of screen.
        $('reset-button').textContent = loadTimeData.getString(
            'resetButtonReset');
        if (this.confirmRollback) {
          $('reset-warning-msg').textContent = loadTimeData.getString(
              'resetAndRollbackWarningTextConfirmational');
          $('reset-warning-details').textContent = loadTimeData.getString(
              'resetAndRollbackWarningDetailsConfirmational');
        } else {
          $('reset-warning-msg').textContent = loadTimeData.getString(
              'resetWarningTextConfirmational');
          $('reset-warning-details').textContent = loadTimeData.getString(
              'resetWarningDetailsConfirmational');
        }
      } else {
        $('reset-warning-msg').textContent = loadTimeData.getString(
            'resetWarningTextInitial');
        $('reset-warning-details').textContent = loadTimeData.getString(
            'resetWarningDetailsInitial');
        if (this.needRestart) {
          $('reset-button').textContent = loadTimeData.getString(
              'resetButtonRelaunch');
        } else {
          $('reset-button').textContent = loadTimeData.getString(
              'resetButtonPowerwash');
        }
      }
    },

    /**
      * Sets restart necessity for the screen.
      * @param {bool} need_restart. If restart required before reset.
      * @private
      */
    setRestartMsg_: function(need_restart) {
      this.classList.toggle('norestart', !need_restart);
      this.needRestart = need_restart;
    },

    /**
      * Sets rollback availability for the screen.
      * @param {bool} can_rollback. If Rollback is available on reset screen.
      * @private
      */
    setRollbackAvailable_: function(show_rollback) {
      this.classList.toggle('norollback', !show_rollback);
      this.showRollback = show_rollback;
    },

    updateViewOnRollbackCall: function() {
      this.classList.add('revert-promise');
      announceAccessibleMessage(
          loadTimeData.getString('resetRevertSpinnerMessage'));
    }
  };
});
