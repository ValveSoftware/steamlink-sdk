// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

/**
 * @fileoverview Externs for the braille IME.
 * @externs
 */

/**
 * @const
 */
chrome.input = {};

/** @const */
chrome.input.ime = {};

/**
 * @constructor
 */
function ChromeInputImeOnKeyEventEvent() {}

/**
 * @param {function(string, !ChromeKeyboardEvent): (boolean|undefined)} callback
 * @param {Array.<string>=} opt_extraInfoSpec
 */
ChromeInputImeOnKeyEventEvent.prototype.addListener =
    function(callback, opt_extraInfoSpec) {};

/**
 * @param {!Object.<string,(string|number)>} parameters An object with
 *     'contextID' (number) and 'text' (string) keys.
 * @param {function(boolean): void=} opt_callback Callback function.
 */
chrome.input.ime.commitText = function(parameters, opt_callback) {};

/**
 * @param {!Object.<string,(string|number)>} parameters An object with
 *     'contextID' (number) and 'text' (string) keys.
 * @param {function(boolean): void=} opt_callback Callback function.
 */
chrome.input.ime.deleteSurroundingText = function(parameters, opt_callback) {};

/**
 * @param {string} requestId
 * @param {boolean} response
 */
chrome.input.ime.keyEventHandled = function(requestId, response) {};

/**
 * @param {{engineID: string, items: Array.<chrome.input.ime.MenuItem>}}
 *     parameters
 * @param {function()=} opt_callback
 */
chrome.input.ime.setMenuItems = function(parameters, opt_callback) {};

/** @type {!ChromeEvent} */
chrome.input.ime.onActivate;

/** @type {!ChromeEvent} */
chrome.input.ime.onBlur;

/** @type {!ChromeEvent} */
chrome.input.ime.onDeactivated;

/** @type {!ChromeEvent} */
chrome.input.ime.onFocus;

/** @type {!ChromeEvent} */
chrome.input.ime.onInputContextUpdate;

/** @type {!ChromeInputImeOnKeyEventEvent} */
chrome.input.ime.onKeyEvent;

/** @type {!ChromeEvent} */
chrome.input.ime.onMenuItemActivated;

/** @type {!ChromeEvent} */
chrome.input.ime.onReset;

/**
 * @const
 */
chrome.runtime = {};

/** @type {!Object|undefined} */
chrome.runtime.lastError = {};

/**
 * @param {string|!Object.<string>=} opt_extensionIdOrConnectInfo Either the
 *     extensionId to connect to, in which case connectInfo params can be
 *     passed in the next optional argument, or the connectInfo params.
 * @param {!Object.<string>=} opt_connectInfo The connectInfo object,
 *     if arg1 was the extensionId to connect to.
 * @return {!Port} New port.
 */
chrome.runtime.connect = function(
    opt_extensionIdOrConnectInfo, opt_connectInfo) {};

/**
 * @constructor
 */
function Port() {}

/** @type {ChromeEvent} */
Port.prototype.onDisconnect;

/** @type {ChromeEvent} */
Port.prototype.onMessage;

/**
 * @param {Object.<string>} obj Message object.
 */
Port.prototype.postMessage = function(obj) {};

/**
 * Note: as of 2012-04-12, this function is no longer documented on
 * the public web pages, but there are still existing usages.
 */
Port.prototype.disconnect = function() {};

/**
 * @constructor
 */
function ChromeEvent() {}

/** @param {Function} callback */
ChromeEvent.prototype.addListener = function(callback) {};

/**
 * @constructor
 */
function ChromeKeyboardEvent() {}

/** @type {string} */
ChromeKeyboardEvent.prototype.type;

/** @type {string} */
ChromeKeyboardEvent.prototype.code;

/** @type {boolean} */
ChromeKeyboardEvent.prototype.altKey;

/** @type {boolean} */
ChromeKeyboardEvent.prototype.ctrlKey;

/** @type {boolean} */
ChromeKeyboardEvent.prototype.shiftKey;

/** @type {boolean} */
ChromeKeyboardEvent.prototype.capsLock;

/** @type {string} */
ChromeKeyboardEvent.prototype.requestId;

/**
 * @constructor
 */
function InputContext() {}

/** @type {number} */
InputContext.prototype.contextID;

/** @type {string} */
InputContext.prototype.type;

/**
 * @typedef {{
 *     id: string,
 *     label: (string|undefined),
 *     style: string,
 *     visible: (boolean|undefined),
 *     checked: (boolean|undefined),
 *     enabled: (boolean|undefined)
 * }}
 */
chrome.input.ime.MenuItem;

/**
 * @type {Object}
 */
var localStorage;
