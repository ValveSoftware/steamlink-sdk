// Copyright 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

chrome.app.runtime.onLaunched.addListener(function() {
  chrome.app.window.create(
      'chrome://settings-frame/options_settings_app.html',
      {'id': 'settings_app', 'height': 550, 'width': 750});
});
