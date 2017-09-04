// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

/**
 * @fileoverview Settings that affect how Chrome interacts with the underlying
 * operating system (i.e. network, background processes, hardware).
 */

Polymer({
  is: 'settings-system-page',

  properties: {
    prefs: {
      type: Object,
      notify: true,
    },
  },

  /**
   * @param {boolean} enabled Whether hardware acceleration is currently
   *     enabled.
   * @private
   */
  shouldShowRestart_: function(enabled) {
    var proxy = settings.SystemPageBrowserProxyImpl.getInstance();
    return enabled != proxy.wasHardwareAccelerationEnabledAtStartup();
  },

  /** @private */
  onChangeProxySettingsTap_: function() {
    settings.SystemPageBrowserProxyImpl.getInstance().changeProxySettings();
  },

  /** @private */
  onExtensionDisable_: function() {
    // TODO(dbeam): this is a pretty huge bummer. It means there are things
    // (inputs) that our prefs system is not observing. And that changes from
    // other sources (i.e. disabling/enabling an extension from
    // chrome://extensions or from the omnibox directly) will not update
    // |this.prefs.proxy| directly (nor the UI). We should fix this eventually.
    this.fire('refresh-pref', 'proxy');
  },

  /** @private */
  onRestartTap_: function() {
    // TODO(dbeam): we should prompt before restarting the browser.
    settings.LifetimeBrowserProxyImpl.getInstance().restart();
  },
});
