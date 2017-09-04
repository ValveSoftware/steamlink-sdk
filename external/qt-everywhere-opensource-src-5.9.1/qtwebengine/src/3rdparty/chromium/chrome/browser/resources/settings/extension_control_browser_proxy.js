// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

cr.define('settings', function() {
  /** @interface */
  function ExtensionControlBrowserProxy() {}

  ExtensionControlBrowserProxy.prototype = {
    // TODO(dbeam): should be be returning !Promise<boolean> to indicate whether
    // it succeeded?
    /** @param {string} extensionId */
    disableExtension: assertNotReached,

    /** @param {string} extensionId */
    manageExtension: assertNotReached,
  };

  /**
   * @implements {settings.ExtensionControlBrowserProxy}
   * @constructor
   */
  function ExtensionControlBrowserProxyImpl() {}
  cr.addSingletonGetter(ExtensionControlBrowserProxyImpl);

  ExtensionControlBrowserProxyImpl.prototype = {
    /** @override */
    disableExtension: function(extensionId) {
      chrome.send('disableExtension', [extensionId]);
    },

    /** @override */
    manageExtension: function(extensionId) {
      window.open('chrome://extensions?id=' + extensionId);
    },
  };

  return {
    ExtensionControlBrowserProxy: ExtensionControlBrowserProxy,
    ExtensionControlBrowserProxyImpl: ExtensionControlBrowserProxyImpl,
  };
});
