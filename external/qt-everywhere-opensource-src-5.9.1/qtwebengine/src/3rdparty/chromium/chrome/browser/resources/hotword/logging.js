// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

cr.define('hotword', function() {
  'use strict';

  /**
   * Wrapper around console.log allowing debug log message to be enabled during
   * development.
   * @param {...*} varArgs
   */
  function debug(varArgs) {
    if (hotword.DEBUG || window.localStorage['hotword.DEBUG'])
      console.log.apply(console, arguments);
  }

  return {
    DEBUG: false,
    debug: debug
  };
});
