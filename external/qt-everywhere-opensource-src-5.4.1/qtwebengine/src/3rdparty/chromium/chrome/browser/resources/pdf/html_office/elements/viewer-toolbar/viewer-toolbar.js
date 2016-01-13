// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

Polymer('viewer-toolbar', {
  fadingIn: false,
  timerId_: undefined,
  inInitialFadeIn_: false,
  ready: function() {
    this.mousemoveCallback = function(e) {
      var rect = this.getBoundingClientRect();
      if (e.clientX >= rect.left && e.clientX <= rect.right &&
          e.clientY >= rect.top && e.clientY <= rect.bottom) {
        this.fadingIn = true;
        // If we hover over the toolbar, cancel the initial fade in.
        if (this.inInitialFadeIn_)
          this.inInitialFadeIn_ = false;
      } else {
        // Initially we want to keep the toolbar up for a longer period.
        if (!this.inInitialFadeIn_)
          this.fadingIn = false;
      }
    }.bind(this);
  },
  attached: function() {
    this.parentNode.addEventListener('mousemove', this.mousemoveCallback);
  },
  detached: function() {
    this.parentNode.removeEventListener('mousemove', this.mousemoveCallback);
  },
  initialFadeIn: function() {
    this.inInitialFadeIn_ = true;
    this.fadeIn();
    this.fadeOutAfterDelay(6000);
  },
  fadingInChanged: function() {
    if (this.fadingIn) {
      this.fadeIn();
    } else {
      if (this.timerId_ === undefined)
        this.fadeOutAfterDelay(3000);
    }
  },
  fadeIn: function() {
    this.style.opacity = 1;
    clearTimeout(this.timerId_);
    this.timerId_ = undefined;
  },
  fadeOutAfterDelay: function(delay) {
    this.timerId_ = setTimeout(
      function() {
        this.style.opacity = 0;
        this.timerId_ = undefined;
        this.inInitialFadeIn_ = false;
      }.bind(this), delay);
  }
});
