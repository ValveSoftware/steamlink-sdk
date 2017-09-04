// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

/** @fileoverview Handles interprocess communcation for the privacy page. */

/** @typedef {{enabled: boolean, managed: boolean}} */
var MetricsReporting;

cr.define('settings', function() {
  /** @interface */
  function PrivacyPageBrowserProxy() {}

  PrivacyPageBrowserProxy.prototype = {
<if expr="_google_chrome and not chromeos">
    /** @return {!Promise<!MetricsReporting>} */
    getMetricsReporting: assertNotReached,

    /** @param {boolean} enabled */
    setMetricsReportingEnabled: assertNotReached,
</if>

<if expr="is_win or is_macosx">
    /** Invokes the native certificate manager (used by win and mac). */
    showManageSSLCertificates: function() {},
</if>

    /** @return {!Promise<boolean>} */
    getSafeBrowsingExtendedReporting: assertNotReached,

    /** @param {boolean} enabled */
    setSafeBrowsingExtendedReportingEnabled: assertNotReached,
  };

  /**
   * @constructor
   * @implements {settings.PrivacyPageBrowserProxy}
   */
  function PrivacyPageBrowserProxyImpl() {}
  cr.addSingletonGetter(PrivacyPageBrowserProxyImpl);

  PrivacyPageBrowserProxyImpl.prototype = {
<if expr="_google_chrome and not chromeos">
    /** @override */
    getMetricsReporting: function() {
      return cr.sendWithPromise('getMetricsReporting');
    },

    /** @override */
    setMetricsReportingEnabled: function(enabled) {
      chrome.send('setMetricsReportingEnabled', [enabled]);
    },
</if>

    /** @override */
    getSafeBrowsingExtendedReporting: function() {
      return cr.sendWithPromise('getSafeBrowsingExtendedReporting');
    },

    /** @override */
    setSafeBrowsingExtendedReportingEnabled: function(enabled) {
      chrome.send('setSafeBrowsingExtendedReportingEnabled', [enabled]);
    },

<if expr="is_win or is_macosx">
    /** @override */
    showManageSSLCertificates: function() {
      chrome.send('showManageSSLCertificates');
    },
</if>
  };

  return {
    PrivacyPageBrowserProxy: PrivacyPageBrowserProxy,
    PrivacyPageBrowserProxyImpl: PrivacyPageBrowserProxyImpl,
  };
});
