// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

/**
 * @param {string} url
 * @constructor
 */
function Audio(url) {}
Audio.prototype.play;
Audio.prototype.pause;
Audio.prototype.autoplay;

/**
 * @type {Object}
 */
chrome.brailleDisplayPrivate = {};

/**
 * @param {function(!{available: boolean, textCellCount: (number|undefined)})}
 *        callback
 */
chrome.brailleDisplayPrivate.getDisplayState = function(callback) {};

/**
 * @type {ChromeEvent}
 */
chrome.brailleDisplayPrivate.onDisplayStateChanged;

/**
 * @type {ChromeEvent}
 */
chrome.brailleDisplayPrivate.onKeyEvent;

/**
 * @param {ArrayBuffer} cells
 */
chrome.brailleDisplayPrivate.writeDots = function(cells) {};

/**
 * @const
 */
chrome.virtualKeyboardPrivate = {};

/**
 * @typedef {{type: string, charValue: number, keyCode: number,
 *            keyName: string, modifiers: (number|undefined)}}
 */
chrome.virtualKeyboardPrivate.VirtualKeyboardEvent;

/**
 * @param {chrome.virtualKeyboardPrivate.VirtualKeyboardEvent} keyEvent
 * @param {Function=} opt_callback
 */
chrome.virtualKeyboardPrivate.sendKeyEvent =
    function(keyEvent, opt_callback) {};
