// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

/**
 * @fileoverview Earcons library that uses the HTML5 Audio element to play back
 * auditory cues.
 *
 */


goog.provide('cvox.EarconsBackground');

goog.require('cvox.AbstractEarcons');


/**
 * @constructor
 * @extends {cvox.AbstractEarcons}
 */
cvox.EarconsBackground = function() {
  goog.base(this);

  this.audioMap = new Object();
  if (localStorage['earcons'] === 'false') {
    this.enabled = false;
  }
};
goog.inherits(cvox.EarconsBackground, cvox.AbstractEarcons);


/**
 * @return {string} The human-readable name of the earcon set.
 */
cvox.EarconsBackground.prototype.getName = function() {
  return 'ChromeVox earcons';
};


/**
 * @return {string} The base URL for loading earcons.
 */
cvox.EarconsBackground.prototype.getBaseUrl = function() {
  return cvox.EarconsBackground.BASE_URL;
};


/**
 * @override
 */
cvox.EarconsBackground.prototype.playEarcon = function(earcon) {
  goog.base(this, 'playEarcon', earcon);
  if (!this.enabled) {
    return;
  }
  if (window['console']) {
    window['console']['log']('Earcon ' + this.getEarconName(earcon));
  }

  this.currentAudio = this.audioMap[earcon];
  if (!this.currentAudio) {
    this.currentAudio = new Audio(this.getBaseUrl() +
        this.getEarconFilename(earcon));
    this.audioMap[earcon] = this.currentAudio;
  }
  try {
    this.currentAudio.currentTime = 0;
  } catch (e) {
  }
  if (this.currentAudio.paused) {
    this.currentAudio.volume = 0.7;
    this.currentAudio.play();
  }
};


/**
 * The base URL for  loading eracons.
 * @type {string}
 */
cvox.EarconsBackground.BASE_URL = 'earcons/';
