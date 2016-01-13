// Copyright (c) 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

function showMessage(msg) {
  var area = $('message-area');
  var entry = document.createElement('div');
  entry.textContent = msg;
  area.appendChild(entry);
  window.setTimeout(function() {
      area.removeChild(entry);
    }, 3000);
}

function getShowMessageCallback(message) {
  return function() {
    var error = chrome.runtime.lastError;
    if (error) {
      showMessage(message + ': ' + error.message);
    } else {
      showMessage(message + ': Success!');
    }
  };
}

function onPageLoad() {
  var networkConfig = $('network-config');
  network.config.NetworkConfig.decorate(networkConfig);

  $('save').onclick = function() {
    chrome.networkingPrivate.setProperties(
        networkConfig.networkId,
        networkConfig.userSettings,
        getShowMessageCallback('Set properties of ' + networkConfig.networkId));
  };

  $('connect').onclick = function() {
    chrome.networkingPrivate.startConnect(
        networkConfig.networkId,
        getShowMessageCallback(
            'Requested connect to ' + networkConfig.networkId));
  };

  $('disconnect').onclick = function() {
    chrome.networkingPrivate.startDisconnect(
        networkConfig.networkId,
        getShowMessageCallback(
            'Requested disconnect from ' + networkConfig.networkId));
  };
}

document.addEventListener('DOMContentLoaded', onPageLoad);
