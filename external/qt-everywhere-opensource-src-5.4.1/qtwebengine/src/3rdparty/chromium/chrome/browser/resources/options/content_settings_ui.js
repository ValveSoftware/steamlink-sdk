// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

cr.define('options', function() {

  //////////////////////////////////////////////////////////////////////////////
  // ContentSettingsRadio class:

  // Define a constructor that uses an input element as its underlying element.
  var ContentSettingsRadio = cr.ui.define('input');

  ContentSettingsRadio.prototype = {
    __proto__: HTMLInputElement.prototype,

    /**
     * Initialization function for the cr.ui framework.
     */
    decorate: function() {
      this.type = 'radio';
      var self = this;

      this.addEventListener('change',
          function(e) {
            chrome.send('setContentFilter', [this.name, this.value]);
          });
    },
  };

  /**
   * Whether the content setting is controlled by something else than the user's
   * settings (either 'policy' or 'extension').
   * @type {string}
   */
  cr.defineProperty(ContentSettingsRadio, 'controlledBy', cr.PropertyKind.ATTR);

  //////////////////////////////////////////////////////////////////////////////
  // HandlersEnabledRadio class:

  // Define a constructor that uses an input element as its underlying element.
  var HandlersEnabledRadio = cr.ui.define('input');

  HandlersEnabledRadio.prototype = {
    __proto__: HTMLInputElement.prototype,

    /**
     * Initialization function for the cr.ui framework.
     */
    decorate: function() {
      this.type = 'radio';
      var self = this;

      this.addEventListener('change',
          function(e) {
            chrome.send('setHandlersEnabled', [this.value == 'allow']);
          });
    },
  };

  // Export
  return {
    ContentSettingsRadio: ContentSettingsRadio,
    HandlersEnabledRadio: HandlersEnabledRadio
  };

});

