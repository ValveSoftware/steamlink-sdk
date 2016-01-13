// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

/**
 * @fileoverview Puts text on a braille display.
 *
 */

goog.provide('cvox.BrailleDisplayManager');

goog.require('cvox.BrailleCaptionsBackground');
goog.require('cvox.BrailleDisplayState');
goog.require('cvox.ExpandingBrailleTranslator');
goog.require('cvox.LibLouis');
goog.require('cvox.NavBraille');


/**
 * @constructor
 */
cvox.BrailleDisplayManager = function() {
  /**
   * @type {cvox.ExpandingBrailleTranslator}
   * @private
   */
  this.translator_ = null;
  /**
   * @type {!cvox.NavBraille}
   * @private
   */
  this.content_ = new cvox.NavBraille({});
  /**
   * @type {!cvox.ExpandingBrailleTranslator.ExpansionType} valueExpansion
   * @private
   */
  this.expansionType_ =
      cvox.ExpandingBrailleTranslator.ExpansionType.SELECTION;
  /**
   * @type {!ArrayBuffer}
   * @private
   */
  this.translatedContent_ = new ArrayBuffer(0);
  /**
   * @type {number}
   * @private
   */
  this.panPosition_ = 0;
  /**
   * @type {function(!cvox.BrailleKeyEvent, cvox.NavBraille)}
   * @private
   */
  this.commandListener_ = function() {};
  /**
   * Current display state used for width calculations.  This is different from
   * realDisplayState_ if the braille captions feature is enabled and there is
   * no hardware display connected.  Otherwise, it is the same object
   * as realDisplayState_.
   * @type {!cvox.BrailleDisplayState}
   * @private
   */
  this.displayState_ = {available: false, textCellCount: undefined};
  /**
   * State reported from the chrome api, reflecting a real hardware
   * display.
   * @type {!cvox.BrailleDisplayState}
   * @private
   */
  this.realDisplayState_ = this.displayState_;
  /**
   * @type {!Array.<number>}
   * @private
   */
  this.textToBraille_ = [];
  /**
   * @type {Array.<number>}
   * @private
   */
  this.brailleToText_ = [];

  cvox.BrailleCaptionsBackground.init(goog.bind(
      this.onCaptionsStateChanged_, this));
  if (goog.isDef(chrome.brailleDisplayPrivate)) {
    var onDisplayStateChanged = goog.bind(this.refreshDisplayState_, this);
    chrome.brailleDisplayPrivate.getDisplayState(onDisplayStateChanged);
    chrome.brailleDisplayPrivate.onDisplayStateChanged.addListener(
        onDisplayStateChanged);
    chrome.brailleDisplayPrivate.onKeyEvent.addListener(
        goog.bind(this.onKeyEvent_, this));
  } else {
    // Get the initial captions state since we won't refresh the display
    // state in an API callback in this case.
    this.onCaptionsStateChanged_();
  }
};


/**
 * Dots representing a cursor.
 * @const
 * @private
 */
cvox.BrailleDisplayManager.CURSOR_DOTS_ = 1 << 6 | 1 << 7;


/**
 * @param {!cvox.NavBraille} content Content to send to the braille display.
 * @param {!cvox.ExpandingBrailleTranslator.ExpansionType} expansionType
 *     If the text has a {@code ValueSpan}, this indicates how that part
 *     of the display content is expanded when translating to braille.
 *     (See {@code cvox.ExpandingBrailleTranslator}).
 */
cvox.BrailleDisplayManager.prototype.setContent = function(
    content, expansionType) {
  this.translateContent_(content, expansionType);
};


/**
 * Sets the command listener.  When a command is invoked, the listener will be
 * called with the BrailleKeyEvent corresponding to the command and the content
 * that was present on the display when the command was invoked.  The content
 * is guaranteed to be identical to an object previously used as the parameter
 * to cvox.BrailleDisplayManager.setContent, or null if no content was set.
 * @param {function(!cvox.BrailleKeyEvent, cvox.NavBraille)} func The listener.
 */
cvox.BrailleDisplayManager.prototype.setCommandListener = function(func) {
  this.commandListener_ = func;
};


