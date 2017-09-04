// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

cr.define('extensions', function() {
  'use strict';

  var MAX_HEIGHT = 600;
  var MAX_WIDTH = 600;
  var MIN_HEIGHT = 300;
  var MIN_WIDTH = 300;
  var HEADER_PADDING = 40;

  var OptionsDialog = Polymer({
    is: 'extensions-options-dialog',
    properties: {
      /** @private {Object} */
      extensionOptions_: Object,

      /** @private {chrome.developerPrivate.ExtensionInfo} */
      data_: Object,
    },

    /** @param {chrome.developerPrivate.ExtensionInfo} data */
    show: function(data) {
      this.data_ = data;
      if (!this.extensionOptions_)
        this.extensionOptions_ = document.createElement('ExtensionOptions');
      this.extensionOptions_.extension = this.data_.id;
      this.extensionOptions_.onclose = this.close.bind(this);
      var bounded = function(min, max, val) {
        return Math.min(Math.max(min, val), max);
      };

      var onSizeChanged = function(e) {
        var minHeaderWidth =
            this.$['icon-and-name-wrapper'].offsetWidth +
            this.$['close-button'].offsetWidth +
            HEADER_PADDING;
        var minWidth = Math.max(minHeaderWidth, MIN_WIDTH);
        this.$.main.style.height =
            bounded(MIN_HEIGHT, MAX_HEIGHT, e.height) + 'px';
        this.$.main.style.width =
            bounded(minWidth, MAX_WIDTH, e.width) + 'px';
      }.bind(this);

      this.extensionOptions_.onpreferredsizechanged = onSizeChanged;
      this.$.main.appendChild(this.extensionOptions_);
      this.$$('dialog').showModal();
      onSizeChanged({height: 0, width: 0});
    },

    close: function() {
      this.$$('dialog').close();
    },
  });

  return {OptionsDialog: OptionsDialog};
});
