// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

/**
 * @fileoverview Contains a factory interface for creating and opening gnubbies.
 */
'use strict';

/**
 * A factory for creating and opening gnubbies.
 * @interface
 */
function GnubbyFactory() {}

/**
 * Enumerates gnubbies.
 * @param {function(number, Array.<llGnubbyDeviceId>)} cb Enumerate callback
 */
GnubbyFactory.prototype.enumerate = function(cb) {
};

/**
 * Creates a new gnubby object, and opens the gnubby with the given index.
 * @param {llGnubbyDeviceId} which The device to open.
 * @param {boolean} forEnroll Whether this gnubby is being opened for enrolling.
 * @param {function(number, usbGnubby=)} cb Called with result of opening the
 *     gnubby.
 * @param {string=} logMsgUrl the url to post log messages to
 */
GnubbyFactory.prototype.openGnubby = function(which, forEnroll, cb, logMsgUrl) {
};
