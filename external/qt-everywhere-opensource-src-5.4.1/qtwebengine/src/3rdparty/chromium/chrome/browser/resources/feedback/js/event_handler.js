// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

/**
 * @type {number}
 * @const
 */
var FEEDBACK_WIDTH = 500;
/**
 * @type {number}
 * @const
 */
var FEEDBACK_HEIGHT = 585;

var initialFeedbackInfo = null;

var whitelistedExtensionIds = [
  'bpmcpldpdmajfigpchkicefoigmkfalc', // QuickOffice
  'ehibbfinohgbchlgdbfpikodjaojhccn', // QuickOffice
  'gbkeegbaiigmenfmjfclcdgdpimamgkj', // QuickOffice
  'efjnaogkjbogokcnohkmnjdojkikgobo', // G+ Photos
  'ebpbnabdhheoknfklmpddcdijjkmklkp', // G+ Photos
  'endkpmfloggdajndjpoekmkjnkolfdbf', // Feedback Extension
  'mlocfejafidcakdddnndjdngfmncfbeg', // Connectivity Diagnostics
  'ganomidahfnpdchomfgdoppjmmedlhia', // Connectivity Diagnostics
  'eemlkeanncmjljgehlbplemhmdmalhdc', // Connectivity Diagnostics
  'kodldpbjkkmmnilagfdheibampofhaom', // Connectivity Diagnostics
  'kkebgepbbgbcmghedmmdfcbdcodlkngh', // Chrome OS Recovery Tool
  'jndclpdbaamdhonoechobihbbiimdgai', // Chrome OS Recovery Tool
  'ljoammodoonkhnehlncldjelhidljdpi', // GetHelp app.
  'ljacajndfccfgnfohlgkdphmbnpkjflk', // Chrome Remote Desktop Dev
  'gbchcmhmhahfdphkhkmpfmihenigjmpp', // Chrome Remote Desktop Stable
  'odkaodonbgfohohmklejpjiejmcipmib', // Chrome Remote Desktop QA
  'dokpleeekgeeiehdhmdkeimnkmoifgdd', // Chrome Remote Desktop QA backup
  'ajoainacpilcemgiakehflpbkbfipojk', // Chrome Remote Desktop Apps V2
  'llohocloplkbhgcfnplnoficdkiechcn', // Play Movies Dev
  'icljpnebmoleodmchaaajbkpoipfoahp', // Play Movies Nightly
  'mjekoljodoiapgkggnlmbecndfpbbcch', // Play Movies Beta
  'gdijeikdkaembjbdobgfkoidjkpbmlkd', // Play Movies Stable
  'knipolnnllmklapflnccelgolnpehhpl', // Hangouts Extension
];

/**
 * Function to determine whether or not a given extension id is whitelisted to
 * invoke the feedback UI.
 * @param {string} id the id of the sender extension.
 * @return {boolean} Whether or not this sender is whitelisted.
 */
function senderWhitelisted(id) {
  return id && whitelistedExtensionIds.indexOf(id) != -1;
}

/**
 * Callback which gets notified once our feedback UI has loaded and is ready to
 * receive its initial feedback info object.
 * @param {Object} request The message request object.
 * @param {Object} sender The sender of the message.
 * @param {function(Object)} sendResponse Callback for sending a response.
 */
function feedbackReadyHandler(request, sender, sendResponse) {
  if (request.ready) {
    // TODO(rkc):  Remove logging once crbug.com/284662 is closed.
    console.log('FEEDBACK_DEBUG: FeedbackUI Ready. Sending feedbackInfo.');
    chrome.runtime.sendMessage(
        {sentFromEventPage: true, data: initialFeedbackInfo});
  }
}


/**
 * Callback which gets notified if another extension is requesting feedback.
 * @param {Object} request The message request object.
 * @param {Object} sender The sender of the message.
 * @param {function(Object)} sendResponse Callback for sending a response.
 */
function requestFeedbackHandler(request, sender, sendResponse) {
  if (request.requestFeedback && senderWhitelisted(sender.id))
    startFeedbackUI(request.feedbackInfo);
}

/**
 * Callback which starts up the feedback UI.
 * @param {Object} feedbackInfo Object containing any initial feedback info.
 */
function startFeedbackUI(feedbackInfo) {
  initialFeedbackInfo = feedbackInfo;
  // TODO(rkc):  Remove logging once crbug.com/284662 is closed.
  console.log('FEEDBACK_DEBUG: Received onFeedbackRequested. Creating Window.');
  chrome.app.window.create('html/default.html', {
      frame: 'none',
      id: 'default_window',
      width: FEEDBACK_WIDTH,
      height: FEEDBACK_HEIGHT,
      hidden: true,
      resizable: false },
      function(appWindow) {});
}

chrome.runtime.onMessage.addListener(feedbackReadyHandler);
chrome.runtime.onMessageExternal.addListener(requestFeedbackHandler);
chrome.feedbackPrivate.onFeedbackRequested.addListener(startFeedbackUI);
