// Copyright (c) 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

/**
 * Takes the |nearbyUrlsData| input argument which holds metadata for web pages
 * broadcast by nearby devices.
 * @param {Object} nearbyUrlsData Information about web pages broadcast by
 *      nearby devices
 */
function renderTemplate(nearbyUrlsData) {
  // This is the javascript code that processes the template:
  jstProcess(new JsEvalContext(nearbyUrlsData), $('physicalWebTemplate'));
}

function requestNearbyURLs() {
  chrome.send('requestNearbyURLs');
}

function physicalWebItemClicked(index) {
  chrome.send('physicalWebItemClicked', [index]);
}

function returnNearbyURLs(nearbyUrlsData) {
  var bodyContainer = $('body-container');
  renderTemplate(nearbyUrlsData);

  bodyContainer.style.visibility = 'visible';
}

document.addEventListener('DOMContentLoaded', requestNearbyURLs);
