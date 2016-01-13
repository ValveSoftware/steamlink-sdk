// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

/**
 * @fileoverview Provides a "bottom half" helper to assist with raw enroll
 * requests.
 */
'use strict';

/**
 * A helper for enroll requests.
 * @extends {Closeable}
 * @interface
 */
function EnrollHelper() {}

/**
 * Attempts to enroll using the provided data.
 * @param {Array} enrollChallenges an array enroll challenges.
 * @param {Array.<SignHelperChallenge>} signChallenges a list of sign
 *     challenges for already enrolled gnubbies, to prevent double-enrolling a
 *     device.
 */
EnrollHelper.prototype.doEnroll =
    function(enrollChallenges, signChallenges) {};

/** Closes this helper. */
EnrollHelper.prototype.close = function() {};

/**
 * A factory for creating enroll helpers.
 * @interface
 */
function EnrollHelperFactory() {}

/**
 * Creates a new enroll helper.
 * @param {!Countdown} timer Timer after whose expiration the caller is no
 *     longer interested in the result of an enroll request.
 * @param {function(number, boolean)} errorCb Called when an enroll request
 *     fails with an error code and whether any gnubbies were found.
 * @param {function(string, string)} successCb Called with the result of a
 *     successful enroll request, along with the version of the gnubby that
 *     provided it.
 * @param {(function(number, boolean)|undefined)} opt_progressCb Called with
 *     progress updates to the enroll request.
 * @param {string=} opt_logMsgUrl A URL to post log messages to.
 * @return {EnrollHelper} the newly created helper.
 */
EnrollHelperFactory.prototype.createHelper =
    function(timer, errorCb, successCb, opt_progressCb, opt_logMsgUrl) {};
