// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef EXTENSIONS_BROWSER_API_SERIAL_SERIAL_CONNECTION_H_
#define EXTENSIONS_BROWSER_API_SERIAL_SERIAL_CONNECTION_H_

#include <string>
#include <vector>

#include "base/callback.h"
#include "base/memory/weak_ptr.h"
#include "base/time/time.h"
#include "content/public/browser/browser_thread.h"
#include "device/serial/serial_io_handler.h"
#include "extensions/browser/api/api_resource.h"
#include "extensions/browser/api/api_resource_manager.h"
#include "extensions/common/api/serial.h"
#include "net/base/io_buffer.h"

using content::BrowserThread;

namespace extensions {

// Encapsulates an open serial port.
// NOTE: Instances of this object should only be constructed on the IO thread,
// and all methods should only be called on the IO thread unless otherwise
// noted.
class SerialConnection : public ApiResource,
                         public base::SupportsWeakPtr<SerialConnection> {
 public:
  typedef device::SerialIoHandler::OpenCompleteCallback OpenCompleteCallback;

  // This is the callback type expected by Receive. Note that an error result
  // does not necessarily imply an empty |data| string, since a receive may
  // complete partially before being interrupted by an error condition.
  using ReceiveCompleteCallback =
      base::Callback<void(const std::vector<char>& data,
                          api::serial::ReceiveError error)>;

  // This is the callback type expected by Send. Note that an error result
  // does not necessarily imply 0 bytes sent, since a send may complete
  // partially before being interrupted by an error condition.
  using SendCompleteCallback =
      base::Callback<void(int bytes_sent, api::serial::SendError error)>;

  SerialConnection(const std::string& port,
                   const std::string& owner_extension_id);
  ~SerialConnection() override;

  // ApiResource override.
  bool IsPersistent() const override;

  void set_persistent(bool persistent) { persistent_ = persistent; }
  bool persistent() const { return persistent_; }

  void set_name(const std::string& name) { name_ = name; }
  const std::string& name() const { return name_; }

  void set_buffer_size(int buffer_size);
  int buffer_size() const { return buffer_size_; }

  void set_receive_timeout(int receive_timeout);
  int receive_timeout() const { return receive_timeout_; }

  void set_send_timeout(int send_timeout);
  int send_timeout() const { return send_timeout_; }

  void set_paused(bool paused);
  bool paused() const { return paused_; }

  // Initiates an asynchronous Open of the device. It is the caller's
  // responsibility to ensure that this SerialConnection stays alive
  // until |callback| is run.
  virtual void Open(const api::serial::ConnectionOptions& options,
                    const OpenCompleteCallback& callback);

  // Begins an asynchronous receive operation. Calling this while a Receive
  // is already pending is a no-op and returns |false| without calling
  // |callback|.
  virtual bool Receive(const ReceiveCompleteCallback& callback);

  // Begins an asynchronous send operation. Calling this while a Send
  // is already pending is a no-op and returns |false| without calling
  // |callback|.
  virtual bool Send(const std::vector<char>& data,
                    const SendCompleteCallback& callback);

  // Flushes input and output buffers.
  bool Flush() const;

  // Configures some subset of port options for this connection.
  // Omitted options are unchanged. Returns |true| iff the configuration
  // changes were successful.
  bool Configure(const api::serial::ConnectionOptions& options);

  // Connection configuration query. Fills values in an existing
  // ConnectionInfo. Returns |true| iff the connection's information
  // was successfully retrieved.
  bool GetInfo(api::serial::ConnectionInfo* info) const;

  // Reads current control signals (DCD, CTS, etc.) into an existing
  // DeviceControlSignals structure. Returns |true| iff the signals were
  // successfully read.
  bool GetControlSignals(
      api::serial::DeviceControlSignals* control_signals) const;

  // Sets one or more control signals (DTR and/or RTS). Returns |true| iff
  // the signals were successfully set. Unininitialized flags in the
  // HostControlSignals structure are left unchanged.
  bool SetControlSignals(
      const api::serial::HostControlSignals& control_signals);

  // Suspend character transmission. Known as setting/sending 'Break' signal.
  bool SetBreak();

  // Restore character transmission. Known as clear/stop sending 'Break' signal.
  bool ClearBreak();

  // Overrides |io_handler_| for testing.
  void SetIoHandlerForTest(scoped_refptr<device::SerialIoHandler> handler);

  static const BrowserThread::ID kThreadId = BrowserThread::IO;

 private:
  friend class ApiResourceManager<SerialConnection>;
  static const char* service_name() { return "SerialConnectionManager"; }

  // Encapsulates a cancelable, delayed timeout task. Posts a delayed
  // task upon construction and implicitly cancels the task upon
  // destruction if it hasn't run yet.
  class TimeoutTask {
   public:
    TimeoutTask(const base::Closure& closure, const base::TimeDelta& delay);
    ~TimeoutTask();

   private:
    void Run() const;

    base::Closure closure_;
    base::TimeDelta delay_;
    base::WeakPtrFactory<TimeoutTask> weak_factory_;
  };

  // Handles a receive timeout.
  void OnReceiveTimeout();

  // Handles a send timeout.
  void OnSendTimeout();

  // Receives read completion notification from the |io_handler_|.
  void OnAsyncReadComplete(int bytes_read, device::serial::ReceiveError error);

  // Receives write completion notification from the |io_handler_|.
  void OnAsyncWriteComplete(int bytes_sent, device::serial::SendError error);

  // The pathname of the serial device.
  std::string port_;

  // Flag indicating whether or not the connection should persist when
  // its host app is suspended.
  bool persistent_;

  // User-specified connection name.
  std::string name_;

  // Size of the receive buffer.
  int buffer_size_;

  // Amount of time (in ms) to wait for a Read to succeed before triggering a
  // timeout response via onReceiveError.
  int receive_timeout_;

  // Amount of time (in ms) to wait for a Write to succeed before triggering
  // a timeout response.
  int send_timeout_;

  // Flag indicating that the connection is paused. A paused connection will not
  // raise new onReceive events.
  bool paused_;

  // Callback to handle the completion of a pending Receive() request.
  ReceiveCompleteCallback receive_complete_;

  // Callback to handle the completion of a pending Send() request.
  SendCompleteCallback send_complete_;

  // Closure which will trigger a receive timeout unless cancelled. Reset on
  // initialization and after every successful Receive().
  std::unique_ptr<TimeoutTask> receive_timeout_task_;

  // Write timeout closure. Reset on initialization and after every successful
  // Send().
  std::unique_ptr<TimeoutTask> send_timeout_task_;

  scoped_refptr<net::IOBuffer> receive_buffer_;

  // Asynchronous I/O handler.
  scoped_refptr<device::SerialIoHandler> io_handler_;
};

}  // namespace extensions

namespace mojo {

template <>
struct TypeConverter<device::serial::HostControlSignalsPtr,
                     extensions::api::serial::HostControlSignals> {
  static device::serial::HostControlSignalsPtr Convert(
      const extensions::api::serial::HostControlSignals& input);
};

template <>
struct TypeConverter<device::serial::ConnectionOptionsPtr,
                     extensions::api::serial::ConnectionOptions> {
  static device::serial::ConnectionOptionsPtr Convert(
      const extensions::api::serial::ConnectionOptions& input);
};

}  // namespace mojo

#endif  // EXTENSIONS_BROWSER_API_SERIAL_SERIAL_CONNECTION_H_
