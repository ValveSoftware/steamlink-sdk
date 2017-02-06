// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

  /**
   * Takes the |moduleListData| input argument which represents data about
   * the currently available modules and populates the html jstemplate
   * with that data. It expects an object structure like the above.
   * @param {Object} moduleListData Information about available modules
   */
  function renderTemplate(moduleListData) {
    var input = new JsEvalContext(moduleListData);
    var output = $('voice-search-info-template');
    jstProcess(input, output);
  }

  /**
   * Asks the C++ VoiceSearchUIDOMHandler to get details about voice search and
   * return the data in returnVoiceSearchInfo() (below).
   */
  function requestVoiceSearchInfo() {
    chrome.send('requestVoiceSearchInfo');
  }

  /**
   * Called by the WebUI to re-populate the page with data representing the
   * current state of voice search.
   * @param {Object} moduleListData Information about available modules.
   */
  function returnVoiceSearchInfo(moduleListData) {
    $('loading-message').hidden = true;
    $('body-container').hidden = false;
    renderTemplate(moduleListData);
  }

  // Get data and have it displayed upon loading.
  document.addEventListener('DOMContentLoaded', requestVoiceSearchInfo);
