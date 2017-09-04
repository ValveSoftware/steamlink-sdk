// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

/** @type {string}
 * @const
 */
var FEEDBACK_LANDING_PAGE =
    'https://support.google.com/chrome/go/feedback_confirmation';

/**
 * Feedback flow defined in feedback_private.idl.
 * @enum {string}
 */
var FeedbackFlow = {
  REGULAR: 'regular',  // Flow in a regular user session.
  LOGIN: 'login',       // Flow on the login screen.
  SHOW_SRT_PROMPT: 'showSrtPrompt'  // Prompt user to try Software Removal Tool
};

/**
 * The status of sending the feedback report as defined in feedback_private.idl.
 * @enum {string}
 */
var ReportStatus = {
  SUCCESS: 'success',
  DELAYED: 'delayed'
};