/**
 * Sets the translator to be used for the braille content and refreshes the
 * braille display with the current content using the new translator.
 * @param {cvox.LibLouis.Translator} defaultTranslator Translator to use by
 *     default from now on.
 * @param {cvox.LibLouis.Translator=} opt_uncontractedTranslator Translator
 *     to use around text selection end-points.
 */
cvox.BrailleDisplayManager.prototype.setTranslator =
    function(defaultTranslator, opt_uncontractedTranslator) {
  var hadTranslator = (this.translator_ != null);
  if (defaultTranslator) {
    this.translator_ = new cvox.ExpandingBrailleTranslator(
        defaultTranslator, opt_uncontractedTranslator);
  } else {
    this.translator_ = null;
  }
  this.translateContent_(this.content_, this.expansionType_);
  if (hadTranslator && !this.translator_) {
    this.refresh_();
  }
};


/**
 * @param {!cvox.BrailleDisplayState} newState Display state reported
 *     by the extension API.
 * @private
 */
cvox.BrailleDisplayManager.prototype.refreshDisplayState_ =
    function(newState) {
  this.realDisplayState_ = newState;
  if (newState.available) {
    this.displayState_ = newState;
  } else {
    this.displayState_ =
        cvox.BrailleCaptionsBackground.getVirtualDisplayState();
  }
  this.panPosition_ = 0;
  this.refresh_();
};


/**
 * Called when the state of braille captions changes.
 * @private
 */
cvox.BrailleDisplayManager.prototype.onCaptionsStateChanged_ = function() {
  // Force reevaluation of the display state based on our stored real
  // hardware display state, meaning that if a real display is connected,
  // that takes precedence over the state from the captions 'virtual' display.
  this.refreshDisplayState_(this.realDisplayState_);
};


/** @private */
cvox.BrailleDisplayManager.prototype.refresh_ = function() {
  if (!this.displayState_.available) {
    return;
  }
  var buf = this.translatedContent_.slice(this.panPosition_,
      this.panPosition_ + this.displayState_.textCellCount);
  if (this.realDisplayState_.available) {
    chrome.brailleDisplayPrivate.writeDots(buf);
  }
  if (cvox.BrailleCaptionsBackground.isEnabled()) {
    var start = this.brailleToTextPosition_(this.panPosition_);
    var end = this.brailleToTextPosition_(this.panPosition_ + buf.byteLength);
    cvox.BrailleCaptionsBackground.setContent(
        this.content_.text.toString().substring(start, end), buf);
  }
};


/**
 * @param {!cvox.NavBraille} newContent New display content.
 * @param {cvox.ExpandingBrailleTranslator.ExpansionType} newExpansionType
 *     How the value part of of the new content should be expanded
 *     with regards to contractions.
 * @private
 */
cvox.BrailleDisplayManager.prototype.translateContent_ = function(
    newContent, newExpansionType) {
  if (!this.translator_) {
    this.content_ = newContent;
    this.expansionType_ = newExpansionType;
    this.translatedContent_ = new ArrayBuffer(0);
    this.textToBraille_.length = 0;
    this.brailleToText_.length = 0;
    return;
  }
  this.translator_.translate(
      newContent.text,
      newExpansionType,
      goog.bind(function(cells, textToBraille, brailleToText) {
        this.content_ = newContent;
        this.expansionType_ = newExpansionType;
        var startIndex = this.content_.startIndex;
        var endIndex = this.content_.endIndex;
        this.panPosition_ = 0;
        if (startIndex >= 0) {
          var translatedStartIndex;
          var translatedEndIndex;
          if (startIndex >= textToBraille.length) {
            // Allow the cells to be extended with one extra cell for
            // a carret after the last character.
            var extCells = new ArrayBuffer(cells.byteLength + 1);
            var extCellsView = new Uint8Array(extCells);
            extCellsView.set(new Uint8Array(cells));
            // Last byte is initialized to 0.
            cells = extCells;
            translatedStartIndex = cells.byteLength - 1;
          } else {
            translatedStartIndex = textToBraille[startIndex];
          }
          if (endIndex >= textToBraille.length) {
            // endIndex can't be past-the-end of the last cell unless
            // startIndex is too, so we don't have to do another
            // extension here.
            translatedEndIndex = cells.byteLength;
          } else {
            translatedEndIndex = textToBraille[endIndex];
          }
          this.writeCursor_(cells, translatedStartIndex, translatedEndIndex);
          if (this.displayState_.available) {
            var textCells = this.displayState_.textCellCount;
            this.panPosition_ = Math.floor(translatedStartIndex / textCells) *
                textCells;
          }
        }
        this.translatedContent_ = cells;
        this.brailleToText_ = brailleToText;
        this.textToBraille_ = textToBraille;
        this.refresh_();
      }, this));
};


