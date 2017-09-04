// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// This Polymer element is used as a container for all the feedback
// elements. Based on a number of factors, it determines which elements
// to show and what will be submitted to the feedback servers.
Polymer({
  is: 'feedback-container',

  properties: {
    /**
     * The user's email, if available.
     * @type {string|undefined}
     */
    email: {
      type: String,
    },

    /**
     * The URL of the page the user was on before sending feedback.
     * @type {string|undefined}
     */
    url: {
      type: String,
    },
  },

  ready: function() {
    // Retrieves the feedback privacy note text, if it exists. On non-official
    // branded builds, the string is not defined.
    this.$.privacyNote.innerHTML =
        loadTimeData.valueExists('privacyNote') ?
            loadTimeData.getString('privacyNote') : '';
  },
});
