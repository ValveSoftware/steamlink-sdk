// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

/**
 * @fileoverview Base class for Braille engines that output to the Braille
 * display.
 *
 */

goog.provide('cvox.AbstractBraille');

goog.require('cvox.BrailleInterface');


/**
 * Creates a new instance.
 * @constructor
 * @implements {cvox.BrailleInterface}
 */
cvox.AbstractBraille = function() {
};


/** @override */
cvox.AbstractBraille.prototype.write = goog.abstractMethod;


/** @override */
cvox.AbstractBraille.prototype.setCommandListener = goog.abstractMethod;
