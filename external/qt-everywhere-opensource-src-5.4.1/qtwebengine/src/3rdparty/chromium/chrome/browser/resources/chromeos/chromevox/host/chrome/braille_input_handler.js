// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

/**
 * @fileoverview Handles braille input keys when the user is typing or editing
 * text in an input field.  This class cooperates with the Braille IME
 * that is built into Chrome OS to do the actual text editing.
 */

goog.provide('cvox.BrailleInputHandler');

goog.require('cvox.BrailleKeyCommand');
goog.require('cvox.BrailleKeyEvent');
goog.require('cvox.ExpandingBrailleTranslator');


/**
 * @constructor
 */
cvox.BrailleInputHandler = function() {
  /**
   * Port of the connected IME if any.
   * @type {Port}
   * @private
   */
  this.imePort_ = null;
  /**
   * {code true} when the Braille IME is connected and has signaled that it is
   * active.
   * @type {boolean}
   * @private
   */
  this.imeActive_ = false;
  /**
   * The input context of the current input field, as reported by the IME.
   * {@code null} if no input field has focus.
   * @type {{contextID: number, type: string}?}
   * @private
   */
  this.inputContext_ = null;
  /**
   * @type {cvox.LibLouis.Translator}
   * @private
   */
  this.defaultTranslator_ = null;
  /**
   * @type {cvox.LibLouis.Translator}
   * @private
   */
  this.uncontractedTranslator_ = null;
  /**
   * The translator currently used for typing, if
   * {@code this.cells_.length > 0}.
   * @type {cvox.LibLouis.Translator}
   * @private
   */
  this.activeTranslator_ = null;
  /**
   * Braille cells that have been typed by the user so far.
   * @type {Array.<number>}
   * @private
   */
  this.cells_ = [];
  /**
   * Text resulting from translating {@code this.cells_}.
   * @type {string}
   * @private
   */
  this.text_ = '';
  /**
   * Text that currently precedes the first selection end-point.
   * @type {string}
   * @private
   */
  this.currentTextBefore_ = '';
  /**
   * Text that currently follows the last selection end-point.
   * @type {string}
   * @private
   */
  this.currentTextAfter_ = '';
  /**
   * List of strings that we expect to be set as preceding text of the
   * selection.  This is populated when we send text changes to the IME so that
   * our own changes don't reset the pending cells.
   * @type {Array.<string>}
   * @private
   */
  this.pendingTextsBefore_ = [];
  /**
   * Cells that were entered while the IME wasn't active.  These will be
   * submitted once the IME becomes active and reports the current input field.
   * This is necessary because the IME is activated on the first braille
   * dots command, but we'll receive the command in parallel.  To work around
   * the race, we store the cell entered until we can submit it to the IME.
   * @type {Array.<number>}
   * @private
   */
  this.pendingCells_ = [];
};


/**
 * The ID of the Braille IME extension built into Chrome OS.
 * @const {string}
 * @private
 */
cvox.BrailleInputHandler.IME_EXTENSION_ID_ =
    'jddehjeebkoimngcbdkaahpobgicbffp';


/**
 * Name of the port to use for communicating with the Braille IME.
 * @const {string}
 * @private
 */
cvox.BrailleInputHandler.IME_PORT_NAME_ = 'cvox.BrailleIme.Port';


/**
 * Starts to listen for connections from the ChromeOS braille IME.
 */
cvox.BrailleInputHandler.prototype.init = function() {
  chrome.runtime.onConnectExternal.addListener(
      goog.bind(this.onImeConnect_, this));
};


/**
 * Sets the translator(s) to be used for input.
 * @param {cvox.LibLouis.Translator} defaultTranslator Translator to use by
 *     default from now on.
 * @param {cvox.LibLouis.Translator=} opt_uncontractedTranslator Translator
 *     to be used inside a word (non-whitespace).
 */
cvox.BrailleInputHandler.prototype.setTranslator = function(
    defaultTranslator, opt_uncontractedTranslator) {
  this.defaultTranslator_ = defaultTranslator;
  this.uncontractedTranslator_ = opt_uncontractedTranslator || null;
  this.resetText_();
};


/**
 * Called when the content on the braille display is updated.  Modifies the
 * input state according to the new content.
 * @param {cvox.Spannable} text Text, optionally with value and selection
 *     spans.
 */
