// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

/**
 * @fileoverview Provides a "bottom half" helper to assist with raw sign
 * requests.
 */
'use strict';

/**
 * A helper for sign requests.
 * @extends {Closeable}
 * @interface
 */
function SignHelper() {}

/**
 * Attempts to sign the provided challenges.
 * @param {Array.<SignHelperChallenge>} challenges the new challenges to sign.
 * @return {boolean} whether the challenges were successfully added.
 */
SignHelper.prototype.doSign = function(challenges) {};

/** Closes this helper. */
SignHelper.prototype.close = function() {};

/**
 * A factory for creating sign helpers.
 * @interface
 */
function SignHelperFactory() {}

/**
 * Creates a new sign helper.
 * @param {Countdown} timer Timer after whose expiration the caller is no longer
 *     interested in the result of a sign request.
 * @param {function(number, boolean)} errorCb Called when a sign request fails
 *     with an error code and whether any gnubbies were found.
 * @param {function(SignHelperChallenge, string, string=)} successCb Called with
 *     the signature produced by a successful sign request.
 * @param {string=} opt_logMsgUrl A URL to post log messages to.
 * @return {SignHelper} The newly created helper.
 */
SignHelperFactory.prototype.createHelper =
    function(timer, errorCb, successCb, opt_logMsgUrl) {};
