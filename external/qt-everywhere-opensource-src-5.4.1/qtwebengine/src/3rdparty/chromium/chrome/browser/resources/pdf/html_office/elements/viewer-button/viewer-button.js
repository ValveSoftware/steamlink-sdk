// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

(function() {
  var dpi = '';

  Polymer('viewer-button', {
    img: '',
    latchable: false,
    ready: function() {
      if (!dpi) {
        var mql = window.matchMedia('(-webkit-min-device-pixel-ratio: 1.3');
        dpi = mql.matches ? 'hi' : 'low';
      }
    },
    imgChanged: function() {
      if (this.img) {
        this.$.icon.style.backgroundImage =
            'url(' + this.getAttribute('assetpath') + 'img/' + dpi +
            'DPI/' + this.img + ')';
      } else {
        this.$.icon.style.backgroundImage = '';
      }
    },
    latchableChanged: function() {
      if (this.latchable)
        this.classList.add('latchable');
      else
        this.classList.remove('latchable');
    },
  });
})();