cvox.BrailleInputHandler.prototype.onDisplayContentChanged = function(text) {
  var valueSpan = text.getSpanInstanceOf(cvox.BrailleUtil.ValueSpan);
  var selectionSpan = text.getSpanInstanceOf(
      cvox.BrailleUtil.ValueSelectionSpan);
  if (!(valueSpan && selectionSpan)) {
    return;
  }
  // The type casts are ok because the spans are known to exist.
  var valueStart = /** @type {number} */ (text.getSpanStart(valueSpan));
  var valueEnd = /** @type {number} */ (text.getSpanEnd(valueSpan));
  var selectionStart =
      /** @type {number} */ (text.getSpanStart(selectionSpan));
  var selectionEnd = /** @type {number} */ (text.getSpanEnd(selectionSpan));
  if (selectionStart < valueStart || selectionEnd > valueEnd) {
    console.error('Selection outside of value in braille content');
    this.resetText_();
    return;
  }
  var oldTextBefore = this.currentTextBefore_;
  var oldTextAfter = this.currentTextAfter_;
  this.currentTextBefore_ = text.toString().substring(
      valueStart, selectionStart);
  this.currentTextAfter_ = text.toString().substring(selectionEnd, valueEnd);
  if (this.cells_.length > 0) {
    // Ignore this change if the preceding text hasn't changed.
    if (oldTextBefore === this.currentTextBefore_) {
      return;
    }
    // See if we are expecting this change as a result of one of our own edits.
    if (this.pendingTextsBefore_.length > 0) {
      // Allow changes to be coalesced by the input system in an attempt to not
      // be too brittle.
      for (var i = 0; i < this.pendingTextsBefore_.length; ++i) {
        if (this.currentTextBefore_ === this.pendingTextsBefore_[i]) {
          // Delete all previous expected changes and ignore this one.
          this.pendingTextsBefore_.splice(0, i + 1);
          return;
        }
      }
    }
    // There was an actual text change (or cursor movement) that we hadn't
    // caused ourselves, reset any pending input.
    this.resetText_();
  } else {
    this.updateActiveTranslator_();
  }
};


/**
 * Handles braille key events used for input by editing the current input field
 * appropriately.
 * @param {!cvox.BrailleKeyEvent} event The key event.
 * @return {boolean} {@code true} if the event was handled, {@code false}
 *     if it should propagate further.
 */
cvox.BrailleInputHandler.prototype.onBrailleKeyEvent = function(event) {
  if (event.command === cvox.BrailleKeyCommand.DOTS) {
    return this.onBrailleDots_(/** @type {number} */(event.brailleDots));
  }
  // Any other braille command cancels the pending cells.
  this.pendingCells_.length = 0;
  if (event.command === cvox.BrailleKeyCommand.STANDARD_KEY) {
    if (event.standardKeyCode === 'Backspace' &&
        !event.altKey && !event.ctrlKey && !event.shiftKey &&
        this.onBackspace_()) {
      return true;
    } else {
      this.sendKeyEventPair_(event);
      return true;
    }
  }
  return false;
};


/**
 * Returns how the value of the currently displayed content should be expanded
 * given the current input state.
 * @return {cvox.ExpandingBrailleTranslator.ExpansionType}
 *     The current expansion type.
 */
cvox.BrailleInputHandler.prototype.getExpansionType = function() {
  if (this.inAlwaysUncontractedContext_()) {
    return cvox.ExpandingBrailleTranslator.ExpansionType.ALL;
  }
  if (this.cells_.length > 0 &&
      this.activeTranslator_ === this.defaultTranslator_) {
    return cvox.ExpandingBrailleTranslator.ExpansionType.NONE;
  }
  return cvox.ExpandingBrailleTranslator.ExpansionType.SELECTION;
};


/**
 * @return {boolean} {@code true} if we have an input context and uncontracted
 *     braille should always be used for that context.
 * @private
 */
cvox.BrailleInputHandler.prototype.inAlwaysUncontractedContext_ = function() {
  if (this.inputContext_) {
    var inputType = this.inputContext_.type;
    return inputType === 'url' || inputType === 'email';
  }
  return false;
};


/**
 * Called when a user typed a braille cell.
 * @param {number} dots The dot pattern of the cell.
 * @return {boolean} Whether the event was handled or should be allowed to
 *    propagate further.
 * @private
 */
