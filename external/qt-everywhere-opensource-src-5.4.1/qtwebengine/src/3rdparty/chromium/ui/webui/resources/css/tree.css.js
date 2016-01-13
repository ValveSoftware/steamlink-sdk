// Copyright (c) 2010 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

(function() {
  /** @const */ var WIDTH = 14;
  /** @const */ var HEIGHT = WIDTH / 2 + 2;
  /** @const */ var MARGIN = 1;
  var ctx = document.getCSSCanvasContext('2d',
                                         'tree-triangle',
                                         WIDTH + MARGIN * 2,
                                         HEIGHT + MARGIN * 2);

  ctx.fillStyle = '#7a7a7a';
  ctx.translate(MARGIN, MARGIN);

  ctx.beginPath();
  ctx.moveTo(0, 0);
  ctx.lineTo(0, 2);
  ctx.lineTo(WIDTH / 2, HEIGHT);
  ctx.lineTo(WIDTH, 2);
  ctx.lineTo(WIDTH, 0);
  ctx.closePath();
  ctx.fill();
  ctx.stroke();
})();