/**
 * @param {cvox.BrailleKeyEvent} event The key event.
 * @private
 */
cvox.BrailleDisplayManager.prototype.onKeyEvent_ = function(event) {
  switch (event.command) {
    case cvox.BrailleKeyCommand.PAN_LEFT:
      this.panLeft_();
      break;
    case cvox.BrailleKeyCommand.PAN_RIGHT:
      this.panRight_();
      break;
    case cvox.BrailleKeyCommand.ROUTING:
      event.displayPosition = this.brailleToTextPosition_(
          event.displayPosition + this.panPosition_);
      // fall through
    default:
      this.commandListener_(event, this.content_);
      break;
  }
};


/**
 * Shift the display by one full display size and refresh the content.
 * Sends the appropriate command if the display is already at the leftmost
 * position.
 * @private
 */
cvox.BrailleDisplayManager.prototype.panLeft_ = function() {
  if (this.panPosition_ <= 0) {
    this.commandListener_({
      command: cvox.BrailleKeyCommand.PAN_LEFT
    }, this.content_);
    return;
  }
  this.panPosition_ = Math.max(
      0, this.panPosition_ - this.displayState_.textCellCount);
  this.refresh_();
};


/**
 * Shifts the display position to the right by one full display size and
 * refreshes the content.  Sends the appropriate command if the display is
 * already at its rightmost position.
 * @private
 */
cvox.BrailleDisplayManager.prototype.panRight_ = function() {
  var newPosition = this.panPosition_ + this.displayState_.textCellCount;
  if (newPosition >= this.translatedContent_.byteLength) {
    this.commandListener_({
      command: cvox.BrailleKeyCommand.PAN_RIGHT
    }, this.content_);
    return;
  }
  this.panPosition_ = newPosition;
  this.refresh_();
};


/**
 * Writes a cursor in the specified range into translated content.
 * @param {ArrayBuffer} buffer Buffer to add cursor to.
 * @param {number} startIndex The start index to place the cursor.
 * @param {number} endIndex The end index to place the cursor (exclusive).
 * @private
 */
cvox.BrailleDisplayManager.prototype.writeCursor_ = function(
    buffer, startIndex, endIndex) {
  if (startIndex < 0 || startIndex >= buffer.byteLength ||
      endIndex < startIndex || endIndex > buffer.byteLength) {
    return;
  }
  if (startIndex == endIndex) {
    endIndex = startIndex + 1;
  }
  var dataView = new DataView(buffer);
  while (startIndex < endIndex) {
    var value = dataView.getUint8(startIndex);
    value |= cvox.BrailleDisplayManager.CURSOR_DOTS_;
    dataView.setUint8(startIndex, value);
    startIndex++;
  }
};


/**
 * Returns the text position corresponding to an absolute braille position,
 * that is not accounting for the current pan position.
 * @private
 * @param {number} braillePosition Braille position relative to the startof
 *        the translated content.
 * @return {number} The mapped position in code units.
 */
cvox.BrailleDisplayManager.prototype.brailleToTextPosition_ =
    function(braillePosition) {
  var mapping = this.brailleToText_;
  if (braillePosition < 0) {
    // This shouldn't happen.
    console.error('WARNING: Braille position < 0: ' + braillePosition);
    return 0;
  } else if (braillePosition >= mapping.length) {
    // This happens when the user clicks on the right part of the display
    // when it is not entirely filled with content.  Allow addressing the
    // position after the last character.
    return this.content_.text.getLength();
  } else {
    return mapping[braillePosition];
  }
};
