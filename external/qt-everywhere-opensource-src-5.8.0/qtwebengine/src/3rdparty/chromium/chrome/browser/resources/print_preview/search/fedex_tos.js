// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

cr.define('print_preview', function() {
  'use strict';

  /**
   * Widget that renders a terms-of-service agreement for using the FedEx Office
   * print destination.
   * @constructor
   * @extends {print_preview.Component}
   */
  function FedexTos() {
    print_preview.Component.call(this);
  };

  /**
   * Enumeration of event types dispatched by the widget.
   * @enum {string}
   */
  FedexTos.EventType = {
    // Dispatched when the user agrees to the terms-of-service.
    AGREE: 'print_preview.FedexTos.AGREE'
  };

  FedexTos.prototype = {
    __proto__: print_preview.Component.prototype,

    /** @param {boolean} isVisible Whether the widget is visible. */
    setIsVisible: function(isVisible) {
      if (isVisible) {
        var heightHelperEl = this.getElement().querySelector('.height-helper');
        this.getElement().style.height = heightHelperEl.offsetHeight + 'px';
      } else {
        this.getElement().style.height = 0;
      }
    },

    /** @override */
    createDom: function() {
      this.setElementInternal(this.cloneTemplateInternal('fedex-tos-template'));
      var tosTextEl = this.getElement().querySelector('.tos-text');
      tosTextEl.innerHTML = loadTimeData.getStringF(
          'fedexTos',
          '<a href="http://www.fedex.com/us/office/copyprint/online/' +
              'googlecloudprint/termsandconditions">',
          '</a>');
    },

    /** @override */
    enterDocument: function() {
      var agreeCheckbox = this.getElement().querySelector('.agree-checkbox');
      this.tracker.add(
          agreeCheckbox, 'click', this.onAgreeCheckboxClick_.bind(this));
    },

    /**
     * Called when the agree checkbox is clicked. Dispatches a AGREE event.
     * @private
     */
    onAgreeCheckboxClick_: function() {
      cr.dispatchSimpleEvent(this, FedexTos.EventType.AGREE);
    }
  };

  // Export
  return {
    FedexTos: FedexTos
  };
});
