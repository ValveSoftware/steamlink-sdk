// Copyright (c) 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

function onPageLoad() {
  var networkStatus = $('network-status');
  network.status.NetworkStatusList.decorate(networkStatus);

  networkStatus.setUserActionHandler(function(action) {
    if (action.command == 'openConfiguration') {
      var configUrl =
          chrome.extension.getURL('config.html?network=' + action.networkId);
      window.open(configUrl, 'network-config-frame');
    }
  });
}

document.addEventListener('DOMContentLoaded', onPageLoad);
