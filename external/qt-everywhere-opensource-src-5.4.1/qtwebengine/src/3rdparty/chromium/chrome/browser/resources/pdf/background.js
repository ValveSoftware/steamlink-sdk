// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

(function() {
  'use strict';

  /**
   * Keep a stack of stream details for requests. These are pushed onto the
   * stack as requests come in and popped off the stack as they are handled by a
   * renderer.
   * TODO(raymes): This is probably racy for multiple requests. We could
   * associate an ID with the request but this code will probably change
   * completely when MIME type handling is improved.
   */
  var streamsCache = [];

  window.popStreamDetails = function() {
    if (streamsCache.length > 0)
      return streamsCache.pop();
  };

  chrome.streamsPrivate.onExecuteMimeTypeHandler.addListener(
    function(streamDetails) {
      // TODO(raymes): Currently this doesn't work with embedded PDFs (it
      // causes the entire frame to navigate). Also work out how we can
      // mask the URL with the URL of the PDF.
      streamsCache.push(streamDetails);
      chrome.tabs.update(streamDetails.tabId, {url: 'index.html'});
    }
  );

}());
