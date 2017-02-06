// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

var appWindow = chrome.app.window.current();

document.addEventListener('DOMContentLoaded', function() {
  chrome.hotwordPrivate.getLocalizedStrings(function(strings) {
    loadTimeData.data = strings;
    i18nTemplate.process(document, loadTimeData);

    var flow = new Flow();
    flow.startFlow();

    var pressFunction = function(e) {
      // Only respond to 'Enter' key presses.
      if (e.type == 'keyup' && e.key != 'Enter')
        return;

      var classes = e.target.classList;
      if (classes.contains('close') || classes.contains('finish-button')) {
        flow.stopTraining();
        appWindow.close();
        e.preventDefault();
      }
      if (classes.contains('retry-button')) {
        flow.handleRetry();
        e.preventDefault();
      }
    };

    $('steps').addEventListener('click', pressFunction);
    $('steps').addEventListener('keyup', pressFunction);

    $('audio-history-agree').addEventListener('click', function(e) {
      flow.enableAudioHistory();
      e.preventDefault();
    });

    $('hotword-start').addEventListener('click', function(e) {
      flow.advanceStep();
      e.preventDefault();
    });

    $('settings-link').addEventListener('click', function(e) {
      chrome.browser.openTab({'url': 'chrome://settings'}, function() {});
      e.preventDefault();
    });
  });
});