cvox.BrailleInputHandler.prototype.onBrailleDots_ = function(dots) {
  if (!this.imeActive_) {
    this.pendingCells_.push(dots);
    return true;
  }
  if (!this.inputContext_ || !this.activeTranslator_) {
    return false;
  }
  // Avoid accumulating cells forever when typing without moving the cursor
  // by flushing the input when we see a blank cell.
  // Note that this might switch to contracted if appropriate.
  if (this.cells_.length > 0 && this.cells_[this.cells_.length - 1] == 0) {
    this.resetText_();
  }
  this.cells_.push(dots);
  this.updateText_();
  return true;
};


/**
 * Handles the backspace key by deleting the last typed cell if possible.
 * @return {boolean} {@code true} if the event was handled, {@code false}
 *     if it wasn't and should propagate further.
 * @private
 */
cvox.BrailleInputHandler.prototype.onBackspace_ = function() {
  if (this.imeActive_ && this.cells_.length > 0) {
    --this.cells_.length;
    this.updateText_();
    return true;
  }
  return false;
};


/**
 * Updates the translated text based on the current cells and sends the
 * delta to the IME.
 * @private
 */
cvox.BrailleInputHandler.prototype.updateText_ = function() {
  var cellsBuffer = new Uint8Array(this.cells_).buffer;
  this.activeTranslator_.backTranslate(cellsBuffer, goog.bind(function(result) {
    if (result === null) {
      console.error('Error when backtranslating braille cells');
      return;
    }
    var oldLength = this.text_.length;
    // Find the common prefix of the old and new text.
    var commonPrefixLength = this.longestCommonPrefixLength_(
        this.text_, result);
    this.text_ = result;
    // How many characters we need to delete from the existing text to replace
    // them with characters from the new text.
    var deleteLength = oldLength - commonPrefixLength;
    // New text, if any, to insert after deleting the deleteLength characters
    // before the cursor.
    var toInsert = result.substring(commonPrefixLength);
    if (deleteLength > 0 || toInsert.length > 0) {
      // After deleting, we expect this text to be present before the cursor.
      var textBeforeAfterDelete = this.currentTextBefore_.substring(
          0, this.currentTextBefore_.length - deleteLength);
      if (deleteLength > 0) {
        // Queue this text up to be ignored when the change comes in.
        this.pendingTextsBefore_.push(textBeforeAfterDelete);
      }
      if (toInsert.length > 0) {
        // Likewise, queue up what we expect to be before the cursor after
        // the replacement text is inserted.
        this.pendingTextsBefore_.push(textBeforeAfterDelete + toInsert);
      }
      // Send the replace operation to be performed asynchronously by
      // the IME.
      this.postImeMessage_({type: 'replaceText',
                       contextID: this.inputContext_.contextID,
                       deleteBefore: deleteLength,
                       newText: toInsert});
    }
  }, this));
};


/**
 * Resets the pending braille input and text.
 * @private
 */
cvox.BrailleInputHandler.prototype.resetText_ = function() {
  this.cells_.length = 0;
  this.text_ = '';
  this.pendingTextsBefore_.length = 0;
  this.updateActiveTranslator_();
};


/**
 * Updates the active translator based on the current input context.
 * @private
 */
cvox.BrailleInputHandler.prototype.updateActiveTranslator_ = function() {
  this.activeTranslator_ = this.defaultTranslator_;
  if (this.uncontractedTranslator_) {
    var textBefore = this.currentTextBefore_;
    var textAfter = this.currentTextAfter_;
    if (this.inAlwaysUncontractedContext_() ||
        (textBefore.length > 0 && /\S$/.test(textBefore)) ||
        (textAfter.length > 0 && /^\S/.test(textAfter))) {
      this.activeTranslator_ = this.uncontractedTranslator_;
    }
  }
};


/**
 * Called when another extension connects to this extension.  Accepts
 * connections from the ChromeOS builtin Braille IME and ignores connections
 * from other extensions.
 * @param {Port} port The port used to communicate with the other extension.
 * @private
 */
