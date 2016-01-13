// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

cr.define('print_preview', function() {
  'use strict';

  /**
   * Creates a ColorSettings object. This object encapsulates all settings and
   * logic related to color selection (color/bw).
   * @param {!print_preview.ticket_item.Color} colorTicketItem Used for writing
   *     and reading color value.
   * @constructor
   * @extends {print_preview.Component}
   */
  function ColorSettings(colorTicketItem) {
    print_preview.Component.call(this);

    /**
     * Used for reading/writing the color value.
     * @type {!print_preview.ticket_items.Color}
     * @private
     */
    this.colorTicketItem_ = colorTicketItem;
  };

  ColorSettings.prototype = {
    __proto__: print_preview.Component.prototype,

    set isEnabled(isEnabled) {
      this.getChildElement('.color-option').disabled = !isEnabled;
      this.getChildElement('.bw-option').disabled = !isEnabled;
    },

    /** @override */
    enterDocument: function() {
      print_preview.Component.prototype.enterDocument.call(this);
      fadeOutOption(this.getElement(), true);
      this.tracker.add(
          this.getChildElement('.color-option'),
          'click',
          this.colorTicketItem_.updateValue.bind(this.colorTicketItem_, true));
      this.tracker.add(
          this.getChildElement('.bw-option'),
          'click',
          this.colorTicketItem_.updateValue.bind(this.colorTicketItem_, false));
      this.tracker.add(
          this.colorTicketItem_,
          print_preview.ticket_items.TicketItem.EventType.CHANGE,
          this.updateState_.bind(this));
    },

    /**
     * Updates state of the widget.
     * @private
     */
    updateState_: function() {
      var isColorCapAvailable = this.colorTicketItem_.isCapabilityAvailable();
      if (isColorCapAvailable) {
        fadeInOption(this.getElement());
        var isColorEnabled = this.colorTicketItem_.getValue();
        this.getChildElement('.color-option').checked = isColorEnabled;
        this.getChildElement('.bw-option').checked = !isColorEnabled;
      } else {
        fadeOutOption(this.getElement());
      }
      this.getElement().setAttribute('aria-hidden', !isColorCapAvailable);
    }
  };

  // Export
  return {
    ColorSettings: ColorSettings
  };
});
