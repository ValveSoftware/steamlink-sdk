// Copyright (c) 2010 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Create the drop down arrow for the drop down buttons.
(function() {
  var ctx = document.getCSSCanvasContext('2d', 'drop-down-arrow', 9, 4);
  ctx.fillStyle = '#000';
  ctx.translate(1.5, .5);
  ctx.beginPath();
  ctx.moveTo(0, 0);
  ctx.lineTo(6, 0);
  ctx.lineTo(3, 3);
  ctx.closePath();
  ctx.fill();
  ctx.stroke();
})();
