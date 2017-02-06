// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef DEVICE_SERIAL_DATA_RECEIVER_H_
#define DEVICE_SERIAL_DATA_RECEIVER_H_

#include <stdint.h>

#include <memory>
#include <queue>

#include "base/callback.h"
#include "base/macros.h"
#include "base/memory/linked_ptr.h"
#include "base/memory/ref_counted.h"
#include "base/memory/weak_ptr.h"
#include "device/serial/buffer.h"
#include "device/serial/data_stream.mojom.h"
#include "mojo/public/cpp/bindings/binding.h"
#include "mojo/public/cpp/system/data_pipe.h"

namespace device {

// A DataReceiver receives data from a DataSource.
class DataReceiver : public base::RefCounted<DataReceiver>,
                     public serial::DataSourceClient {
 public:
  typedef base::Callback<void(std::unique_ptr<ReadOnlyBuffer>)>
      ReceiveDataCallback;
  typedef base::Callback<void(int32_t error)> ReceiveErrorCallback;

  // Constructs a DataReceiver to receive data from |source|, using a buffer
  // size of |buffer_size|, with connection errors reported as
  // |fatal_error_value|.
  DataReceiver(mojo::InterfacePtr<serial::DataSource> source,
               mojo::InterfaceRequest<serial::DataSourceClient> client,
               uint32_t buffer_size,
               int32_t fatal_error_value);

  // Begins a receive operation. If this returns true, exactly one of |callback|
  // and |error_callback| will eventually be called. If there is already a
  // receive in progress or there has been a connection error, this will have no
  // effect and return false.
  bool Receive(const ReceiveDataCallback& callback,
               const ReceiveErrorCallback& error_callback);

 private:
  class PendingReceive;
  struct DataFrame;
  friend class base::RefCounted<DataReceiver>;

  ~DataReceiver() override;

  // serial::DataSourceClient overrides.
  // Invoked by the DataSource to report errors.
  void OnError(int32_t error) override;
  // Invoked by the DataSource transmit data.
  void OnData(mojo::Array<uint8_t> data) override;

  // mojo error handler
  void OnConnectionError();

  // Invoked by the PendingReceive to report that the user is done with the
  // receive buffer, having read |bytes_read| bytes from it.
  void Done(uint32_t bytes_read);

  // The implementation of Receive(). If a |pending_error_| is ready to
  // dispatch, it does so. Otherwise, this attempts to read from |handle_| and
  // dispatches the contents to |pending_receive_|. If |handle_| is not ready,
  // |waiter_| is used to wait until it is ready. If an error occurs in reading
  // |handle_|, ShutDown() is called.
  void ReceiveInternal();

  // Called when we encounter a fatal error. If a receive is in progress,
  // |fatal_error_value_| will be reported to the user.
  void ShutDown();

  // The control connection to the data source.
  mojo::InterfacePtr<serial::DataSource> source_;
  mojo::Binding<serial::DataSourceClient> client_;

  // The error value to report in the event of a fatal error.
  const int32_t fatal_error_value_;

  // Whether we have encountered a fatal error and shut down.
  bool shut_down_;

  // The current pending receive operation if there is one.
  std::unique_ptr<PendingReceive> pending_receive_;

  // The queue of pending data frames (data or an error) received from the
  // DataSource.
  std::queue<linked_ptr<DataFrame>> pending_data_frames_;

  base::WeakPtrFactory<DataReceiver> weak_factory_;

  DISALLOW_COPY_AND_ASSIGN(DataReceiver);
};

}  // namespace device

#endif  // DEVICE_SERIAL_DATA_RECEIVER_H_