cvox.BrailleInputHandler.prototype.onImeConnect_ = function(port) {
  if (port.name !== cvox.BrailleInputHandler.IME_PORT_NAME_ ||
      port.sender.id !== cvox.BrailleInputHandler.IME_EXTENSION_ID_) {
    return;
  }
  if (this.imePort_) {
    this.imePort_.disconnect();
  }
  port.onDisconnect.addListener(goog.bind(this.onImeDisconnect_, this, port));
  port.onMessage.addListener(goog.bind(this.onImeMessage_, this));
  this.imePort_ = port;
};


/**
 * Called when a message is received from the IME.
 * @param {*} message The message.
 * @private
 */
cvox.BrailleInputHandler.prototype.onImeMessage_ = function(message) {
  if (!goog.isObject(message)) {
    console.error('Unexpected message from Braille IME: ',
                  JSON.stringify(message));
  }
  switch (message.type) {
    case 'activeState':
      this.imeActive_ = message.active;
      break;
    case 'inputContext':
      this.inputContext_ = message.context;
      this.resetText_();
      if (this.imeActive_ && this.inputContext_) {
        this.pendingCells_.forEach(goog.bind(this.onBrailleDots_, this));
      }
      this.pendingCells_.length = 0;
      break;
    case 'brailleDots':
      this.onBrailleDots_(message['dots']);
      break;
    case 'backspace':
      // Note that we can't send the backspace key through the
      // virtualKeyboardPrivate API in this case because it would then be
      // processed by the IME again, leading to an infinite loop.
      this.postImeMessage_(
          {type: 'keyEventHandled', requestId: message['requestId'],
           result: this.onBackspace_()});
      break;
    case 'reset':
      this.resetText_();
      break;
    default:
      console.error('Unexpected message from Braille IME: ',
                    JSON.stringify(message));
    break;
  }
};


/**
 * Called when the IME port is disconnected.
 * @param {Port} port The port that was disconnected.
 * @private
 */
cvox.BrailleInputHandler.prototype.onImeDisconnect_ = function(port) {
  this.imePort_ = null;
  this.resetText_();
  this.imeActive_ = false;
  this.inputContext_ = null;
};


/**
 * Posts a message to the IME.
 * @param {Object} message The message.
 * @return {boolean} {@code true} if the message was sent, {@code false} if
 *     there was no connection open to the IME.
 * @private
 */
cvox.BrailleInputHandler.prototype.postImeMessage_ = function(message) {
  if (this.imePort_) {
    this.imePort_.postMessage(message);
    return true;
  }
  return false;
};


/**
 * Sends a {@code keydown} key event followed by a {@code keyup} event
 * corresponding to an event generated by the braille display.
 * @param {!cvox.BrailleKeyEvent} event The braille key event to base the
 *     key events on.
 * @private
 */
cvox.BrailleInputHandler.prototype.sendKeyEventPair_ = function(event) {
  // Use the virtual keyboard API instead of the IME key event API
  // so that these keys work even if the Braille IME is not active.
  var keyName = /** @type {string} */ (event.standardKeyCode);
  var numericCode = cvox.BrailleKeyEvent.keyCodeToLegacyCode(keyName);
  if (!goog.isDef(numericCode)) {
    throw Error('Unknown key code in event: ' + JSON.stringify(event));
  }
  var keyEvent = {
    type: 'keydown',
    keyCode: numericCode,
    keyName: keyName,
    charValue: cvox.BrailleKeyEvent.keyCodeToCharValue(keyName),
    // See chrome/common/extensions/api/virtual_keyboard_private.json for
    // these constants.
    modifiers: (event.shiftKey ? 2 : 0) |
        (event.ctrlKey ? 4 : 0) |
        (event.altKey ? 8 : 0)
  };
  chrome.virtualKeyboardPrivate.sendKeyEvent(keyEvent);
  keyEvent.type = 'keyup';
  chrome.virtualKeyboardPrivate.sendKeyEvent(keyEvent);
};


/**
 * Returns the length of the longest common prefix of two strings.
 * @param {string} first The first string.
 * @param {string} second The second string.
 * @return {number} The longest common prefix, which may be 0 for an
 *     empty common prefix.
 * @private
 */
cvox.BrailleInputHandler.prototype.longestCommonPrefixLength_ = function(
    first, second) {
  var limit = Math.min(first.length, second.length);
  var i;
  for (i = 0; i < limit; ++i) {
    if (first.charAt(i) != second.charAt(i)) {
      break;
    }
  }
  return i;
};
