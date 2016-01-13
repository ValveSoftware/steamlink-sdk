// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

/**
 * @fileoverview Initializes the injected content script on the document.
 *
 * NOTE(deboer): This file will automatically initialize ChromeVox.  If you can
 * control when ChromeVox starts, consider using cvox.InitGlobals instead.
 *
 */

goog.provide('cvox.ChromeVoxInit');

goog.require('cvox.ChromeVox');
goog.require('cvox.HostFactory');
goog.require('cvox.InitGlobals');

cvox.ChromeVox.initTimeout_ = 100;

/**
 * Call the init function later, safely
 * @private
 */
cvox.ChromeVox.recallInit_ = function(reason) {
  window.console.log(reason +
                     ' Will try again in ' +
                     cvox.ChromeVox.initTimeout_ + 'ms');
  window.setTimeout(cvox.ChromeVox.initDocument, cvox.ChromeVox.initTimeout_);
  cvox.ChromeVox.initTimeout_ *= 2;
};


/**
 * Initializes cvox.ChromeVox when the document is ready.
 */
cvox.ChromeVox.initDocument = function() {
  if (!document.body) {
    cvox.ChromeVox.recallInit_('ChromeVox not starting on unloaded page: ' +
        document.location.href + '.');
    return;
  }

  // Setup globals
  cvox.ChromeVox.host = cvox.HostFactory.getHost();

  if (!cvox.ChromeVox.host.ttsLoaded()) {
    cvox.ChromeVox.recallInit_('ChromeVox not starting; waiting for TTS. ' +
                               document.location.href + '.');
    return;
  }

  window.console.log('Starting ChromeVox.');

  cvox.InitGlobals.initGlobals();

  // Add a global function to disable this instance of ChromeVox.
  // There is a scenario where two copies of the content script can get
  // loaded into the same tab on browser startup - one automatically
  // and one because the background page injects the content script into
  // every tab on startup. This allows the background page to first deactivate
  // any existing copy of the content script (if any) before loading it again,
  // otherwise there can be duplicate event listeners.
  window.disableChromeVox = function() {
    cvox.ChromeVox.host.killChromeVox();
  };
};


/**
 * Reinitialize ChromeVox, if the extension is disabled and then enabled
 * again, but our injected page script has remained.
 */
cvox.ChromeVox.reinit = function() {
  cvox.ChromeVox.host.reinit();
  cvox.ChromeVox.initDocument();
};

if (!COMPILED) {
  // NOTE(deboer): This is called when this script is loaded, automatically
  // starting ChromeVox. If this isn't the uncompiled script, it will be
  // called in loader.js.
  cvox.ChromeVox.initDocument();
}
