// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

var appId = 'hotword_audio_verification';

chrome.app.runtime.onLaunched.addListener(function() {
  // We need to focus the window if it already exists, since it
  // is created as 'hidden'.
  //
  // Note: If we ever launch on another platform, make sure that this works
  // with window managers that support hiding (e.g. Cmd+h on an app window on
  // Mac).
  var appWindow = chrome.app.window.get(appId);
  if (appWindow) {
    appWindow.focus();
    return;
  }

  chrome.app.window.create('main.html', {
    'frame': 'none',
    'resizable': false,
    'hidden': true,
    'id': appId,
    'innerBounds': {
      'width': 784,
      'height': 448
    }
  });
});
