// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

Polymer('viewer-progress-bar', {
  progress: 0,
  text: 'Loading',
  numSegments: 8,
  segments: [],
  ready: function() {
    this.numSegmentsChanged();
  },
  progressChanged: function() {
    var numVisible = this.progress * this.segments.length / 100.0;
    for (var i = 0; i < this.segments.length; i++) {
      this.segments[i].style.visibility =
          i < numVisible ? 'visible' : 'hidden';
    }

    if (this.progress >= 100 || this.progress < 0)
      this.style.opacity = 0;
  },
  numSegmentsChanged: function() {
    // Clear the existing segments.
    this.segments = [];
    var segmentsElement = this.$.segments;
    segmentsElement.innerHTML = '';

    // Create the new segments.
    var segment = document.createElement('li');
    segment.classList.add('segment');
    var angle = 360 / this.numSegments;
    for (var i = 0; i < this.numSegments; ++i) {
      var segmentCopy = segment.cloneNode(true);
      segmentCopy.style.webkitTransform =
          'rotate(' + (i * angle) + 'deg) skewY(' +
          -1 * (90 - angle) + 'deg)';
      segmentsElement.appendChild(segmentCopy);
      this.segments.push(segmentCopy);
    }
    this.progressChanged();
  }
});
