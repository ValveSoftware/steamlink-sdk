// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

/**
 * @fileoverview
 * 'settings-setup-pin-dialog' is the settings page for choosing a PIN.
 *
 * Example:
 *
 * <settings-setup-pin-dialog set-modes="[[quickUnlockSetModes]]">
 * </settings-setup-pin-dialog>
 */

(function() {
'use strict';

/**
 * A list of the top-10 most commmonly used PINs. This list is taken from
 * www.datagenetics.com/blog/september32012/.
 * @const
 */
var WEAK_PINS = [
  '1234', '1111', '0000', '1212', '7777', '1004', '2000', '4444', '2222',
  '6969'
];

Polymer({
  is: 'settings-setup-pin-dialog',

  behaviors: [I18nBehavior],

  properties: {
    /**
     * The current PIN keyboard value.
     * @private
     */
    pinKeyboardValue_: String,

    /**
     * Stores the initial PIN value so it can be confirmed.
     * @private
     */
    initialPin_: String,

    /**
     * The actual problem message to display.
     * @private
     */
    problemMessage_: String,

    /**
     * The type of problem class to show (warning or error).
     */
    problemClass_: String,

    /**
     * Should the step-specific submit button be displayed?
     * @private
     */
    enableSubmit_: Boolean,

    /**
     * The current step/subpage we are on.
     * @private
     */
    isConfirmStep_: {
      type: Boolean,
      value: false
    },
  },

  /** @override */
  attached: function() {
    this.resetState_();
  },

  open: function() {
    this.$.dialog.showModal();
    this.$.pinKeyboard.focus();
  },

  close: function() {
    if (this.$.dialog.open)
      this.$.dialog.close();

    this.resetState_();
  },

  /**
   * Resets the element to the initial state.
   * @private
   */
  resetState_: function() {
    this.initialPin_ = '';
    this.pinKeyboardValue_ = '';
    this.enableSubmit_ = false;
    this.isConfirmStep_ = false;
    this.onPinChange_();
  },

  /** @private */
  onCancelTap_: function() {
    this.resetState_();
    this.$.dialog.close();
  },

  /**
   * Returns true if the given PIN is likely easy to guess.
   * @private
   * @param {string} pin
   * @return {boolean}
   */
  isPinWeak_: function(pin) {
    // Warn if it's a top-10 pin.
    if (WEAK_PINS.includes(pin))
      return true;

    // Warn if the PIN is consecutive digits.
    var delta = 0;
    for (var i = 1; i < pin.length; ++i) {
      var prev = Number(pin[i - 1]);
      var num = Number(pin[i]);
      if (Number.isNaN(prev) || Number.isNaN(num))
        return false;
      delta = Math.max(delta, Math.abs(num - prev));
    }

    return delta <= 1;
  },

  /**
   * Returns true if the given PIN matches PIN requirements, such as minimum
   * length.
   * @private
   * @param {string|undefined} pin
   * @return {boolean}
   */
  isPinLongEnough_: function(pin) {
    return !!pin && pin.length >= 4;
  },

  /**
   * Returns true if the currently entered PIN is the same as the initially
   * submitted PIN.
   * @private
   * @return {boolean}
   */
  isPinConfirmed_: function() {
    return this.isPinLongEnough_(this.pinKeyboardValue_) &&
           this.initialPin_ == this.pinKeyboardValue_;
  },

  /**
   * Notify the user about a problem.
   * @private
   * @param {string} messageId
   * @param {string} problemClass
   */
  showProblem_: function(messageId, problemClass) {
    var previousMessage = this.problemMessage_;

    // Update problem info.
    this.problemMessage_ = this.i18n(messageId);
    this.problemClass_ = problemClass;
    this.updateStyles();

    // If the problem message has changed, fire an alert.
    if (previousMessage != this.problemMessage_)
      this.$.problemDiv.setAttribute('role', 'alert');
   },

  /** @private */
  hideProblem_: function() {
    this.problemMessage_ = '';
    this.problemClass_ = '';
  },

  /** @private */
  onPinChange_: function() {
    if (!this.isConfirmStep_) {
      var isPinLongEnough = this.isPinLongEnough_(this.pinKeyboardValue_);
      var isWeak = isPinLongEnough && this.isPinWeak_(this.pinKeyboardValue_);

      if (!isPinLongEnough)
        this.showProblem_('configurePinTooShort', 'warning');
      else if (isWeak)
        this.showProblem_('configurePinWeakPin', 'warning');
      else
        this.hideProblem_();

      this.enableSubmit_ = isPinLongEnough;

    } else {
      if (this.isPinConfirmed_())
        this.hideProblem_();
      else
        this.showProblem_('configurePinMismatched', 'warning');

      this.enableSubmit_ = true;
    }
  },

  /** @private */
  onPinSubmit_: function() {
    if (!this.isConfirmStep_) {
      if (this.isPinLongEnough_(this.pinKeyboardValue_)) {
        this.initialPin_ = this.pinKeyboardValue_;
        this.pinKeyboardValue_ = '';
        this.isConfirmStep_ = true;
        this.onPinChange_();
        this.$.pinKeyboard.focus();
      }
    } else {
      // onPinSubmit_ gets called if the user hits enter on the PIN keyboard.
      // The PIN is not guaranteed to be valid in that case.
      if (!this.isPinConfirmed_()) {
        this.showProblem_('configurePinMismatched', 'error');
        return;
      }

      function onSetModesCompleted(didSet) {
        if (!didSet) {
          console.error('Failed to update pin');
          return;
        }

        this.resetState_();
        this.fire('done');
      }

      this.setModes.call(
        null,
        [chrome.quickUnlockPrivate.QuickUnlockMode.PIN],
        [this.pinKeyboardValue_],
        onSetModesCompleted.bind(this));
    }
  },

  /**
   * @private
   * @param {string} problemMessage
   * @return {boolean}
   */
  hasProblem_: function(problemMessage) {
    return !!problemMessage;
  },

  /**
   * @private
   * @param {boolean} isConfirmStep
   * @return {string}
   */
  getTitleMessage_: function(isConfirmStep) {
    return this.i18n(isConfirmStep ? 'configurePinConfirmPinTitle' :
                                     'configurePinChoosePinTitle');
  },

  /**
   * @private
   * @param {boolean} isConfirmStep
   * @return {string}
   */
  getContinueMessage_: function(isConfirmStep) {
    return this.i18n(isConfirmStep ? 'confirm' : 'configurePinContinueButton');
  },
});

})();
