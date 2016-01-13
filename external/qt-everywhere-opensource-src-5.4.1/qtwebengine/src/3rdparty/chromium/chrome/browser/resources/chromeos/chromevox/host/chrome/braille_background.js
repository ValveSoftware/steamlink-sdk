// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

/**
 * @fileoverview Sends Braille commands to the Braille API.
 */

goog.provide('cvox.BrailleBackground');

goog.require('cvox.AbstractBraille');
goog.require('cvox.BrailleDisplayManager');
goog.require('cvox.BrailleInputHandler');
goog.require('cvox.BrailleKeyEvent');
goog.require('cvox.BrailleTable');
goog.require('cvox.ChromeVox');
goog.require('cvox.LibLouis');


/**
 * @constructor
 * @param {cvox.BrailleDisplayManager=} opt_displayManagerForTest
 *        Display manager (for mocking in tests).
 * @param {cvox.BrailleInputHandler=} opt_inputHandlerForTest Input handler
 *        (for mocking in tests).
 * @extends {cvox.AbstractBraille}
 */
cvox.BrailleBackground = function(opt_displayManagerForTest,
                                  opt_inputHandlerForTest) {
  goog.base(this);
  /**
   * @type {cvox.BrailleDisplayManager}
   * @private
   */
  this.displayManager_ = opt_displayManagerForTest ||
      new cvox.BrailleDisplayManager();
  cvox.BrailleTable.getAll(goog.bind(function(tables) {
    /**
     * @type {!Array.<cvox.BrailleTable.Table>}
     * @private
     */
    this.tables_ = tables;
    this.initialize_(0);
  }, this));
  this.displayManager_.setCommandListener(
      goog.bind(this.onBrailleKeyEvent_, this));
  /**
   * @type {cvox.NavBraille}
   * @private
   */
  this.lastContent_ = null;
  /**
   * @type {?string}
   * @private
   */
  this.lastContentId_ = null;
  /**
   * @type {!cvox.BrailleInputHandler}
   * @private
   */
  this.inputHandler_ = opt_inputHandlerForTest ||
      new cvox.BrailleInputHandler();
  this.inputHandler_.init();
};
goog.inherits(cvox.BrailleBackground, cvox.AbstractBraille);


/** @override */
cvox.BrailleBackground.prototype.write = function(params) {
  this.lastContentId_ = null;
  this.lastContent_ = null;
  this.inputHandler_.onDisplayContentChanged(params.text);
  this.displayManager_.setContent(
      params, this.inputHandler_.getExpansionType());
};


/** @override */
cvox.BrailleBackground.prototype.setCommandListener = function(func) {
  // TODO(plundblad): Implement when the background page handles commands
  // as well.
};


/**
 * Refreshes the braille translator used for output.  This should be
 * called when something changed (such as a preference) to make sure that
 * the correct translator is used.
 */
