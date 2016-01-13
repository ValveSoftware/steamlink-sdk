// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

define("mojo/public/js/bindings/connection", [
  "mojo/public/js/bindings/router",
], function(router) {

  function Connection(handle, localFactory, remoteFactory) {
    this.router_ = new router.Router(handle);
    this.remote = new remoteFactory(this.router_);
    this.local = new localFactory(this.remote);
    this.router_.setIncomingReceiver(this.local);
  }

  Connection.prototype.close = function() {
    this.router_.close();
    this.router_ = null;
    this.local = null;
    this.remote = null;
  };

  Connection.prototype.encounteredError = function() {
    return this.router_.encounteredError();
  };

  var exports = {};
  exports.Connection = Connection;
  return exports;
});
