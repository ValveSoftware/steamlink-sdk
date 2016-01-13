// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Common external variables when compiling ChromeVox background code

var localStorage = {};

/**
 * @type {Object}
 */
chrome.systemPrivate = {};

/**
 * @param {function(!Object)} callback
 */
chrome.systemPrivate.getUpdateStatus = function(callback) {};

/** @type {ChromeEvent} */
chrome.systemPrivate.onBrightnessChanged;

/** @type ChromeEvent */
chrome.systemPrivate.onVolumeChanged;

/** @type ChromeEvent */
chrome.systemPrivate.onScreenUnlocked;

/** @type ChromeEvent */
chrome.systemPrivate.onWokeUp;


/**
 * @type {Object}
 */
chrome.accessibilityPrivate = {};

/**
 * @param {boolean} on
 */
chrome.accessibilityPrivate.setAccessibilityEnabled = function(on) {};

/**
 * @param {boolean} on
 */
chrome.accessibilityPrivate.setNativeAccessibilityEnabled = function(on) {
};

/**
 * @param {number} tabId
 * @param {function(Array.<!Object>)} callback
 */
chrome.accessibilityPrivate.getAlertsForTab =
    function(tabId, callback) {};

/** @type ChromeEvent */
chrome.accessibilityPrivate.onWindowOpened;

/** @type ChromeEvent */
chrome.accessibilityPrivate.onWindowClosed;

/** @type ChromeEvent */
chrome.accessibilityPrivate.onMenuOpened;

/** @type ChromeEvent */
chrome.accessibilityPrivate.onMenuClosed;

/** @type ChromeEvent */
chrome.accessibilityPrivate.onControlFocused;

/** @type ChromeEvent */
chrome.accessibilityPrivate.onControlAction;

/** @type ChromeEvent */
chrome.accessibilityPrivate.onControlHover;

/** @type ChromeEvent */
chrome.accessibilityPrivate.onTextChanged;

/** @type ChromeEvent */
chrome.accessibilityPrivate.onChromeVoxLoadStateChanged;
/** @type {function()} */
chrome.accessibilityPrivate.onChromeVoxLoadStateChanged.destroy_;


/**
 * @type {Object}
 */
chrome.experimental = {};

/**
 * @type {Object}
 *
 * TODO(dmazzoni): Remove after the stable version of Chrome no longer
 * has the experimental accessibility API.
 */
chrome.experimental.accessibility = chrome.accessibilityPrivate;


/**
 *
 */
chrome.app.getDetails = function() {};

/** @constructor */
var AccessibilityObject = function() {};
/** @type {string} */
AccessibilityObject.prototype.type;
/** @type {string} */
AccessibilityObject.prototype.name;
/** @type {Object} */
AccessibilityObject.prototype.details;
/** @type {string} */
AccessibilityObject.prototype.details.value;
/** @type {number} */
AccessibilityObject.prototype.details.selectionStart;
/** @type {number} */
AccessibilityObject.prototype.details.selectionEnd;
/** @type {number} */
AccessibilityObject.prototype.details.itemCount;
/** @type {number} */
AccessibilityObject.prototype.details.itemIndex;
