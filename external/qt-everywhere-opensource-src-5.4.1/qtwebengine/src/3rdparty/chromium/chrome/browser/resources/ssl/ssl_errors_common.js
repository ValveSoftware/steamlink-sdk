// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Should match SSLBlockingPageCommands in ssl_blocking_page.cc.
var CMD_DONT_PROCEED = 0;
var CMD_PROCEED = 1;
var CMD_MORE = 2;
var CMD_RELOAD = 3;
var CMD_HELP = 4;

var keyPressState = 0;

function $(o) {
  return document.getElementById(o);
}

function sendCommand(cmd) {
  window.domAutomationController.setAutomationId(1);
  window.domAutomationController.send(cmd);
}

// This allows errors to be skippped by typing "danger" into the page.
function keyPressHandler(e) {
  var sequence = 'danger';
  if (sequence.charCodeAt(keyPressState) == e.keyCode) {
    keyPressState++;
    if (keyPressState == sequence.length) {
      sendCommand(CMD_PROCEED);
      keyPressState = 0;
    }
  } else {
    keyPressState = 0;
  }
}

function sharedSetup() {
  document.addEventListener('contextmenu', function(e) {
    e.preventDefault();
  });
  document.addEventListener('keypress', keyPressHandler);
}

document.addEventListener('DOMContentLoaded', sharedSetup);
