// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

/**
 * @fileoverview Defined the convenience function cvox.Msgs.getMsg.
 */

goog.provide('cvox.AbstractMsgs');



/**
 * @constructor
 */
cvox.AbstractMsgs = function() { };



/**
 * Return the current locale.
 * @return {string} The locale.
 */
cvox.AbstractMsgs.prototype.getLocale = function() { };


/**
 * Returns the message with the given message id.
 *
 * If we can't find a message, throw an exception.  This allows us to catch
 * typos early.
 *
 * @param {string} message_id The id.
 * @param {Array.<string>=} opt_subs Substitution strings.
 * @return {string} The message.
 */
cvox.AbstractMsgs.prototype.getMsg = function(message_id, opt_subs) {
};


/**
 * Retuns a number formatted correctly.
 *
 * @param {number} num The number.
 * @return {string} The number in the correct locale.
 */
cvox.AbstractMsgs.prototype.getNumber = function(num) {
};
