// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.


/**
 * @fileoverview A cvox.AbstractMsgs implementation for Chrome.
 */

goog.provide('cvox.ChromeMsgs');

goog.require('cvox.AbstractMsgs');
goog.require('cvox.HostFactory');



/**
 * @constructor
 * @extends {cvox.AbstractMsgs}
 */
cvox.ChromeMsgs = function() {
  cvox.AbstractMsgs.call(this);
};
goog.inherits(cvox.ChromeMsgs, cvox.AbstractMsgs);


/**
 * The namespace for all Chromevox messages.
 * @type {string}
 * @const
 * @private
 */
cvox.ChromeMsgs.NAMESPACE_ = 'chromevox_';


/**
 * Return the current locale.
 * @return {string} The locale.
 */
cvox.ChromeMsgs.prototype.getLocale = function() {
  return chrome.i18n.getMessage('locale');
};


/**
 * Returns the message with the given message id from the ChromeVox namespace.
 *
 * If we can't find a message, throw an exception.  This allows us to catch
 * typos early.
 *
 * @param {string} message_id The id.
 * @param {Array.<string>=} opt_subs Substitution strings.
 * @return {string} The message.
 */
cvox.ChromeMsgs.prototype.getMsg = function(message_id, opt_subs) {
  var message = chrome.i18n.getMessage(
      cvox.ChromeMsgs.NAMESPACE_ + message_id, opt_subs);
  if (message == undefined || message == '') {
    throw new Error('Invalid ChromeVox message id: ' + message_id);
  }
  return message;
};


/**
 * Processes an HTML DOM the text of "i18n" elements with translated messages.
 * This function expects HTML elements with a i18n clean and a msgid attribute.
 *
 * @param {Node} root The root node where the translation should be performed.
 */
cvox.ChromeMsgs.prototype.addTranslatedMessagesToDom = function(root) {
  var elts = root.querySelectorAll('.i18n');
  for (var i = 0; i < elts.length; i++) {
    var msgid = elts[i].getAttribute('msgid');
    if (!msgid) {
      throw new Error('Element has no msgid attribute: ' + elts[i]);
    }
    elts[i].textContent = this.getMsg(msgid);
    elts[i].classList.add('i18n-processed');
  }
};


/**
 * Retuns a number formatted correctly.
 *
 * @param {number} num The number.
 * @return {string} The number in the correct locale.
 */
cvox.ChromeMsgs.prototype.getNumber = function(num) {
  return '' + num;
};

cvox.HostFactory.msgsConstructor = cvox.ChromeMsgs;
