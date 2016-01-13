// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

cr.define('print_preview', function() {
  'use strict';

  /**
   * Creates a PageSettings object. This object encapsulates all settings and
   * logic related to page selection.
   * @param {!print_preview.ticket_items.PageRange} pageRangeTicketItem Used to
   *     read and write page range settings.
   * @constructor
   * @extends {print_preview.Component}
   */
  function PageSettings(pageRangeTicketItem) {
    print_preview.Component.call(this);

    /**
     * Used to read and write page range settings.
     * @type {!print_preview.ticket_items.PageRange}
     * @private
     */
    this.pageRangeTicketItem_ = pageRangeTicketItem;

    /**
     * Timeout used to delay processing of the custom page range input.
     * @type {?number}
     * @private
     */
    this.customInputTimeout_ = null;

    /**
     * Custom page range input.
     * @type {HTMLInputElement}
     * @private
     */
    this.customInput_ = null;

    /**
     * Custom page range radio button.
     * @type {HTMLInputElement}
     * @private
     */
    this.customRadio_ = null;

    /**
     * All page rage radio button.
     * @type {HTMLInputElement}
     * @private
     */
    this.allRadio_ = null;

    /**
     * Container of a hint to show when the custom page range is invalid.
     * @type {HTMLElement}
     * @private
     */
    this.customHintEl_ = null;
  };

  /**
   * CSS classes used by the page settings.
   * @enum {string}
   * @private
   */
  PageSettings.Classes_ = {
    ALL_RADIO: 'page-settings-all-radio',
    CUSTOM_HINT: 'page-settings-custom-hint',
    CUSTOM_INPUT: 'page-settings-custom-input',
    CUSTOM_RADIO: 'page-settings-custom-radio'
  };

  /**
   * Delay in milliseconds before processing custom page range input.
   * @type {number}
   * @private
   */
  PageSettings.CUSTOM_INPUT_DELAY_ = 500;

  PageSettings.prototype = {
    __proto__: print_preview.Component.prototype,

    set isEnabled(isEnabled) {
      this.customInput_.disabled = !isEnabled;
      this.allRadio_.disabled = !isEnabled;
      this.customRadio_.disabled = !isEnabled;
    },

    /** @override */
    enterDocument: function() {
      print_preview.Component.prototype.enterDocument.call(this);
      fadeOutOption(this.getElement(), true);
      this.tracker.add(
          this.allRadio_, 'click', this.onAllRadioClick_.bind(this));
      this.tracker.add(
          this.customRadio_, 'click', this.onCustomRadioClick_.bind(this));
      this.tracker.add(
          this.customInput_, 'blur', this.onCustomInputBlur_.bind(this));
      this.tracker.add(
          this.customInput_, 'focus', this.onCustomInputFocus_.bind(this));
      this.tracker.add(
          this.customInput_, 'keydown', this.onCustomInputKeyDown_.bind(this));
      this.tracker.add(
          this.customInput_, 'keyup', this.onCustomInputKeyUp_.bind(this));
      this.tracker.add(
          this.pageRangeTicketItem_,
          print_preview.ticket_items.TicketItem.EventType.CHANGE,
          this.onPageRangeTicketItemChange_.bind(this));
    },

    /** @override */
    exitDocument: function() {
      print_preview.Component.prototype.exitDocument.call(this);
      this.customInput_ = null;
      this.customRadio_ = null;
      this.allRadio_ = null;
      this.customHintEl_ = null;
    },

    /** @override */
    decorateInternal: function() {
      this.customInput_ = this.getElement().getElementsByClassName(
          PageSettings.Classes_.CUSTOM_INPUT)[0];
      this.allRadio_ = this.getElement().getElementsByClassName(
          PageSettings.Classes_.ALL_RADIO)[0];
      this.customRadio_ = this.getElement().getElementsByClassName(
          PageSettings.Classes_.CUSTOM_RADIO)[0];
      this.customHintEl_ = this.getElement().getElementsByClassName(
          PageSettings.Classes_.CUSTOM_HINT)[0];
      this.customHintEl_.textContent = localStrings.getStringF(
          'pageRangeInstruction',
          localStrings.getString('examplePageRangeText'));
    },

    /**
     * @param {boolean} Whether the custom hint is visible.
     * @private
     */
    setInvalidStateVisible_: function(isVisible) {
      if (isVisible) {
        this.customInput_.classList.add('invalid');
        this.customHintEl_.setAttribute('aria-hidden', 'false');
        fadeInElement(this.customHintEl_);
      } else {
        this.customInput_.classList.remove('invalid');
        fadeOutElement(this.customHintEl_);
        this.customHintEl_.setAttribute('aria-hidden', 'true');
      }
    },

    /**
     * Called when the all radio button is clicked. Updates the print ticket.
     * @private
     */
    onAllRadioClick_: function() {
      this.pageRangeTicketItem_.updateValue(null);
    },

    /**
     * Called when the custom radio button is clicked. Updates the print ticket.
     * @private
     */
    onCustomRadioClick_: function() {
      this.customInput_.focus();
    },

    /**
     * Called when the custom input is blurred. Enables the all radio button if
     * the custom input is empty.
     * @private
     */
    onCustomInputBlur_: function() {
      if (this.customInput_.value == '') {
        this.allRadio_.checked = true;
      }
    },

    /**
     * Called when the custom input is focused.
     * @private
     */
    onCustomInputFocus_: function() {
      this.customRadio_.checked = true;
      this.pageRangeTicketItem_.updateValue(this.customInput_.value);
    },

    /**
     * Called when a key is pressed on the custom input.
     * @param {Event} event Contains the key that was pressed.
     * @private
     */
    onCustomInputKeyDown_: function(event) {
      if (event.keyCode == 13 /*enter*/) {
        if (this.customInputTimeout_) {
          clearTimeout(this.customInputTimeout_);
          this.customInputTimeout_ = null;
        }
        this.pageRangeTicketItem_.updateValue(this.customInput_.value);
      }
    },

    /**
     * Called when a key is pressed on the custom input.
     * @param {Event} event Contains the key that was pressed.
     * @private
     */
    onCustomInputKeyUp_: function(event) {
      if (this.customInputTimeout_) {
        clearTimeout(this.customInputTimeout_);
        this.customInputTimeout_ = null;
      }
      if (event.keyCode != 13 /*enter*/) {
        this.customRadio_.checked = true;
        this.customInputTimeout_ = setTimeout(
            this.onCustomInputTimeout_.bind(this),
            PageSettings.CUSTOM_INPUT_DELAY_);
      }
    },

    /**
     * Called after a delay following a key press in the custom input.
     * @private
     */
    onCustomInputTimeout_: function() {
      this.customInputTimeout_ = null;
      if (this.customRadio_.checked) {
        this.pageRangeTicketItem_.updateValue(this.customInput_.value);
      }
    },

    /**
     * Called when the print ticket changes. Updates the state of the component.
     * @private
     */
    onPageRangeTicketItemChange_: function() {
      if (this.pageRangeTicketItem_.isCapabilityAvailable()) {
        var pageRangeStr = this.pageRangeTicketItem_.getValue();
        if (pageRangeStr || this.customRadio_.checked) {
          if (!document.hasFocus() ||
              document.activeElement != this.customInput_) {
            this.customInput_.value = pageRangeStr;
          }
          this.customRadio_.checked = true;
          this.setInvalidStateVisible_(!this.pageRangeTicketItem_.isValid());
        } else {
          this.allRadio_.checked = true;
          this.setInvalidStateVisible_(false);
        }
        fadeInOption(this.getElement());
      } else {
        fadeOutOption(this.getElement());
      }
    }
  };

  // Export
  return {
    PageSettings: PageSettings
  };
});
