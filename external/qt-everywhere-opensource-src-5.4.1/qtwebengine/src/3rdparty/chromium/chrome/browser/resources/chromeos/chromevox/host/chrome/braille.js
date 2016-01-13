// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

/**
 * @fileoverview Bridge that sends Braille messages from content scripts or
 * other pages to the main background page.
 *
 */

goog.provide('cvox.ChromeBraille');

goog.require('cvox.AbstractBraille');
goog.require('cvox.BrailleKeyEvent');
goog.require('cvox.ChromeVoxUserCommands');
goog.require('cvox.HostFactory');


/**
 * @constructor
 * @extends {cvox.AbstractBraille}
 */
cvox.ChromeBraille = function() {
  goog.base(this);
  /**
   * @type {function(!cvox.BrailleKeyEvent, cvox.NavBraille)}
   * @private
   */
  this.commandListener_ = this.defaultCommandListener_;
  /**
   * @type {cvox.NavBraille}
   * @private
   */
  this.lastContent_ = null;
  /**
   * @type {string}
   * @private
   */
  this.lastContentId_ = '';
  /**
   * @type {number}
   * @private
   */
  this.nextLocalId_ = 1;
  cvox.ExtensionBridge.addMessageListener(goog.bind(function(msg, port) {
    // Since ChromeVox gets injected into multiple iframes on a page, check to
    // ensure that this is the "active" iframe via its focused state.
    // Furthermore, if the active element is itself an iframe, the focus is
    // within the iframe even though the containing document also has focus.
    // Don't process the event if this document isn't focused or focus lies in
    // a descendant.
    if (!document.hasFocus() || document.activeElement.tagName == 'IFRAME') {
      return;
    }
    if (msg['message'] == 'BRAILLE' && msg['args']) {
      var content = null;
      if (msg['contentId'] == this.lastContentId_) {
        content = this.lastContent_;
      }
      this.commandListener_(msg['args'], content);
    }
  }, this));
};
goog.inherits(cvox.ChromeBraille, cvox.AbstractBraille);


/** @override */
cvox.ChromeBraille.prototype.write = function(params) {
  this.lastContent_ = params;
  this.updateLastContentId_();
  var outParams = params.toJson();

  var message = {'target': 'BRAILLE',
                 'action': 'write',
                 'params': outParams,
                 'contentId' : this.lastContentId_};

  cvox.ExtensionBridge.send(message);
};


/** @private */
cvox.ChromeBraille.prototype.updateLastContentId_ = function() {
  this.lastContentId_ = cvox.ExtensionBridge.uniqueId() + '.' +
      this.nextLocalId_++;
};


/** @override */
cvox.ChromeBraille.prototype.setCommandListener = function(func) {
  this.commandListener_ = func;
};


/**
 * Dispatches braille input commands.
 * @param {!cvox.BrailleKeyEvent} brailleEvt The braille key event.
 * @param {cvox.NavBraille} content display content when command was issued,
 *                                  if available.
 * @private
 */
cvox.ChromeBraille.prototype.defaultCommandListener_ = function(brailleEvt,
                                                                content) {
  var command = cvox.ChromeVoxUserCommands.commands[brailleEvt.command];
  if (command) {
    command({event: brailleEvt, content: content});
  } else {
    console.error('Unknown braille command: ' + JSON.stringify(brailleEvt));
  }
};


/** Export platform constructor. */
cvox.HostFactory.brailleConstructor = cvox.ChromeBraille;
