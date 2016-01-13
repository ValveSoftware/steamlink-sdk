// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

define("mojo/public/js/bindings/connector", [
  "mojo/public/js/bindings/codec",
  "mojo/public/js/bindings/core",
  "mojo/public/js/bindings/support",
], function(codec, core, support) {

  function Connector(handle) {
    this.handle_ = handle;
    this.dropWrites_ = false;
    this.error_ = false;
    this.incomingReceiver_ = null;
    this.readWaitCookie_ = null;
    this.errorHandler_ = null;

    this.waitToReadMore_();
  }

  Connector.prototype.close = function() {
    if (this.readWaitCookie_) {
      support.cancelWait(this.readWaitCookie_);
      this.readWaitCookie_ = null;
    }
    if (this.handle_ != null) {
      core.close(this.handle_);
      this.handle_ = null;
    }
  };

  Connector.prototype.accept = function(message) {
    if (this.error_)
      return false;

    if (this.dropWrites_)
      return true;

    var result = core.writeMessage(this.handle_,
                                   new Uint8Array(message.buffer.arrayBuffer),
                                   message.handles,
                                   core.WRITE_MESSAGE_FLAG_NONE);
    switch (result) {
      case core.RESULT_OK:
        // The handles were successfully transferred, so we don't own them
        // anymore.
        message.handles = [];
        break;
      case core.RESULT_FAILED_PRECONDITION:
        // There's no point in continuing to write to this pipe since the other
        // end is gone. Avoid writing any future messages. Hide write failures
        // from the caller since we'd like them to continue consuming any
        // backlog of incoming messages before regarding the message pipe as
        // closed.
        this.dropWrites_ = true;
        break;
      default:
        // This particular write was rejected, presumably because of bad input.
        // The pipe is not necessarily in a bad state.
        return false;
    }
    return true;
  };

  Connector.prototype.setIncomingReceiver = function(receiver) {
    this.incomingReceiver_ = receiver;
  };

  Connector.prototype.setErrorHandler = function(handler) {
    this.errorHandler_ = handler;
  };

  Connector.prototype.encounteredError = function() {
    return this.error_;
  };

  Connector.prototype.waitToReadMore_ = function() {
    this.readWaitCookie_ = support.asyncWait(this.handle_,
                                             core.HANDLE_SIGNAL_READABLE,
                                             this.readMore_.bind(this));
  };

  Connector.prototype.readMore_ = function(result) {
    for (;;) {
      var read = core.readMessage(this.handle_,
                                  core.READ_MESSAGE_FLAG_NONE);
      if (read.result == core.RESULT_SHOULD_WAIT) {
        this.waitToReadMore_();
        return;
      }
      if (read.result != core.RESULT_OK) {
        this.error_ = true;
        if (this.errorHandler_)
          this.errorHandler_.onError(read.result);
        return;
      }
      var buffer = new codec.Buffer(read.buffer);
      var message = new codec.Message(buffer, read.handles);
      if (this.incomingReceiver_) {
          this.incomingReceiver_.accept(message);
      }
    }
  };

  var exports = {};
  exports.Connector = Connector;
  return exports;
});
