// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

cr.define('extension_info', function() {
  'use strict';

  /**
   * Initialize the info popup.
   */
  function load() {
    $('extension-item').style.backgroundImage =
        'url(' + loadTimeData.getString('icon') + ')';
    $('extension-title-running').textContent =
        loadTimeData.getStringF('isRunning', loadTimeData.getString('name'));
  }

  return {
    load: load
  };
});

window.addEventListener('DOMContentLoaded', extension_info.load);
