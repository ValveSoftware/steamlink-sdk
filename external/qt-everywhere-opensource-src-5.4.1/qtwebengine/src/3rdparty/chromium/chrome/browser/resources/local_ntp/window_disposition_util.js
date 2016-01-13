// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.


/**
 * @fileoverview Utilities for handling window disposition.
 */


/**
 * The JavaScript button event value for a middle click.
 * @type {number}
 * @const
 */
var MIDDLE_MOUSE_BUTTON = 1;


/**
 * Gets the desired navigation behavior from a event. This function works both
 * with mouse and keyboard events.
 * @param {Event} e The event object.
 * @return {WindowOpenDisposition} The desired navigation behavior.
 */
function getDispositionFromEvent(e) {
  var middleButton = e.button == MIDDLE_MOUSE_BUTTON;
  return chrome.embeddedSearch.newTabPage.getDispositionFromClick(middleButton,
                                                                  e.altKey,
                                                                  e.ctrlKey,
                                                                  e.metaKey,
                                                                  e.shiftKey);
}
