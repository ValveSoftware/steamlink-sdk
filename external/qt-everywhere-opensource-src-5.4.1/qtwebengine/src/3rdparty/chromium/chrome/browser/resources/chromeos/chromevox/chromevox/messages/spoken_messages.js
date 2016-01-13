// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

/**
 * @fileoverview Useful abstraction when  speaking messages.
 *
 * Usage:
 * $m('aria_role_link').withCount(document.links.length)
 * .andPause()
 * .andMessage('aria_role_forms')
 * .withCount(document.forms.length)
 * .andEnd()
 * .speakFlush();
 *
 */

goog.provide('cvox.SpokenMessages');

goog.require('cvox.AbstractTts');
goog.require('cvox.ChromeVox');
goog.require('cvox.SpokenMessage');

/**
 * @type {Array}
 */
cvox.SpokenMessages.messages = [];

/**
 * Speaks the message chain and interrupts any on-going speech.
 */
cvox.SpokenMessages.speakFlush = function() {
  cvox.SpokenMessages.speak(cvox.AbstractTts.QUEUE_MODE_FLUSH);
};

/**
 * Speaks the message chain after on-going speech finishes.
 */
cvox.SpokenMessages.speakQueued = function() {
  cvox.SpokenMessages.speak(cvox.AbstractTts.QUEUE_MODE_QUEUE);
};

/**
 * Speak the message chain.
 * @param {number} mode The speech queue mode.
 */
cvox.SpokenMessages.speak = function(mode) {
  for (var i = 0; i < cvox.SpokenMessages.messages.length; ++i) {
    var message = cvox.SpokenMessages.messages[i];

    // An invalid message format.
    if (!message || (!message.id && !message.raw))
      throw 'Invalid message received.';

    var finalText = '';
    if (message.count != null) {
      if (message.count <= 0) {
        try {
          finalText +=
              cvox.ChromeVox.msgs.getMsg(message.id[0] + '_optional_default');
        } catch(e) {
          // The message doesn't exist.
          continue;
        }
      } else if (message.count == 1) {
        finalText += cvox.ChromeVox.msgs.getMsg(message.id[0] + '_singular');
      } else {
        finalText += cvox.ChromeVox.msgs.getMsg(message.id[0] + '_plural',
                                                [message.count]);
      }
    } else {
      if (message.raw) {
        finalText += message.raw;
      } else {
        finalText +=
            cvox.ChromeVox.msgs.getMsg.apply(cvox.ChromeVox.msgs, message.id);
      }
    }

    cvox.ChromeVox.tts.speak(finalText, mode,
                             cvox.AbstractTts.PERSONALITY_ANNOUNCEMENT);

    // Always queue after the first message.
    mode = cvox.AbstractTts.QUEUE_MODE_QUEUE;
  }

  cvox.SpokenMessages.messages = [];
}

  /**
   * The newest message.
   * @return {cvox.SpokenMessage} The newest (current) message.
   */
cvox.SpokenMessages.currentMessage = function() {
  if (cvox.SpokenMessages.messages.length == 0)
    throw 'Invalid usage of SpokenMessages; start the chain using $m()';
  return cvox.SpokenMessages.messages[cvox.SpokenMessages.messages.length - 1];
};

/**
 * Quantifies the current message.
 * This will modify the way the message gets read.
 * For example, if the count is 2, the message becomes pluralized according
 * to our i18n resources. The message "2 links" is a possible output.
 * @param {Number} count Quantifies current message.
 * @return {Object} This object, useful for chaining.
 */
cvox.SpokenMessages.withCount = function(count) {
  cvox.SpokenMessages.currentMessage().count = count;
  return cvox.SpokenMessages;
}

/**
 * Quantifies the current message.
 * Modifies the message with a current index/total description (commonly seen
 * in lists).
 * @param {number} index The current item.
 * @param {number} total The total number of items.
 * @return {Object} This object, useful for chaining.
 */
cvox.SpokenMessages.andIndexTotal = function(index, total) {
  var newMessage = new cvox.SpokenMessage();
  newMessage.raw = cvox.ChromeVox.msgs.getMsg('index_total', [index, total]);
  cvox.SpokenMessages.messages.push(newMessage);
  return cvox.SpokenMessages;
}

/**
 * Ends a message. with an appropriate marker.
 * @return {Object} This object, useful for chaining.
 */
cvox.SpokenMessages.andEnd = function() {
  return cvox.SpokenMessages.andMessage('end');
};

/**
 * Adds a message.
 * @param {string|Array} messageId The id of the message.
 * @return {Object} This object, useful for chaining.
 */
cvox.SpokenMessages.andMessage = function(messageId) {
  var newMessage = new cvox.SpokenMessage();
  newMessage.id = typeof(messageId) == 'string' ? [messageId] : messageId;
  cvox.SpokenMessages.messages.push(newMessage);
  return cvox.SpokenMessages;
};


/**
 * Adds a string as a message.
 * @param {string} message An already localized string.
 * @return {Object} This object, useful for chaining.
 */
cvox.SpokenMessages.andRawMessage = function(message) {
  var newMessage = new cvox.SpokenMessage();
  newMessage.raw = message;
  cvox.SpokenMessages.messages.push(newMessage);
  return cvox.SpokenMessages;
}

/**
 * Pauses after the message, with an appropriate marker.
 * @return {Object} This object, useful for chaining.
 */
cvox.SpokenMessages.andPause = function() {
  return cvox.SpokenMessages.andMessage('pause');
};

cvox.$m = cvox.SpokenMessages.andMessage;
