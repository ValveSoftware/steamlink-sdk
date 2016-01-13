// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

/**
 * @fileoverview A basic abstraction of messages.
 *
 */

goog.provide('cvox.SpokenMessage');

/**
 * @constructor
 */
cvox.SpokenMessage = function() {
  /** @type {?Number} */
  this.count = null;

  /** @type {Array} */
  this.id = null;

  /**
   * A message that has been already localized and should be sent to tts raw.
   * @type {?string}
   */
  this.raw = null;
};
