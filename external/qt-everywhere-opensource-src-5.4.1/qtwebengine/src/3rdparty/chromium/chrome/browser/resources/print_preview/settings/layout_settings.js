// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

cr.define('print_preview', function() {
  'use strict';

  /**
   * Creates a LayoutSettings object. This object encapsulates all settings and
   * logic related to layout mode (portrait/landscape).
   * @param {!print_preview.ticket_items.Landscape} landscapeTicketItem Used to
   *     get the layout written to the print ticket.
   * @constructor
   * @extends {print_preview.Component}
   */
  function LayoutSettings(landscapeTicketItem) {
    print_preview.Component.call(this);

    /**
     * Used to get the layout written to the print ticket.
     * @type {!print_preview.ticket_items.Landscape}
     * @private
     */
    this.landscapeTicketItem_ = landscapeTicketItem;
  };

  /**
   * CSS classes used by the layout settings.
   * @enum {string}
   * @private
   */
  LayoutSettings.Classes_ = {
    LANDSCAPE_RADIO: 'layout-settings-landscape-radio',
    PORTRAIT_RADIO: 'layout-settings-portrait-radio'
  };

  LayoutSettings.prototype = {
    __proto__: print_preview.Component.prototype,

    /** @param {boolean} isEnabled Whether this component is enabled. */
    set isEnabled(isEnabled) {
      this.landscapeRadioButton_.disabled = !isEnabled;
      this.portraitRadioButton_.disabled = !isEnabled;
    },

    /** @override */
    enterDocument: function() {
      print_preview.Component.prototype.enterDocument.call(this);
      fadeOutOption(this.getElement(), true);
      this.tracker.add(
          this.portraitRadioButton_,
          'click',
          this.onLayoutButtonClick_.bind(this));
      this.tracker.add(
          this.landscapeRadioButton_,
          'click',
          this.onLayoutButtonClick_.bind(this));
      this.tracker.add(
          this.landscapeTicketItem_,
          print_preview.ticket_items.TicketItem.EventType.CHANGE,
          this.onLandscapeTicketItemChange_.bind(this));
    },

    /**
     * @return {HTMLInputElement} The portrait orientation radio button.
     * @private
     */
    get portraitRadioButton_() {
      return this.getElement().getElementsByClassName(
          LayoutSettings.Classes_.PORTRAIT_RADIO)[0];
    },

    /**
     * @return {HTMLInputElement} The landscape orientation radio button.
     * @private
     */
    get landscapeRadioButton_() {
      return this.getElement().getElementsByClassName(
          LayoutSettings.Classes_.LANDSCAPE_RADIO)[0];
    },

    /**
     * Called when one of the radio buttons is clicked. Updates the print ticket
     * store.
     * @private
     */
    onLayoutButtonClick_: function() {
      this.landscapeTicketItem_.updateValue(this.landscapeRadioButton_.checked);
    },

    /**
     * Called when the print ticket store changes state. Updates the state of
     * the radio buttons and hides the setting if necessary.
     * @private
     */
    onLandscapeTicketItemChange_: function() {
      if (this.landscapeTicketItem_.isCapabilityAvailable()) {
        var isLandscapeEnabled = this.landscapeTicketItem_.getValue();
        this.portraitRadioButton_.checked = !isLandscapeEnabled;
        this.landscapeRadioButton_.checked = isLandscapeEnabled;
        fadeInOption(this.getElement());
      } else {
        fadeOutOption(this.getElement());
      }
    }
  };

  // Export
  return {
    LayoutSettings: LayoutSettings
  };
});