cvox.BrailleBackground.prototype.refreshTranslator = function() {
  if (!this.liblouis_) {
    return;
  }
  // First, see if we have a braille table set previously.
  var tables = this.tables_;
  var table = cvox.BrailleTable.forId(tables, localStorage['brailleTable']);
  if (!table) {
    // Match table against current locale.
    var currentLocale = chrome.i18n.getMessage('@@ui_locale').split(/[_-]/);
    var major = currentLocale[0];
    var minor = currentLocale[1];
    var firstPass = tables.filter(function(table) {
      return table.locale.split(/[_-]/)[0] == major;
    });
    if (firstPass.length > 0) {
      table = firstPass[0];
      if (minor) {
        var secondPass = firstPass.filter(function(table) {
          return table.locale.split(/[_-]/)[1] == minor;
        });
        if (secondPass.length > 0) {
          table = secondPass[0];
        }
      }
    }
  }
  if (!table) {
    table = cvox.BrailleTable.forId(tables, 'en-US-comp8');
  }
  // TODO(plundblad): ONly update when user explicitly chooses a table
  // so that switching locales changes table by default.
  localStorage['brailleTable'] = table.id;
  if (table.dots == '6') {
    localStorage['brailleTableType'] = 'brailleTable6';
    localStorage['brailleTable6'] = table.id;
  } else {
    localStorage['brailleTableType'] = 'brailleTable8';
    localStorage['brailleTable8'] = table.id;
  }

  // Initialize all other defaults.
  // TODO(plundblad): Stop doing this here.
  if (!localStorage['brailleTable6']) {
    localStorage['brailleTable6'] = 'en-US-g1';
  }
  if (!localStorage['brailleTable8']) {
    localStorage['brailleTable8'] = 'en-US-comp8';
  }

  // If the user explicitly set an 8 dot table, use that when looking
  // for an uncontracted table.  Otherwise, use the current table and let
  // getUncontracted find an appropriate corresponding table.
  var table8Dot = cvox.BrailleTable.forId(tables,
                                          localStorage['brailleTable8']);
  var uncontractedTable = cvox.BrailleTable.getUncontracted(
      tables,
      table8Dot ? table8Dot : table);
  this.liblouis_.getTranslator(table.fileName, goog.bind(
      function(translator) {
        if (uncontractedTable.id === table.id) {
          this.displayManager_.setTranslator(translator);
          this.inputHandler_.setTranslator(translator);
        } else {
          this.liblouis_.getTranslator(uncontractedTable.fileName, goog.bind(
              function(uncontractedTranslator) {
                this.displayManager_.setTranslator(
                    translator, uncontractedTranslator);
                this.inputHandler_.setTranslator(
                    translator, uncontractedTranslator);
              }, this));
        }
      }, this));
};


/**
 * Called when a Braille message is received from a page content script.
 * @param {Object} msg The Braille message.
 */
cvox.BrailleBackground.prototype.onBrailleMessage = function(msg) {
  if (msg['action'] == 'write') {
    this.lastContentId_ = msg['contentId'];
    this.lastContent_ = cvox.NavBraille.fromJson(msg['params']);
    this.inputHandler_.onDisplayContentChanged(this.lastContent_.text);
    this.displayManager_.setContent(
        this.lastContent_, this.inputHandler_.getExpansionType());
  }
};


/**
 * Initialization to be done after part of the background page's DOM has been
 * constructed. Currently only used on ChromeOS.
 * @param {number} retries Number of retries.
 * @private
 */
cvox.BrailleBackground.prototype.initialize_ = function(retries) {
  if (retries > 5) {
    console.error(
        'Timeout waiting for document.body; not initializing braille.');
    return;
  }
  if (!document.body) {
    window.setTimeout(goog.bind(this.initialize_, this, ++retries), 500);
  } else {
    /**
     * @type {cvox.LibLouis}
     * @private
     */
    this.liblouis_ = new cvox.LibLouis(
        chrome.extension.getURL(
            'chromevox/background/braille/liblouis_nacl.nmf'),
        chrome.extension.getURL(
            'chromevox/background/braille/tables'));
    this.liblouis_.attachToElement(document.body);
    this.refreshTranslator();
  }
};


/**
 * Handles braille key events by dispatching either to the input handler or
 * a content script.
 * @param {!cvox.BrailleKeyEvent} brailleEvt The event.
 * @param {cvox.NavBraille} content Content of display when event fired.
 * @private
 */
cvox.BrailleBackground.prototype.onBrailleKeyEvent_ = function(
    brailleEvt, content) {
  if (this.inputHandler_.onBrailleKeyEvent(brailleEvt)) {
    return;
  }
  this.sendCommand_(brailleEvt, content);
};


/**
 * Dispatches braille input commands to the content script.
 * @param {!cvox.BrailleKeyEvent} brailleEvt The event.
 * @param {cvox.NavBraille} content Content of display when event fired.
 * @private
 */
cvox.BrailleBackground.prototype.sendCommand_ =
    function(brailleEvt, content) {
  var msg = {
    'message': 'BRAILLE',
    'args': brailleEvt
  };
  if (content === this.lastContent_) {
    msg.contentId = this.lastContentId_;
  }
  cvox.ExtensionBridge.send(msg);
};
