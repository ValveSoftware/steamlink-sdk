// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

cr.define('print_preview', function() {
  'use strict';

  /**
   * Component that renders the copies settings UI.
   * @param {!print_preview.ticket_items.Copies} copiesTicketItem Used to read
   *     and write the copies value.
   * @param {!print_preview.ticket_items.Collate} collateTicketItem Used to read
   *     and write the collate value.
   * @constructor
   * @extends {print_preview.Component}
   */
  function CopiesSettings(copiesTicketItem, collateTicketItem) {
    print_preview.Component.call(this);

    /**
     * Used to read and write the copies value.
     * @type {!print_preview.ticket_items.Copies}
     * @private
     */
    this.copiesTicketItem_ = copiesTicketItem;

    /**
     * Used to read and write the collate value.
     * @type {!print_preview.ticket_items.Collate}
     * @private
     */
    this.collateTicketItem_ = collateTicketItem;

    /**
     * Timeout used to delay processing of the copies input.
     * @type {?number}
     * @private
     */
    this.textfieldTimeout_ = null;

    /**
     * Whether this component is enabled or not.
     * @type {boolean}
     * @private
     */
    this.isEnabled_ = true;
  };

  /**
   * Delay in milliseconds before processing the textfield.
   * @type {number}
   * @private
   */
  CopiesSettings.TEXTFIELD_DELAY_ = 250;

  CopiesSettings.prototype = {
    __proto__: print_preview.Component.prototype,

    /** @param {boolean} isEnabled Whether the copies settings is enabled. */
    set isEnabled(isEnabled) {
      this.getChildElement('input.copies').disabled = !isEnabled;
      this.getChildElement('input.collate').disabled = !isEnabled;
      this.isEnabled_ = isEnabled;
      if (isEnabled) {
        this.updateState_();
      } else {
        this.getChildElement('button.increment').disabled = true;
        this.getChildElement('button.decrement').disabled = true;
      }
    },

    /** @override */
    enterDocument: function() {
      print_preview.Component.prototype.enterDocument.call(this);
      fadeOutOption(this.getElement(), true);
      this.tracker.add(
          this.getChildElement('input.copies'),
          'keydown',
          this.onTextfieldKeyDown_.bind(this));
      this.tracker.add(
          this.getChildElement('input.copies'),
          'input',
          this.onTextfieldInput_.bind(this));
      this.tracker.add(
          this.getChildElement('input.copies'),
          'blur',
          this.onTextfieldBlur_.bind(this));
      this.tracker.add(
          this.getChildElement('button.increment'),
          'click',
          this.onButtonClicked_.bind(this, 1));
      this.tracker.add(
          this.getChildElement('button.decrement'),
          'click',
          this.onButtonClicked_.bind(this, -1));
      this.tracker.add(
          this.getChildElement('input.collate'),
          'click',
          this.onCollateCheckboxClick_.bind(this));
      this.tracker.add(
          this.copiesTicketItem_,
          print_preview.ticket_items.TicketItem.EventType.CHANGE,
          this.updateState_.bind(this));
      this.tracker.add(
          this.collateTicketItem_,
          print_preview.ticket_items.TicketItem.EventType.CHANGE,
          this.updateState_.bind(this));
    },

    /**
     * Updates the state of the copies settings UI controls.
     * @private
     */
    updateState_: function() {
      if (!this.copiesTicketItem_.isCapabilityAvailable()) {
        fadeOutOption(this.getElement());
        return;
      }

      if (this.getChildElement('input.copies').value !=
          this.copiesTicketItem_.getValue()) {
        this.getChildElement('input.copies').value =
            this.copiesTicketItem_.getValue();
      }

      var currentValueGreaterThan1 = false;
      if (this.copiesTicketItem_.isValid()) {
        this.getChildElement('input.copies').classList.remove('invalid');
        fadeOutElement(this.getChildElement('.hint'));
        this.getChildElement('.hint').setAttribute('aria-hidden', true);
        var currentValue = this.copiesTicketItem_.getValueAsNumber();
        var currentValueGreaterThan1 = currentValue > 1;
        this.getChildElement('button.increment').disabled =
            !this.isEnabled_ ||
            !this.copiesTicketItem_.wouldValueBeValid(currentValue + 1);
        this.getChildElement('button.decrement').disabled =
            !this.isEnabled_ ||
            !this.copiesTicketItem_.wouldValueBeValid(currentValue - 1);
      } else {
        this.getChildElement('input.copies').classList.add('invalid');
        this.getChildElement('.hint').setAttribute('aria-hidden', false);
        fadeInElement(this.getChildElement('.hint'));
        this.getChildElement('button.increment').disabled = true;
        this.getChildElement('button.decrement').disabled = true;
      }

      if (!(this.getChildElement('.collate-container').hidden =
             !this.collateTicketItem_.isCapabilityAvailable() ||
             !currentValueGreaterThan1)) {
        this.getChildElement('input.collate').checked =
            this.collateTicketItem_.getValue();
      }

      fadeInOption(this.getElement());
    },

    /**
     * Called whenever the increment/decrement buttons are clicked.
     * @param {number} delta Must be 1 for an increment button click and -1 for
     *     a decrement button click.
     * @private
     */
    onButtonClicked_: function(delta) {
      // Assumes text field has a valid number.
      var newValue =
          parseInt(this.getChildElement('input.copies').value) + delta;
      this.copiesTicketItem_.updateValue(newValue + '');
    },

    /**
     * Called after a timeout after user input into the textfield.
     * @private
     */
    onTextfieldTimeout_: function() {
      this.textfieldTimeout_ = null;
      var copiesVal = this.getChildElement('input.copies').value;
      if (copiesVal != '') {
        this.copiesTicketItem_.updateValue(copiesVal);
      }
    },

    /**
     * Called when a key is pressed on the custom input.
     * @param {Event} event Contains the key that was pressed.
     * @private
     */
    onTextfieldKeyDown_: function(event) {
      if (event.keyCode == 13 /*enter*/) {
        if (this.textfieldTimeout_) {
          clearTimeout(this.textfieldTimeout_);
        }
        this.onTextfieldTimeout_();
      }
    },

    /**
     * Called when a input event occurs on the textfield. Starts an input
     * timeout.
     * @private
     */
    onTextfieldInput_: function() {
      if (this.textfieldTimeout_) {
        clearTimeout(this.textfieldTimeout_);
      }
      this.textfieldTimeout_ = setTimeout(
          this.onTextfieldTimeout_.bind(this), CopiesSettings.TEXTFIELD_DELAY_);
    },

    /**
     * Called when the focus leaves the textfield. If the textfield is empty,
     * its value is set to 1.
     * @private
     */
    onTextfieldBlur_: function() {
      if (this.getChildElement('input.copies').value == '') {
        this.copiesTicketItem_.updateValue('1');
      }
    },

    /**
     * Called when the collate checkbox is clicked. Updates the print ticket.
     * @private
     */
    onCollateCheckboxClick_: function() {
      this.collateTicketItem_.updateValue(
          this.getChildElement('input.collate').checked);
    }
  };

  // Export
  return {
    CopiesSettings: CopiesSettings
  };
});
