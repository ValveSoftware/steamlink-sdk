// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

//
// <window-controls> shadow element implementation.
//

var chrome = requireNative('chrome').GetChrome();
var forEach = require('utils').forEach;
var addTagWatcher = require('tagWatcher').addTagWatcher;
var appWindow = require('app.window');
var getHtmlTemplate =
  requireNative('app_window_natives').GetWindowControlsHtmlTemplate;

/**
 * @constructor
 */
function WindowControls(node) {
  this.node_ = node;
  this.shadowRoot_ = this.createShadowRoot_(node);
  this.setupWindowControls_();
}

/**
 * @private
 */
WindowControls.prototype.template_element = null;

/**
 * @private
 */
WindowControls.prototype.createShadowRoot_ = function(node) {
  // Initialize |template| from HTML template resource and cache result.
  var template = WindowControls.prototype.template_element;
  if (!template) {
    var element = document.createElement('div');
    element.innerHTML = getHtmlTemplate();
    WindowControls.prototype.template_element = element.firstChild;
    template = WindowControls.prototype.template_element;
  }
  // Create shadow root element with template clone as first child.
  var shadowRoot = node.createShadowRoot();
  shadowRoot.appendChild(template.content.cloneNode(true));
  return shadowRoot;
}

/**
 * @private
 */
WindowControls.prototype.setupWindowControls_ = function() {
  var self = this;
  this.shadowRoot_.querySelector("#close-control").addEventListener('click',
      function(e) {
        chrome.app.window.current().close();
      });

  this.shadowRoot_.querySelector("#maximize-control").addEventListener('click',
      function(e) {
        self.maxRestore_();
      });
}

/**
 * @private
 * Restore or maximize depending on current state
 */
WindowControls.prototype.maxRestore_ = function() {
  if (chrome.app.window.current().isMaximized()) {
    chrome.app.window.current().restore();
  } else {
    chrome.app.window.current().maximize();
  }
}

addTagWatcher('WINDOW-CONTROLS', function(addedNode) {
  new WindowControls(addedNode);
});
