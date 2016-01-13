// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

define("mojo/public/js/bindings/router", [
  "mojo/public/js/bindings/codec",
  "mojo/public/js/bindings/connector",
], function(codec, connector) {

  function Router(handle) {
    this.connector_ = new connector.Connector(handle);
    this.incomingReceiver_ = null;
    this.nextRequestID_ = 0;
    this.responders_ = {};

    this.connector_.setIncomingReceiver({
        accept: this.handleIncomingMessage_.bind(this),
    });
    this.connector_.setErrorHandler({
        onError: this.handleConnectionError_.bind(this),
    });
  }

  Router.prototype.close = function() {
    this.responders_ = {};  // Drop any responders.
    this.connector_.close();
  };

  Router.prototype.accept = function(message) {
    this.connector_.accept(message);
  };

  Router.prototype.reject = function(message) {
    // TODO(mpcomplete): no way to trasmit errors over a Connection.
  };

  Router.prototype.acceptWithResponder = function(message, responder) {
    // Reserve 0 in case we want it to convey special meaning in the future.
    var requestID = this.nextRequestID_++;
    if (requestID == 0)
      requestID = this.nextRequestID_++;

    message.setRequestID(requestID);
    var result = this.connector_.accept(message);

    this.responders_[requestID] = responder;

    // TODO(mpcomplete): accept should return a Promise too, maybe?
    if (result)
      return Promise.resolve();
    return Promise.reject(Error("Connection error"));
  };

  Router.prototype.setIncomingReceiver = function(receiver) {
    this.incomingReceiver_ = receiver;
  };

  Router.prototype.encounteredError = function() {
    return this.connector_.encounteredError();
  };

  Router.prototype.handleIncomingMessage_ = function(message) {
    var flags = message.getFlags();
    if (flags & codec.kMessageExpectsResponse) {
      if (this.incomingReceiver_) {
        this.incomingReceiver_.acceptWithResponder(message, this);
      } else {
        // If we receive a request expecting a response when the client is not
        // listening, then we have no choice but to tear down the pipe.
        this.close();
      }
    } else if (flags & codec.kMessageIsResponse) {
      var reader = new codec.MessageReader(message);
      var requestID = reader.requestID;
      var responder = this.responders_[requestID];
      delete this.responders_[requestID];
      responder.accept(message);
    } else {
      if (this.incomingReceiver_)
        this.incomingReceiver_.accept(message);
    }
  };

  Router.prototype.handleConnectionError_ = function(result) {
    for (var each in this.responders_)
      this.responders_[each].reject(result);
    this.close();
  };

  var exports = {};
  exports.Router = Router;
  return exports;
});
