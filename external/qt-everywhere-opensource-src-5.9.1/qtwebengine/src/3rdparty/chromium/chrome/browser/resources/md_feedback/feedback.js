// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

var Feedback = {};

/**
 * API invoked by the browser MdFeedbackWebUIMessageHandler to communicate
 * with this UI.
 */
Feedback.UI = class {

  /**
   * Populates the feedback form with data.
   *
   * @param {{email: (string|undefined),
   *          url: (string|undefined)}} data
   * Parameters in data:
   *   email - user's email, if available.
   *   url - url of the tab the user was on before triggering feedback.
   */
  static setData(data) {
    $('container').email = data['email'];
    $('container').url = data['url'];
  }
};

/** API invoked by this UI to communicate with the browser WebUI message
 * handler.
 */
Feedback.BrowserApi = class {
  /**
   * Requests data to initialize the WebUI with.
   * The data will be returned via Feedback.UI.setData.
   */
  static requestData() {
    chrome.send('requestData');
  }
};

window.addEventListener('DOMContentLoaded', function() {
  Feedback.BrowserApi.requestData();
});
