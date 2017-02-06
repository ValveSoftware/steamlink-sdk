// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

define('data_receiver', [
    'device/serial/data_stream.mojom',
    'device/serial/data_stream_serialization.mojom',
    'mojo/public/js/core',
    'mojo/public/js/router',
], function(dataStream, serialization, core, router) {
  /**
   * @module data_receiver
   */

  /**
   * A pending receive operation.
   * @constructor
   * @alias module:data_receiver~PendingReceive
   * @private
   */
  function PendingReceive() {
    /**
     * The promise that will be resolved or rejected when this receive completes
     * or fails, respectively.
     * @type {!Promise<ArrayBuffer>}
     * @private
     */
    this.promise_ = new Promise(function(resolve, reject) {
      /**
       * The callback to call with the data received on success.
       * @type {Function}
       * @private
       */
      this.dataCallback_ = resolve;
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
   * @return {Promise<ArrayBuffer>} A promise to the data received.
   */
  PendingReceive.prototype.getPromise = function() {
    return this.promise_;
  };

  /**
   * Dispatches received data to the promise returned by
   * [getPromise]{@link module:data_receiver.PendingReceive#getPromise}.
   * @param {!ArrayBuffer} data The data to dispatch.
   */
  PendingReceive.prototype.dispatchData = function(data) {
    this.dataCallback_(data);
  };

  /**
   * Dispatches an error if the offset of the error has been reached.
   * @param {!PendingReceiveError} error The error to dispatch.
   * @param {number} bytesReceived The number of bytes that have been received.
   */
  PendingReceive.prototype.dispatchError = function(error) {
    if (error.queuePosition > 0)
      return false;

    var e = new Error();
    e.error = error.error;
    this.errorCallback_(e);
    return true;
  };

  /**
   * Unconditionally dispatches an error.
   * @param {number} error The error to dispatch.
   */
  PendingReceive.prototype.dispatchFatalError = function(error) {
    var e = new Error();
    e.error = error;
    this.errorCallback_(e);
  };

  /**
   * A DataReceiver that receives data from a DataSource.
   * @param {!MojoHandle} source The handle to the DataSource.
   * @param {!MojoHandle} client The handle to the DataSourceClient.
   * @param {number} bufferSize How large a buffer to use.
   * @param {number} fatalErrorValue The receive error value to report in the
   *     event of a fatal error.
   * @constructor
   * @alias module:data_receiver.DataReceiver
   */
  function DataReceiver(source, client, bufferSize, fatalErrorValue) {
    this.init_(source, client, fatalErrorValue, 0, null, [], false);
    this.source_.init(bufferSize);
  }

  DataReceiver.prototype =
      $Object.create(dataStream.DataSourceClient.stubClass.prototype);

  /**
   * Closes this DataReceiver.
   */
  DataReceiver.prototype.close = function() {
    if (this.shutDown_)
      return;
    this.shutDown_ = true;
    this.router_.close();
    this.clientRouter_.close();
    if (this.receive_) {
      this.receive_.dispatchFatalError(this.fatalErrorValue_);
      this.receive_ = null;
    }
  };

  /**
   * Initialize this DataReceiver.
   * @param {!MojoHandle} source A handle to the DataSource.
   * @param {!MojoHandle} client A handle to the DataSourceClient.
   * @param {number} fatalErrorValue The error to dispatch in the event of a
   *     fatal error.
   * @param {number} bytesReceived The number of bytes already received.
   * @param {PendingReceiveError} pendingError The pending error if there is
   *     one.
   * @param {!Array<!ArrayBuffer>} pendingData Data received from the
   *     DataSource not yet requested by the client.
   * @param {boolean} paused Whether the DataSource is paused.
   * @private
   */
  DataReceiver.prototype.init_ = function(source, client, fatalErrorValue,
                                          bytesReceived, pendingError,
                                          pendingData, paused) {
    /**
     * The [Router]{@link module:mojo/public/js/router.Router} for the
     * connection to the DataSource.
     * @private
     */
    this.router_ = new router.Router(source);
    /**
     * The [Router]{@link module:mojo/public/js/router.Router} for the
     * connection to the DataSource.
     * @private
     */
    this.clientRouter_ = new router.Router(client);
    /**
     * The connection to the DataSource.
     * @private
     */
    this.source_ = new dataStream.DataSource.proxyClass(this.router_);
    this.client_ = new dataStream.DataSourceClient.stubClass(this);
    this.clientRouter_.setIncomingReceiver(this.client_);
    /**
     * The current receive operation.
     * @type {module:data_receiver~PendingReceive}
     * @private
     */
    this.receive_ = null;
    /**
     * The error to be dispatched in the event of a fatal error.
     * @const {number}
     * @private
     */
    this.fatalErrorValue_ = fatalErrorValue;
    /**
     * The pending error if there is one.
     * @type {PendingReceiveError}
     * @private
     */
    this.pendingError_ = pendingError;
    /**
     * Whether the DataSource is paused.
     * @type {boolean}
     * @private
     */
    this.paused_ = paused;
    /**
     * A queue of data that has been received from the DataSource, but not
     * consumed by the client.
     * @type {module:data_receiver~PendingData[]}
     * @private
     */
    this.pendingDataBuffers_ = pendingData;
    /**
     * Whether this DataReceiver has shut down.
     * @type {boolean}
     * @private
     */
    this.shutDown_ = false;
  };

  /**
   * Serializes this DataReceiver.
   * This will cancel a receive if one is in progress.
   * @return {!Promise<SerializedDataReceiver>} A promise that will resolve to
   *     the serialization of this DataReceiver. If this DataReceiver has shut
   *     down, the promise will resolve to null.
   */
  DataReceiver.prototype.serialize = function() {
    if (this.shutDown_)
      return Promise.resolve(null);

    if (this.receive_) {
      this.receive_.dispatchFatalError(this.fatalErrorValue_);
      this.receive_ = null;
    }
    var serialized = new serialization.SerializedDataReceiver();
    serialized.source = this.router_.connector_.handle_;
    serialized.client = this.clientRouter_.connector_.handle_;
    serialized.fatal_error_value = this.fatalErrorValue_;
    serialized.paused = this.paused_;
    serialized.pending_error = this.pendingError_;
    serialized.pending_data = [];
    $Array.forEach(this.pendingDataBuffers_, function(buffer) {
      serialized.pending_data.push(new Uint8Array(buffer));
    });
    this.router_.connector_.handle_ = null;
    this.router_.close();
    this.clientRouter_.connector_.handle_ = null;
    this.clientRouter_.close();
    this.shutDown_ = true;
    return Promise.resolve(serialized);
  };

  /**
   * Deserializes a SerializedDataReceiver.
   * @param {SerializedDataReceiver} serialized The serialized DataReceiver.
   * @return {!DataReceiver} The deserialized DataReceiver.
   */
  DataReceiver.deserialize = function(serialized) {
    var receiver = $Object.create(DataReceiver.prototype);
    receiver.deserialize_(serialized);
    return receiver;
  };

  /**
   * Deserializes a SerializedDataReceiver into this DataReceiver.
   * @param {SerializedDataReceiver} serialized The serialized DataReceiver.
   * @private
   */
  DataReceiver.prototype.deserialize_ = function(serialized) {
    if (!serialized) {
      this.shutDown_ = true;
      return;
    }
    var pendingData = [];
    $Array.forEach(serialized.pending_data, function(data) {
      var buffer = new Uint8Array(data.length);
      buffer.set(data);
      pendingData.push(buffer.buffer);
    });
    this.init_(serialized.source, serialized.client,
               serialized.fatal_error_value, serialized.bytes_received,
               serialized.pending_error, pendingData, serialized.paused);
  };

  /**
   * Receive data from the DataSource.
   * @return {Promise<ArrayBuffer>} A promise to the received data. If an error
   *     occurs, the promise will reject with an Error object with a property
   *     error containing the error code.
   * @throws Will throw if this has encountered a fatal error or another receive
   *     is in progress.
   */
  DataReceiver.prototype.receive = function() {
    if (this.shutDown_)
      throw new Error('DataReceiver has been closed');
    if (this.receive_)
      throw new Error('Receive already in progress.');
    var receive = new PendingReceive();
    var promise = receive.getPromise();
    if (this.pendingError_ &&
        receive.dispatchError(this.pendingError_)) {
      this.pendingError_ = null;
      this.paused_ = true;
      return promise;
    }
    if (this.paused_) {
      this.source_.resume();
      this.paused_ = false;
    }
    this.receive_ = receive;
    this.dispatchData_();
    return promise;
  };

  DataReceiver.prototype.dispatchData_ = function() {
    if (!this.receive_) {
      this.close();
      return;
    }
    if (this.pendingDataBuffers_.length) {
      this.receive_.dispatchData(this.pendingDataBuffers_[0]);
      this.source_.reportBytesReceived(this.pendingDataBuffers_[0].byteLength);
      this.receive_ = null;
      this.pendingDataBuffers_.shift();
      if (this.pendingError_)
        this.pendingError_.queuePosition--;
    }
  };

  /**
   * Invoked by the DataSource when an error is encountered.
   * @param {number} offset The location at which the error occurred.
   * @param {number} error The error that occurred.
   * @private
   */
  DataReceiver.prototype.onError = function(error) {
    if (this.shutDown_)
      return;

    var pendingError = new serialization.PendingReceiveError();
    pendingError.error = error;
    pendingError.queuePosition = this.pendingDataBuffers_.length;
    if (this.receive_ && this.receive_.dispatchError(pendingError)) {
      this.receive_ = null;
      this.paused_ = true;
      return;
    }
    this.pendingError_ = pendingError;
  };

  DataReceiver.prototype.onData = function(data) {
    var buffer = new ArrayBuffer(data.length);
    var uintView = new Uint8Array(buffer);
    uintView.set(data);
    this.pendingDataBuffers_.push(buffer);
    if (this.receive_)
      this.dispatchData_();
  };

  return {DataReceiver: DataReceiver};
});
