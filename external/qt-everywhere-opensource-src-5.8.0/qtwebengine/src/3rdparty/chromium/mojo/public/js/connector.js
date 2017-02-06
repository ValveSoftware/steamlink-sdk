// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

define("mojo/public/js/connector", [
  "mojo/public/js/buffer",
  "mojo/public/js/codec",
  "mojo/public/js/core",
  "mojo/public/js/support",
], function(buffer, codec, core, support) {

  function Connector(handle) {
    if (!core.isHandle(handle))
      throw new Error("Connector: not a handle " + handle);
    this.handle_ = handle;
    this.dropWrites_ = false;
    this.error_ = false;
    this.incomingReceiver_ = null;
    this.readWatcher_ = null;
    this.errorHandler_ = null;

    if (handle) {
      this.readWatcher_ = support.watch(handle,
                                        core.HANDLE_SIGNAL_READABLE,
                                        this.readMore_.bind(this));
    }
  }

  Connector.prototype.close = function() {
    if (this.readWatcher_) {
      support.cancelWatch(this.readWatcher_);
      this.readWatcher_ = null;
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

  Connector.prototype.readMore_ = function(result) {
    for (;;) {
      var read = core.readMessage(this.handle_,
                                  core.READ_MESSAGE_FLAG_NONE);
      if (this.handle_ == null) // The connector has been closed.
        return;
      if (read.result == core.RESULT_SHOULD_WAIT)
        return;
      if (read.result != core.RESULT_OK) {
        this.error_ = true;
        if (this.errorHandler_)
          this.errorHandler_.onError(read.result);
        return;
      }
      var messageBuffer = new buffer.Buffer(read.buffer);
      var message = new codec.Message(messageBuffer, read.handles);
      if (this.incomingReceiver_) {
          this.incomingReceiver_.accept(message);
      }
    }
  };

  // The TestConnector subclass is only intended to be used in unit tests. It
  // doesn't automatically listen for input messages. Instead, you need to
  // call waitForNextMessage to block and wait for the next incoming message.
  function TestConnector(handle) {
    Connector.call(this, handle);
  }

  TestConnector.prototype = Object.create(Connector.prototype);

  TestConnector.prototype.waitForNextMessage = function() {
    var wait = core.wait(this.handle_, core.HANDLE_SIGNAL_READABLE,
                         core.DEADLINE_INDEFINITE);
    this.readMore_(wait.result);
  }

  var exports = {};
  exports.Connector = Connector;
  exports.TestConnector = TestConnector;
  return exports;
});
