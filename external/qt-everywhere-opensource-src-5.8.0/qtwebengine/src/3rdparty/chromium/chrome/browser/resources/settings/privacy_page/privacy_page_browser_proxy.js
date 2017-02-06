// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

/** @fileoverview Handles interprocess communcation for the privacy page. */

cr.define('settings', function() {
  /** @interface */
  function PrivacyPageBrowserProxy() {}

  PrivacyPageBrowserProxy.prototype = {
    /** Invokes the native certificate manager (used by win and mac). */
    showManageSSLCertificates: function() {},
  };

  /**
   * @constructor
   * @implements {settings.PrivacyPageBrowserProxy}
   */
  function PrivacyPageBrowserProxyImpl() {}
  cr.addSingletonGetter(PrivacyPageBrowserProxyImpl);

  PrivacyPageBrowserProxyImpl.prototype = {
    /** @override */
    showManageSSLCertificates: function() {
      chrome.send('showManageSSLCertificates');
    },
  };

  return {
    PrivacyPageBrowserProxy: PrivacyPageBrowserProxy,
    PrivacyPageBrowserProxyImpl: PrivacyPageBrowserProxyImpl,
  };
});
