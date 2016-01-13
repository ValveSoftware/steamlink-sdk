// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

chrome.runtime.onStartup.addListener(
    function() {
  chrome.alarms.create('hangout_services_fxpre', {'delayInMinutes': 2});
});

chrome.alarms.onAlarm.addListener(function(alarm) {
  if (!alarm || alarm.name != 'hangout_services_fxpre')
    return;

  var e = document.createElement('iframe');
  e.src = 'https://plus.google.com/hangouts/_/fxpre';
  document.body.appendChild(e);
});
