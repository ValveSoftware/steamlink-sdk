// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

/**
 * @fileoverview U2F gnubbyd background page
 */

'use strict';

// Singleton tracking available devices.
var gnubbies = new Gnubbies();
llUsbGnubby.register(gnubbies);
// Only include HID support if it's available in this browser.
if (chrome.hid) {
  llHidGnubby.register(gnubbies);
}

var GNUBBY_FACTORY = new UsbGnubbyFactory(gnubbies);
var SIGN_HELPER_FACTORY = new UsbSignHelperFactory(GNUBBY_FACTORY);
var ENROLL_HELPER_FACTORY = new UsbEnrollHelperFactory(GNUBBY_FACTORY);

/**
 * @param {boolean} toleratesMultipleResponses Whether the web page can handle
 *     multiple responses given to its sendResponse callback.
 * @param {Object} request Request object
 * @param {MessageSender} sender Sender frame
 * @param {Function} sendResponse Response callback
 * @return {?Closeable} Optional handler object that should be closed when port
 *     closes
 */
function handleWebPageRequest(toleratesMultipleResponses, request, sender,
    sendResponse) {
  switch (request.type) {
    case GnubbyMsgTypes.ENROLL_WEB_REQUEST:
      return handleEnrollRequest(ENROLL_HELPER_FACTORY, sender, request,
          sendResponse, toleratesMultipleResponses);

    case GnubbyMsgTypes.SIGN_WEB_REQUEST:
      return handleSignRequest(SIGN_HELPER_FACTORY, sender, request,
          sendResponse, toleratesMultipleResponses);

    default:
      var response = formatWebPageResponse(
          GnubbyMsgTypes.ENROLL_WEB_REPLY, GnubbyCodeTypes.BAD_REQUEST);
      sendResponse(response);
      return null;
  }
}

// Message handler for requests coming from web pages.
function messageHandler(toleratesMultipleResponses, request, sender,
    sendResponse) {
  console.log(UTIL_fmt('onMessageExternal listener: ' + request.type));
  console.log(UTIL_fmt('request'));
  console.log(request);
  console.log(UTIL_fmt('sender'));
  console.log(sender);

  handleWebPageRequest(toleratesMultipleResponses, request, sender,
      sendResponse);
  return true;
}

// Listen to web pages.
chrome.runtime.onMessageExternal.addListener(messageHandler.bind(null, false));

// List to connection events, and wire up a message handler on the port.
chrome.runtime.onConnectExternal.addListener(function(port) {
  var closeable;
  port.onMessage.addListener(function(request) {
    var toleratesMultipleResponses = true;
    closeable = handleWebPageRequest(toleratesMultipleResponses, request,
        port.sender,
        function(response) {
          response['requestId'] = request['requestId'];
          port.postMessage(response);
        });
  });
  port.onDisconnect.addListener(function() {
    if (closeable) {
      closeable.close();
    }
  });
});
