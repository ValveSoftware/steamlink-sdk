// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// This module implements the public-facing API functions for the
// <extensionview> tag.

var ExtensionViewInternal =
    require('extensionViewInternal').ExtensionViewInternal;
var ExtensionViewImpl = require('extensionView').ExtensionViewImpl;
var ExtensionViewConstants =
    require('extensionViewConstants').ExtensionViewConstants;

// An array of <extensionview>'s public-facing API methods.
var EXTENSION_VIEW_API_METHODS = [
  // Loads the given src into extensionview. Must be called every time the
  // the extensionview should load a new page. This is the only way to set
  // the extension and src attributes. Returns a promise indicating whether
  // or not load was successful.
  'load'
];

// -----------------------------------------------------------------------------
// Custom API method implementations.

ExtensionViewImpl.prototype.load = function(src) {
  return new Promise(function(resolve, reject) {
    this.loadQueue.push({src: src, resolve: resolve, reject: reject});
    this.loadNextSrc();
  }.bind(this))
  .then(function onLoadResolved() {
    this.pendingLoad = null;
    this.loadNextSrc();
  }.bind(this), function onLoadRejected() {
    this.pendingLoad = null;
    this.loadNextSrc();
    reject('Failed to load.');
  }.bind(this));
};

// -----------------------------------------------------------------------------

ExtensionViewImpl.getApiMethods = function() {
  return EXTENSION_VIEW_API_METHODS;
};
