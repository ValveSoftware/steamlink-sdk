// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

define([
  "console",
  "gin/test/expect",
  "monotonic_clock",
  "timer",
  "mojo/apps/js/bindings/threading"
], function(console, expect, monotonicClock, timer, threading) {
  var global = this;
  var then = monotonicClock.seconds();
  var t = timer.createOneShot(100, function() {
    var now = monotonicClock.seconds();
    expect(now).toBeGreaterThan(then);
    global.result = "PASS";
    threading.quit();
  });
});
