// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

define('keep_alive', [
    'content/public/renderer/frame_interfaces',
    'extensions/common/mojo/keep_alive.mojom',
    'mojo/public/js/core',
], function(frameInterfaces, mojom, core) {

  /**
   * An object that keeps the background page alive until closed.
   * @constructor
   * @alias module:keep_alive~KeepAlive
   */
  function KeepAlive() {
    /**
     * The handle to the keep-alive object in the browser.
     * @type {!MojoHandle}
     * @private
     */
    this.handle_ = frameInterfaces.getInterface(mojom.KeepAlive.name);
  }

  /**
   * Removes this keep-alive.
   */
  KeepAlive.prototype.close = function() {
    core.close(this.handle_);
  };

  var exports = {};

  return {
    /**
     * Creates a keep-alive.
     * @return {!module:keep_alive~KeepAlive} A new keep-alive.
     */
    createKeepAlive: function() { return new KeepAlive(); }
  };
});
