// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

function sendCommand(cmd) {
  window.domAutomationController.setAutomationId(1);
  window.domAutomationController.send(cmd);
}

function initialize() {
  if (loadTimeData.getBoolean('allowAccessRequests')) {
    $('request-access-button').onclick = function(event) {
      updateAfterRequestSent();
      sendCommand('request');
    };
  } else {
    $('request-access-button').hidden = true;
  }
  $('back-button').onclick = function(event) {
    sendCommand('back');
  };
}

/**
 * Updates the interstitial to show that the request was sent.
 */
function updateAfterRequestSent() {
  $('error-img').hidden = true;
  $('request-access-button').hidden = true;
  $('block-page-message').hidden = true;
  $('request-sent-message').hidden = false;
}

document.addEventListener('DOMContentLoaded', initialize);
