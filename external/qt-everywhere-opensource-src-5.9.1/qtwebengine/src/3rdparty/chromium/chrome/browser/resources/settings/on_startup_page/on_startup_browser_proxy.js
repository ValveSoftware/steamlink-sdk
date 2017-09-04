// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

/** @typedef {{id: string, name: string, canBeDisabled: boolean}} */
var NtpExtension;

cr.define('settings', function() {
  /** @interface */
  function OnStartupBrowserProxy() {}

  OnStartupBrowserProxy.prototype = {
    /** @return {!Promise<?NtpExtension>} */
    getNtpExtension: assertNotReached,
  };

  /**
   * @constructor
   * @implements {settings.OnStartupBrowserProxy}
   */
  function OnStartupBrowserProxyImpl() {}
  cr.addSingletonGetter(OnStartupBrowserProxyImpl);

  OnStartupBrowserProxyImpl.prototype = {
    /** @override */
    getNtpExtension: function() {
      return cr.sendWithPromise('getNtpExtension');
    },
  };

  return {
    OnStartupBrowserProxy: OnStartupBrowserProxy,
    OnStartupBrowserProxyImpl: OnStartupBrowserProxyImpl,
  };
});
