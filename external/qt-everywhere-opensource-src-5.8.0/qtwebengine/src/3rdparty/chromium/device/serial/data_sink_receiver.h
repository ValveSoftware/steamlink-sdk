// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef DEVICE_SERIAL_DATA_SINK_RECEIVER_H_
#define DEVICE_SERIAL_DATA_SINK_RECEIVER_H_

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

class DataSinkReceiver : public base::RefCounted<DataSinkReceiver>,
                         public serial::DataSink {
 public:
  typedef base::Callback<void(std::unique_ptr<ReadOnlyBuffer>)> ReadyCallback;
  typedef base::Callback<void(int32_t error)> CancelCallback;
  typedef base::Callback<void()> ErrorCallback;

  // Constructs a DataSinkReceiver. Whenever the pipe is ready for reading, the
  // |ready_callback| will be called with the ReadOnlyBuffer to read.
  // |ready_callback| will not be called again until the previous ReadOnlyBuffer
  // is destroyed. If a connection error occurs, |error_callback| will be called
  // and the DataSinkReceiver will act as if ShutDown() had been called. If
  // |cancel_callback| is valid, it will be called when the DataSink client
  // requests cancellation of the in-progress read.
  DataSinkReceiver(mojo::InterfaceRequest<serial::DataSink> request,
                   const ReadyCallback& ready_callback,
                   const CancelCallback& cancel_callback,
                   const ErrorCallback& error_callback);

  // Shuts down this DataSinkReceiver. After shut down, |ready_callback|,
  // |cancel_callback| and |error_callback| will never be called.
  void ShutDown();

 private:
  class Buffer;
  class DataFrame;
  friend class base::RefCounted<DataSinkReceiver>;

  ~DataSinkReceiver() override;

  // serial::DataSink overrides.
  void Cancel(int32_t error) override;
  void OnData(mojo::Array<uint8_t> data,
              const OnDataCallback& callback) override;
  void ClearError() override;

  void OnConnectionError();

  // Dispatches data to |ready_callback_|.
  void RunReadyCallback();

  // Reports a successful read of |bytes_read|.
  void Done(uint32_t bytes_read);

  // Reports a partially successful or unsuccessful read of |bytes_read|
  // with an error of |error|.
  void DoneWithError(uint32_t bytes_read, int32_t error);

  // Marks |bytes_read| bytes as being read.
  bool DoneInternal(uint32_t bytes_read);

  // Reports an error to the client.
  void ReportError(uint32_t bytes_read, int32_t error);

  // Reports a fatal error to the client and shuts down.
  void DispatchFatalError();

  mojo::Binding<serial::DataSink> binding_;

  // The callback to call when there is data ready to read.
  const ReadyCallback ready_callback_;

  // The callback to call when the client has requested cancellation.
  const CancelCallback cancel_callback_;

  // The callback to call if a fatal error occurs.
  const ErrorCallback error_callback_;

  // The current error that has not been cleared by a ClearError message..
  int32_t current_error_;

  // The buffer passed to |ready_callback_| if one exists. This is not owned,
  // but the Buffer will call Done or DoneWithError before being deleted.
  Buffer* buffer_in_use_;

  // The data we have received from the client that has not been passed to
  // |ready_callback_|.
  std::queue<linked_ptr<DataFrame>> pending_data_buffers_;

  // Whether we have encountered a fatal error and shut down.
  bool shut_down_;

  base::WeakPtrFactory<DataSinkReceiver> weak_factory_;

  DISALLOW_COPY_AND_ASSIGN(DataSinkReceiver);
};

}  // namespace device

#endif  // DEVICE_SERIAL_DATA_SINK_RECEIVER_H_
