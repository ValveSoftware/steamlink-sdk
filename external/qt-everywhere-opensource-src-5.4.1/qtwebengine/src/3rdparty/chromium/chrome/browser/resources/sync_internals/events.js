// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

(function() {
'use strict';
var toggleDisplay = function(event) {
  var originatingButton = event.target;
  var detailsNode = originatingButton.parentNode.getElementsByClassName(
      'details')[0];

  if (detailsNode.getAttribute('hidden') != null) {
    detailsNode.removeAttribute('hidden');
  } else {
    detailsNode.setAttribute('hidden', 'hidden');
  }
}

var syncEvents = $('sync-events');

var entries = chrome.sync.log.entries;
var displaySyncEvents = function() {
  var eventTemplateContext = {
    eventList: entries,
  };
  var context = new JsEvalContext(eventTemplateContext);
  jstProcess(context, syncEvents);
}

syncEvents.addEventListener('click', toggleDisplay);
chrome.sync.log.addEventListener('append', function(event) {
  displaySyncEvents();
});
})();
