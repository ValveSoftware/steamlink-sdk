// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.


/**
 * @fileoverview Testing stub for messages.
 */

goog.provide('cvox.TestMsgs');

goog.require('cvox.AbstractMsgs');
goog.require('cvox.HostFactory');
goog.require('cvox.TestMessages');



/**
 * @constructor
 * @extends {cvox.AbstractMsgs}
 */
cvox.TestMsgs = function() {
  cvox.AbstractMsgs.call(this);
};
goog.inherits(cvox.TestMsgs, cvox.AbstractMsgs);


/**
 * Return the current locale.
 * @return {string} The locale.
 */
cvox.TestMsgs.prototype.getLocale = function() {
  return 'testing';
};


/**
 * Returns the message with the given message id from the ChromeVox namespace.
 *
 * @param {string} message_id The id.
 * @param {Array.<string>} opt_subs Substitution strings.
 * @return {string} The message.
 */
cvox.TestMsgs.prototype.getMsg = function(message_id, opt_subs) {
  if (!message_id) {
    var e = new Error();
    e.message = 'Message id required';
    throw e;
  }
  var message = cvox.TestMessages['chromevox_' + message_id];
  if (message == undefined) {
    var e = new Error();
    e.message = 'missing-msg: ' + message_id;
    throw e;
  }

  var messageString = message.message;
  if (opt_subs) {
    // Unshift a null to make opt_subs and message.placeholders line up.
    for (var i = 0; i < opt_subs.length; i++) {
      var placeholderObject = message.placeholders ?
          message.placeholders[i + 1] : undefined;
      if (!placeholderObject) {
        // Allow tests to substitute the string 'dummy' if they don't know
        // how many substitutions a message has.
        if (opt_subs[i] == 'dummy') {
          continue;
        }
        var e = new Error();
        e.message = 'Bad placeholder ' + i + ' for message id ' + message_id;
        throw e;
      }
      var placeholder = message.placeholders[i + 1].content;
      messageString = messageString.replace(placeholder, opt_subs[i]);
    }
  }
  return messageString;
};


/**
 * Retuns a number formatted correctly.
 *
 * @param {number} num The number.
 * @return {string} The number in the correct locale.
 */
cvox.TestMsgs.prototype.getNumber = function(num) {
  return '' + num;
};

/**
 * Cosntructor for the host factory.
 */
cvox.HostFactory.msgsConstructor = cvox.TestMsgs;
