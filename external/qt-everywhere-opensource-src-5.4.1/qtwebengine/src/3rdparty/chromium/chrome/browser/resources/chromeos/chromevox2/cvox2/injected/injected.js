// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

/**
 * @fileoverview The entry point for all ChromeVox2 related code for the
 * injected content script.
 * This script will temporarily forward any events we need during the
 * ChromeVox/ChromeVox2 timeframe where extension APIs do not exist (e.g.
 * keyboard events).
 */

goog.provide('cvox2.Injected');

var port = chrome.extension.connect({name: 'chromevox2'});
document.body.addEventListener('keydown', function(evt) {
  port.postMessage({keydown: evt.keyCode});
}, true);
