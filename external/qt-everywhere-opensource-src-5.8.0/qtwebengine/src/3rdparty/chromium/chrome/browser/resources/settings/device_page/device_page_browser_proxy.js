// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

/** @fileoverview A helper object used for testing the Device page. */
cr.define('settings', function() {
  /** @interface */
  function DevicePageBrowserProxy() {}

  DevicePageBrowserProxy.prototype = {
    /**
     * Override to interact with the on-tap/on-keydown event on the Learn More
     * link.
     * @param {!Event} e
     */
    handleLinkEvent: function(e) {},

    /** Initializes the keyboard WebUI handler. */
    initializeKeyboard: function() {},

    /** Shows the Ash keyboard shortcuts overlay. */
    showKeyboardShortcutsOverlay: function() {},
  };

  /**
   * @constructor
   * @implements {settings.DevicePageBrowserProxy}
   */
  function DevicePageBrowserProxyImpl() {}
  cr.addSingletonGetter(DevicePageBrowserProxyImpl);

  DevicePageBrowserProxyImpl.prototype = {
    /** override */
    handleLinkEvent: function(e) {
      // Prevent the link from activating its parent element when tapped or
      // when Enter is pressed.
      if (e.type != 'keydown' || e.keyCode == 13)
        e.stopPropagation();
    },

    /** @override */
    initializeKeyboard: function() {
      chrome.send('initializeKeyboardSettings');
    },

    /** @override */
    showKeyboardShortcutsOverlay: function() {
      chrome.send('showKeyboardShortcutsOverlay');
    },
  };

  return {
    DevicePageBrowserProxy: DevicePageBrowserProxy,
    DevicePageBrowserProxyImpl: DevicePageBrowserProxyImpl,
  };
});
