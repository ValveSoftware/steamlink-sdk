// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

define('serial_service', [
    'content/public/renderer/frame_service_registry',
    'data_receiver',
    'data_sender',
    'device/serial/serial.mojom',
    'device/serial/serial_serialization.mojom',
    'mojo/public/js/core',
    'mojo/public/js/router',
    'stash_client',
], function(serviceProvider,
            dataReceiver,
            dataSender,
            serialMojom,
            serialization,
            core,
            routerModule,
            stashClient) {
  /**
   * A Javascript client for the serial service and connection Mojo services.
   *
   * This provides a thick client around the Mojo services, exposing a JS-style
   * interface to serial connections and information about serial devices. This
   * converts parameters and result between the Apps serial API types and the
   * Mojo types.
   */

  var service = new serialMojom.SerialService.proxyClass(
      new routerModule.Router(
          serviceProvider.connectToService(serialMojom.SerialService.name)));

  function getDevices() {
    return service.getDevices().then(function(response) {
      return $Array.map(response.devices, function(device) {
        var result = {path: device.path};
        if (device.has_vendor_id)
          result.vendorId = device.vendor_id;
        if (device.has_product_id)
          result.productId = device.product_id;
        if (device.display_name)
          result.displayName = device.display_name;
        return result;
      });
    });
  }

  var DATA_BITS_TO_MOJO = {
    undefined: serialMojom.DataBits.NONE,
    'seven': serialMojom.DataBits.SEVEN,
    'eight': serialMojom.DataBits.EIGHT,
  };
  var STOP_BITS_TO_MOJO = {
    undefined: serialMojom.StopBits.NONE,
    'one': serialMojom.StopBits.ONE,
    'two': serialMojom.StopBits.TWO,
  };
  var PARITY_BIT_TO_MOJO = {
    undefined: serialMojom.ParityBit.NONE,
    'no': serialMojom.ParityBit.NO,
    'odd': serialMojom.ParityBit.ODD,
    'even': serialMojom.ParityBit.EVEN,
  };
  var SEND_ERROR_TO_MOJO = {
    undefined: serialMojom.SendError.NONE,
    'disconnected': serialMojom.SendError.DISCONNECTED,
    'pending': serialMojom.SendError.PENDING,
    'timeout': serialMojom.SendError.TIMEOUT,
    'system_error': serialMojom.SendError.SYSTEM_ERROR,
  };
  var RECEIVE_ERROR_TO_MOJO = {
    undefined: serialMojom.ReceiveError.NONE,
    'disconnected': serialMojom.ReceiveError.DISCONNECTED,
    'device_lost': serialMojom.ReceiveError.DEVICE_LOST,
    'timeout': serialMojom.ReceiveError.TIMEOUT,
    'break': serialMojom.ReceiveError.BREAK,
    'frame_error': serialMojom.ReceiveError.FRAME_ERROR,
    'overrun': serialMojom.ReceiveError.OVERRUN,
    'buffer_overflow': serialMojom.ReceiveError.BUFFER_OVERFLOW,
    'parity_error': serialMojom.ReceiveError.PARITY_ERROR,
    'system_error': serialMojom.ReceiveError.SYSTEM_ERROR,
  };

  function invertMap(input) {
    var output = {};
    for (var key in input) {
      if (key == 'undefined')
        output[input[key]] = undefined;
      else
        output[input[key]] = key;
    }
    return output;
  }
  var DATA_BITS_FROM_MOJO = invertMap(DATA_BITS_TO_MOJO);
  var STOP_BITS_FROM_MOJO = invertMap(STOP_BITS_TO_MOJO);
  var PARITY_BIT_FROM_MOJO = invertMap(PARITY_BIT_TO_MOJO);
  var SEND_ERROR_FROM_MOJO = invertMap(SEND_ERROR_TO_MOJO);
  var RECEIVE_ERROR_FROM_MOJO = invertMap(RECEIVE_ERROR_TO_MOJO);

  function getServiceOptions(options) {
    var out = {};
    if (options.dataBits)
      out.data_bits = DATA_BITS_TO_MOJO[options.dataBits];
    if (options.stopBits)
      out.stop_bits = STOP_BITS_TO_MOJO[options.stopBits];
    if (options.parityBit)
      out.parity_bit = PARITY_BIT_TO_MOJO[options.parityBit];
    if ('ctsFlowControl' in options) {
      out.has_cts_flow_control = true;
      out.cts_flow_control = options.ctsFlowControl;
    }
    if ('bitrate' in options)
      out.bitrate = options.bitrate;
    return out;
  }

  function convertServiceInfo(result) {
    if (!result.info)
      throw new Error('Failed to get ConnectionInfo.');
    return {
      ctsFlowControl: !!result.info.cts_flow_control,
      bitrate: result.info.bitrate || undefined,
      dataBits: DATA_BITS_FROM_MOJO[result.info.data_bits],
      stopBits: STOP_BITS_FROM_MOJO[result.info.stop_bits],
      parityBit: PARITY_BIT_FROM_MOJO[result.info.parity_bit],
    };
  }

  // Update client-side options |clientOptions| from the user-provided
  // |options|.
  function updateClientOptions(clientOptions, options) {
    if ('name' in options)
      clientOptions.name = options.name;
    if ('receiveTimeout' in options)
      clientOptions.receiveTimeout = options.receiveTimeout;
    if ('sendTimeout' in options)
      clientOptions.sendTimeout = options.sendTimeout;
    if ('bufferSize' in options)
      clientOptions.bufferSize = options.bufferSize;
    if ('persistent' in options)
      clientOptions.persistent = options.persistent;
  };

  function Connection(connection, router, receivePipe, receiveClientPipe,
                      sendPipe, id, options) {
    var state = new serialization.ConnectionState();
    state.connectionId = id;
    updateClientOptions(state, options);
    var receiver = new dataReceiver.DataReceiver(
        receivePipe, receiveClientPipe, state.bufferSize,
        serialMojom.ReceiveError.DISCONNECTED);
    var sender = new dataSender.DataSender(sendPipe, state.bufferSize,
                                           serialMojom.SendError.DISCONNECTED);
    this.init_(state,
               connection,
               router,
               receiver,
               sender,
               null,
               serialMojom.ReceiveError.NONE);
    connections_.set(id, this);
    this.startReceive_();
  }

  // Initializes this Connection from the provided args.
  Connection.prototype.init_ = function(state,
                                        connection,
                                        router,
                                        receiver,
                                        sender,
                                        queuedReceiveData,
                                        queuedReceiveError) {
    this.state_ = state;

    // queuedReceiveData_ or queuedReceiveError_ will store the receive result
    // or error, respectively, if a receive completes or fails while this
    // connection is paused. At most one of the the two may be non-null: a
    // receive completed while paused will only set one of them, no further
    // receives will be performed while paused and a queued result is dispatched
    // before any further receives are initiated when unpausing.
    if (queuedReceiveError != serialMojom.ReceiveError.NONE)
      this.queuedReceiveError_ = {error: queuedReceiveError};
    if (queuedReceiveData) {
      this.queuedReceiveData_ = new ArrayBuffer(queuedReceiveData.length);
      new Int8Array(this.queuedReceiveData_).set(queuedReceiveData);
    }
    this.router_ = router;
    this.remoteConnection_ = connection;
    this.receivePipe_ = receiver;
    this.sendPipe_ = sender;
    this.sendInProgress_ = false;
  };

  Connection.create = function(path, options) {
    options = options || {};
    var serviceOptions = getServiceOptions(options);
    var pipe = core.createMessagePipe();
    var sendPipe = core.createMessagePipe();
    var receivePipe = core.createMessagePipe();
    var receivePipeClient = core.createMessagePipe();
    service.connect(path,
                    serviceOptions,
                    pipe.handle0,
                    sendPipe.handle0,
                    receivePipe.handle0,
                    receivePipeClient.handle0);
    var router = new routerModule.Router(pipe.handle1);
    var connection = new serialMojom.Connection.proxyClass(router);
    return connection.getInfo().then(convertServiceInfo).then(function(info) {
      return Promise.all([info, allocateConnectionId()]);
    }).catch(function(e) {
      router.close();
      core.close(sendPipe.handle1);
      core.close(receivePipe.handle1);
      core.close(receivePipeClient.handle1);
      throw e;
    }).then(function(results) {
      var info = results[0];
      var id = results[1];
      var serialConnectionClient = new Connection(connection,
                                                  router,
                                                  receivePipe.handle1,
                                                  receivePipeClient.handle1,
                                                  sendPipe.handle1,
                                                  id,
                                                  options);
      var clientInfo = serialConnectionClient.getClientInfo_();
      for (var key in clientInfo) {
        info[key] = clientInfo[key];
      }
      return {
        connection: serialConnectionClient,
        info: info,
      };
    });
  };

  Connection.prototype.close = function() {
    this.router_.close();
    this.receivePipe_.close();
    this.sendPipe_.close();
    clearTimeout(this.receiveTimeoutId_);
    clearTimeout(this.sendTimeoutId_);
    connections_.delete(this.state_.connectionId);
    return true;
  };

  Connection.prototype.getClientInfo_ = function() {
    return {
      connectionId: this.state_.connectionId,
      paused: this.state_.paused,
      persistent: this.state_.persistent,
      name: this.state_.name,
      receiveTimeout: this.state_.receiveTimeout,
      sendTimeout: this.state_.sendTimeout,
      bufferSize: this.state_.bufferSize,
    };
  };

  Connection.prototype.getInfo = function() {
    var info = this.getClientInfo_();
    return this.remoteConnection_.getInfo().then(convertServiceInfo).then(
        function(result) {
      for (var key in result) {
        info[key] = result[key];
      }
      return info;
    }).catch(function() {
      return info;
    });
  };

  Connection.prototype.setOptions = function(options) {
    updateClientOptions(this.state_, options);
    var serviceOptions = getServiceOptions(options);
    if ($Object.keys(serviceOptions).length == 0)
      return true;
    return this.remoteConnection_.setOptions(serviceOptions).then(
        function(result) {
      return !!result.success;
    }).catch(function() {
      return false;
    });
  };

  Connection.prototype.getControlSignals = function() {
    return this.remoteConnection_.getControlSignals().then(function(result) {
      if (!result.signals)
        throw new Error('Failed to get control signals.');
      var signals = result.signals;
      return {
        dcd: !!signals.dcd,
        cts: !!signals.cts,
        ri: !!signals.ri,
        dsr: !!signals.dsr,
      };
    });
  };

  Connection.prototype.setControlSignals = function(signals) {
    var controlSignals = {};
    if ('dtr' in signals) {
      controlSignals.has_dtr = true;
      controlSignals.dtr = signals.dtr;
    }
    if ('rts' in signals) {
      controlSignals.has_rts = true;
      controlSignals.rts = signals.rts;
    }
    return this.remoteConnection_.setControlSignals(controlSignals).then(
        function(result) {
      return !!result.success;
    });
  };

  Connection.prototype.flush = function() {
    return this.remoteConnection_.flush().then(function(result) {
      return !!result.success;
    });
  };

  Connection.prototype.setPaused = function(paused) {
    this.state_.paused = paused;
    if (paused) {
      clearTimeout(this.receiveTimeoutId_);
      this.receiveTimeoutId_ = null;
    } else if (!this.receiveInProgress_) {
      this.startReceive_();
    }
  };

  Connection.prototype.send = function(data) {
    if (this.sendInProgress_)
      return Promise.resolve({bytesSent: 0, error: 'pending'});

    if (this.state_.sendTimeout) {
      this.sendTimeoutId_ = setTimeout(function() {
        this.sendPipe_.cancel(serialMojom.SendError.TIMEOUT);
      }.bind(this), this.state_.sendTimeout);
    }
    this.sendInProgress_ = true;
    return this.sendPipe_.send(data).then(function(bytesSent) {
      return {bytesSent: bytesSent};
    }).catch(function(e) {
      return {
        bytesSent: e.bytesSent,
        error: SEND_ERROR_FROM_MOJO[e.error],
      };
    }).then(function(result) {
      if (this.sendTimeoutId_)
        clearTimeout(this.sendTimeoutId_);
      this.sendTimeoutId_ = null;
      this.sendInProgress_ = false;
      return result;
    }.bind(this));
  };

  Connection.prototype.startReceive_ = function() {
    this.receiveInProgress_ = true;
    var receivePromise = null;
    // If we have a queued receive result, dispatch it immediately instead of
    // starting a new receive.
    if (this.queuedReceiveData_) {
      receivePromise = Promise.resolve(this.queuedReceiveData_);
      this.queuedReceiveData_ = null;
    } else if (this.queuedReceiveError_) {
      receivePromise = Promise.reject(this.queuedReceiveError_);
      this.queuedReceiveError_ = null;
    } else {
      receivePromise = this.receivePipe_.receive();
    }
    receivePromise.then(this.onDataReceived_.bind(this)).catch(
        this.onReceiveError_.bind(this));
    this.startReceiveTimeoutTimer_();
  };

  Connection.prototype.onDataReceived_ = function(data) {
    this.startReceiveTimeoutTimer_();
    this.receiveInProgress_ = false;
    if (this.state_.paused) {
      this.queuedReceiveData_ = data;
      return;
    }
    if (this.onData) {
      this.onData(data);
    }
    if (!this.state_.paused) {
      this.startReceive_();
    }
  };

  Connection.prototype.onReceiveError_ = function(e) {
    clearTimeout(this.receiveTimeoutId_);
    this.receiveInProgress_ = false;
    if (this.state_.paused) {
      this.queuedReceiveError_ = e;
      return;
    }
    var error = e.error;
    this.state_.paused = true;
    if (this.onError)
      this.onError(RECEIVE_ERROR_FROM_MOJO[error]);
  };

  Connection.prototype.startReceiveTimeoutTimer_ = function() {
    clearTimeout(this.receiveTimeoutId_);
    if (this.state_.receiveTimeout && !this.state_.paused) {
      this.receiveTimeoutId_ = setTimeout(this.onReceiveTimeout_.bind(this),
                                          this.state_.receiveTimeout);
    }
  };

  Connection.prototype.onReceiveTimeout_ = function() {
    if (this.onError)
      this.onError('timeout');
    this.startReceiveTimeoutTimer_();
  };

  Connection.prototype.serialize = function() {
    connections_.delete(this.state_.connectionId);
    this.onData = null;
    this.onError = null;
    var handle = this.router_.connector_.handle_;
    this.router_.connector_.handle_ = null;
    this.router_.close();
    clearTimeout(this.receiveTimeoutId_);
    clearTimeout(this.sendTimeoutId_);

    // Serializing receivePipe_ will cancel an in-progress receive, which would
    // pause the connection, so save it ahead of time.
    var paused = this.state_.paused;
    return Promise.all([
        this.receivePipe_.serialize(),
        this.sendPipe_.serialize(),
    ]).then(function(serializedComponents) {
      var queuedReceiveError = serialMojom.ReceiveError.NONE;
      if (this.queuedReceiveError_)
        queuedReceiveError = this.queuedReceiveError_.error;
      this.state_.paused = paused;
      var serialized = new serialization.SerializedConnection();
      serialized.state = this.state_;
      serialized.queuedReceiveError = queuedReceiveError;
      serialized.queuedReceiveData =
          this.queuedReceiveData_ ? new Int8Array(this.queuedReceiveData_) :
                                    null;
      serialized.connection = handle;
      serialized.receiver = serializedComponents[0];
      serialized.sender = serializedComponents[1];
      return serialized;
    }.bind(this));
  };

  Connection.deserialize = function(serialized) {
    var serialConnection = $Object.create(Connection.prototype);
    var router = new routerModule.Router(serialized.connection);
    var connection = new serialMojom.Connection.proxyClass(router);
    var receiver = dataReceiver.DataReceiver.deserialize(serialized.receiver);
    var sender = dataSender.DataSender.deserialize(serialized.sender);

    // Ensure that paused and persistent are booleans.
    serialized.state.paused = !!serialized.state.paused;
    serialized.state.persistent = !!serialized.state.persistent;
    serialConnection.init_(serialized.state,
                           connection,
                           router,
                           receiver,
                           sender,
                           serialized.queuedReceiveData,
                           serialized.queuedReceiveError);
    serialConnection.awaitingResume_ = true;
    var connectionId = serialized.state.connectionId;
    connections_.set(connectionId, serialConnection);
    if (connectionId >= nextConnectionId_)
      nextConnectionId_ = connectionId + 1;
    return serialConnection;
  };

  // Resume receives on a deserialized connection.
  Connection.prototype.resumeReceives = function() {
    if (!this.awaitingResume_)
      return;
    this.awaitingResume_ = false;
    if (!this.state_.paused)
      this.startReceive_();
  };

  // All accesses to connections_ and nextConnectionId_ other than those
  // involved in deserialization should ensure that
  // connectionDeserializationComplete_ has resolved first.
  var connectionDeserializationComplete_ = stashClient.retrieve(
      'serial', serialization.SerializedConnection).then(function(decoded) {
    if (!decoded)
      return;
    return Promise.all($Array.map(decoded, Connection.deserialize));
  });

  // The map of connection ID to connection object.
  var connections_ = new Map();

  // The next connection ID to be allocated.
  var nextConnectionId_ = 0;

  function getConnections() {
    return connectionDeserializationComplete_.then(function() {
      return new Map(connections_);
    });
  }

  function getConnection(id) {
    return getConnections().then(function(connections) {
      if (!connections.has(id))
        throw new Error('Serial connection not found.');
      return connections.get(id);
    });
  }

  function allocateConnectionId() {
    return connectionDeserializationComplete_.then(function() {
      return nextConnectionId_++;
    });
  }

  stashClient.registerClient(
      'serial', serialization.SerializedConnection, function() {
    return connectionDeserializationComplete_.then(function() {
      var clientPromises = [];
      for (var connection of connections_.values()) {
        if (connection.state_.persistent)
          clientPromises.push(connection.serialize());
        else
          connection.close();
      }
      return Promise.all($Array.map(clientPromises, function(promise) {
        return promise.then(function(serialization) {
          return {
            serialization: serialization,
            monitorHandles: !serialization.paused,
          };
        });
      }));
    });
  });

  return {
    getDevices: getDevices,
    createConnection: Connection.create,
    getConnection: getConnection,
    getConnections: getConnections,
    // For testing.
    Connection: Connection,
  };
});
