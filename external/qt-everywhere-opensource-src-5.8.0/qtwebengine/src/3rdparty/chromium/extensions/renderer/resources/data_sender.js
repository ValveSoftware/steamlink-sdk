// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

define('data_sender', [
    'device/serial/data_stream.mojom',
    'device/serial/data_stream_serialization.mojom',
    'mojo/public/js/core',
    'mojo/public/js/router',
], function(dataStreamMojom, serialization, core, routerModule) {
  /**
   * @module data_sender
   */

  /**
   * A pending send operation.
   * @param {!ArrayBuffer} data The data to be sent.
   * @constructor
   * @alias module:data_sender~PendingSend
   * @private
   */
  function PendingSend(data) {
    /**
     * The data to be sent.
     * @type {ArrayBuffer}
     * @private
     */
    this.data_ = data;
    /**
     * The total length of data to be sent.
     * @type {number}
     * @private
     */
    this.length_ = data.byteLength;
    /**
     * The promise that will be resolved or rejected when this send completes
     * or fails, respectively.
     * @type {!Promise<number>}
     * @private
     */
    this.promise_ = new Promise(function(resolve, reject) {
      /**
       * The callback to call on success.
       * @type {Function}
       * @private
       */
      this.successCallback_ = resolve;
      /**
       * The callback to call with the error on failure.
       * @type {Function}
       * @private
       */
      this.errorCallback_ = reject;
    }.bind(this));
  }

  /**
   * Returns the promise that will be resolved when this operation completes or
   * rejected if an error occurs.
   * @return {!Promise<number>} A promise to the number of bytes sent.
   */
  PendingSend.prototype.getPromise = function() {
    return this.promise_;
  };

  /**
   * Invoked when the DataSink reports that bytes have been sent. Resolves the
   * promise returned by
   * [getPromise()]{@link module:data_sender~PendingSend#getPromise} once all
   * bytes have been reported as sent.
   */
  PendingSend.prototype.reportBytesSent = function() {
    this.successCallback_(this.length_);
  };

  /**
   * Invoked when the DataSink reports an error. Rejects the promise returned by
   * [getPromise()]{@link module:data_sender~PendingSend#getPromise} unless the
   * error occurred after this send, that is, unless numBytes is greater than
   * the nubmer of outstanding bytes.
   * @param {number} numBytes The number of bytes sent.
   * @param {number} error The error reported by the DataSink.
   */
  PendingSend.prototype.reportBytesSentAndError = function(numBytes, error) {
    var e = new Error();
    e.error = error;
    e.bytesSent = numBytes;
    this.errorCallback_(e);
  };

  /**
   * Writes pending data into the data pipe.
   * @param {!DataSink} sink The DataSink to receive the data.
   * @return {!Object} result The send result.
   * @return {boolean} result.completed Whether all of the pending data was
   *     sent.
   */
  PendingSend.prototype.sendData = function(sink) {
    var dataSent = sink.onData(new Uint8Array(this.data_));
    this.data_ = null;
    return dataSent;
  };

  /**
   * A DataSender that sends data to a DataSink.
   * @param {!MojoHandle} sink The handle to the DataSink.
   * @param {number} bufferSize How large a buffer to use for data.
   * @param {number} fatalErrorValue The send error value to report in the
   *     event of a fatal error.
   * @constructor
   * @alias module:data_sender.DataSender
   */
  function DataSender(sink, bufferSize, fatalErrorValue) {
    this.init_(sink, fatalErrorValue);
  }

  /**
   * Closes this DataSender.
   */
  DataSender.prototype.close = function() {
    if (this.shutDown_)
      return;
    this.shutDown_ = true;
    this.router_.close();
    while (this.sendsAwaitingAck_.length) {
      this.sendsAwaitingAck_.pop().reportBytesSentAndError(
          0, this.fatalErrorValue_);
    }
    this.callCancelCallback_();
  };

  /**
   * Initialize this DataSender.
   * @param {!MojoHandle} sink A handle to the DataSink.
   * @param {number} fatalErrorValue The error to dispatch in the event of a
   *     fatal error.
   * @private
   */
  DataSender.prototype.init_ = function(sink, fatalErrorValue) {
    /**
     * The error to be dispatched in the event of a fatal error.
     * @const {number}
     * @private
     */
    this.fatalErrorValue_ = fatalErrorValue;
    /**
     * Whether this DataSender has shut down.
     * @type {boolean}
     * @private
     */
    this.shutDown_ = false;
    /**
     * The [Router]{@link module:mojo/public/js/router.Router} for the
     * connection to the DataSink.
     * @private
     */
    this.router_ = new routerModule.Router(sink);
    /**
     * The connection to the DataSink.
     * @private
     */
    this.sink_ = new dataStreamMojom.DataSink.proxyClass(this.router_);
    /**
     * A queue of sends that have sent their data to the DataSink, but have not
     * been received by the DataSink.
     * @type {!module:data_sender~PendingSend[]}
     * @private
     */
    this.sendsAwaitingAck_ = [];

    /**
     * The callback that will resolve a pending cancel if one is in progress.
     * @type {?Function}
     * @private
     */
    this.pendingCancel_ = null;

    /**
     * The promise that will be resolved when a pending cancel completes if one
     * is in progress.
     * @type {Promise}
     * @private
     */
    this.cancelPromise_ = null;
  };

  /**
   * Serializes this DataSender.
   * This will cancel any sends in progress before the returned promise
   * resolves.
   * @return {!Promise<SerializedDataSender>} A promise that will resolve to
   *     the serialization of this DataSender. If this DataSender has shut down,
   *     the promise will resolve to null.
   */
  DataSender.prototype.serialize = function() {
    if (this.shutDown_)
      return Promise.resolve(null);

    var readyToSerialize = Promise.resolve();
    if (this.sendsAwaitingAck_.length) {
      if (this.pendingCancel_)
        readyToSerialize = this.cancelPromise_;
      else
        readyToSerialize = this.cancel(this.fatalErrorValue_);
    }
    return readyToSerialize.then(function() {
      var serialized = new serialization.SerializedDataSender();
      serialized.sink = this.router_.connector_.handle_;
      serialized.fatal_error_value = this.fatalErrorValue_;
      this.router_.connector_.handle_ = null;
      this.router_.close();
      this.shutDown_ = true;
      return serialized;
    }.bind(this));
  };

  /**
   * Deserializes a SerializedDataSender.
   * @param {SerializedDataSender} serialized The serialized DataSender.
   * @return {!DataSender} The deserialized DataSender.
   */
  DataSender.deserialize = function(serialized) {
    var sender = $Object.create(DataSender.prototype);
    sender.deserialize_(serialized);
    return sender;
  };

  /**
   * Deserializes a SerializedDataSender into this DataSender.
   * @param {SerializedDataSender} serialized The serialized DataSender.
   * @private
   */
  DataSender.prototype.deserialize_ = function(serialized) {
    if (!serialized) {
      this.shutDown_ = true;
      return;
    }
    this.init_(serialized.sink, serialized.fatal_error_value,
               serialized.buffer_size);
  };

  /**
   * Sends data to the DataSink.
   * @return {!Promise<number>} A promise to the number of bytes sent. If an
   *     error occurs, the promise will reject with an Error object with a
   *     property error containing the error code.
   * @throws Will throw if this has encountered a fatal error or a cancel is in
   *     progress.
   */
  DataSender.prototype.send = function(data) {
    if (this.shutDown_)
      throw new Error('DataSender has been closed');
    if (this.pendingCancel_)
      throw new Error('Cancel in progress');
    var send = new PendingSend(data);
    this.sendsAwaitingAck_.push(send);
    send.sendData(this.sink_).then(this.reportBytesSentAndError.bind(this));
    return send.getPromise();
  };

  /**
   * Requests the cancellation of any in-progress sends. Calls to
   * [send()]{@link module:data_sender.DataSender#send} will fail until the
   * cancel has completed.
   * @param {number} error The error to report for cancelled sends.
   * @return {!Promise} A promise that will resolve when the cancel completes.
   * @throws Will throw if this has encountered a fatal error or another cancel
   *     is in progress.
   */
  DataSender.prototype.cancel = function(error) {
    if (this.shutDown_)
      throw new Error('DataSender has been closed');
    if (this.pendingCancel_)
      throw new Error('Cancel already in progress');
    if (this.sendsAwaitingAck_.length == 0)
      return Promise.resolve();

    this.sink_.cancel(error);
    this.cancelPromise_ = new Promise(function(resolve) {
      this.pendingCancel_ = resolve;
    }.bind(this));
    return this.cancelPromise_;
  };

  /**
   * Calls and clears the pending cancel callback if one is pending.
   * @private
   */
  DataSender.prototype.callCancelCallback_ = function() {
    if (this.pendingCancel_) {
      this.cancelPromise_ = null;
      this.pendingCancel_();
      this.pendingCancel_ = null;
    }
  };

  /**
   * Invoked by the DataSink to report that data has been successfully sent.
   * @private
   */
  DataSender.prototype.reportBytesSent = function() {
    var result = this.sendsAwaitingAck_[0].reportBytesSent();
    this.sendsAwaitingAck_.shift();

    // A cancel is completed when all of the sends that were in progress have
    // completed or failed. This is the case where all sends complete
    // successfully.
    if (this.sendsAwaitingAck_.length == 0)
      this.callCancelCallback_();
  };

  /**
   * Invoked by the DataSink to report an error in sending data.
   * @param {number} numBytes The number of bytes sent.
   * @param {number} error The error reported by the DataSink.
   * @private
   */
  DataSender.prototype.reportBytesSentAndError = function(result) {
    var numBytes = result.bytes_sent;
    var error = result.error;
    if (!error) {
      this.reportBytesSent();
      return;
    }
    var result =
        this.sendsAwaitingAck_[0].reportBytesSentAndError(numBytes, error);
    this.sendsAwaitingAck_.shift();
    if (this.sendsAwaitingAck_.length)
      return;
    this.callCancelCallback_();
    this.sink_.clearError();
  };

  return {DataSender: DataSender};
});
