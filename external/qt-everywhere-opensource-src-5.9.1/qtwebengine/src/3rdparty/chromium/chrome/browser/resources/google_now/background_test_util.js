// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Mocks for globals needed for loading background.js.

var wrapper = {instrumentChromeApiFunction: emptyMock};

function buildAuthenticationManager() {
  return {
    addListener: emptyMock
  };
}

var instrumentApiFunction = emptyMock;
var buildTaskManager = emptyMock;
var buildAttemptManager = emptyMock;
var buildCardSet = emptyMock;

var instrumented = {};
mockChromeEvent(instrumented, 'gcm.onMessage');
mockChromeEvent(instrumented, 'notifications.onButtonClicked');
mockChromeEvent(instrumented, 'notifications.onClicked');
mockChromeEvent(instrumented, 'notifications.onClosed');
mockChromeEvent(instrumented, 'notifications.onPermissionLevelChanged');
mockChromeEvent(instrumented, 'notifications.onShowSettings');
mockChromeEvent(instrumented, 'runtime.onInstalled');
mockChromeEvent(instrumented, 'runtime.onStartup');
mockChromeEvent(instrumented, 'storage.onChanged');

NOTIFICATION_CARDS_URL = 'https://test/';
navigator = {language: 'en-US'};
