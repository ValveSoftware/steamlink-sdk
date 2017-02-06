// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef DEVICE_SERIAL_DATA_SOURCE_SENDER_H_
#define DEVICE_SERIAL_DATA_SOURCE_SENDER_H_

#include <stdint.h>

#include <memory>
#include <vector>

#include "base/callback.h"
#include "base/macros.h"
#include "base/memory/ref_counted.h"
#include "base/memory/weak_ptr.h"
#include "device/serial/buffer.h"
#include "device/serial/data_stream.mojom.h"
#include "mojo/public/cpp/bindings/binding.h"
#include "mojo/public/cpp/system/data_pipe.h"

namespace device {

// A DataSourceSender is an interface between a source of data and a
// DataSourceClient.
class DataSourceSender : public base::RefCounted<DataSourceSender>,
                         public serial::DataSource {
 public:
  typedef base::Callback<void(std::unique_ptr<WritableBuffer>)> ReadyCallback;
  typedef base::Callback<void()> ErrorCallback;

  // Constructs a DataSourceSender. Whenever the pipe is ready for writing, the
  // |ready_callback| will be called with the WritableBuffer to be filled.
  // |ready_callback| will not be called again until the previous WritableBuffer
  // is destroyed. If a connection error occurs, |error_callback| will be
  // called and the DataSourceSender will act as if ShutDown() had been called.
  DataSourceSender(mojo::InterfaceRequest<serial::DataSource> source,
                   mojo::InterfacePtr<serial::DataSourceClient> client,
                   const ReadyCallback& ready_callback,
                   const ErrorCallback& error_callback);

  // Shuts down this DataSourceSender. After shut down, |ready_callback| and
  // |error_callback| will never be called.
  void ShutDown();

 private:
  friend class base::RefCounted<DataSourceSender>;
  class PendingSend;

  ~DataSourceSender() override;

  // mojo::InterfaceImpl<serial::DataSourceSender> overrides.
  void Init(uint32_t buffer_size) override;
  void Resume() override;
  void ReportBytesReceived(uint32_t bytes_sent) override;
  // mojo error handler. Calls DispatchFatalError().
  void OnConnectionError();

  // Gets more data to send to the DataSourceClient.
  void GetMoreData();

  // Invoked to pass |data| obtained in response to |ready_callback_|.
  void Done(const std::vector<char>& data);

  // Invoked to pass |data| and |error| obtained in response to
  // |ready_callback_|.
  void DoneWithError(const std::vector<char>& data, int32_t error);

  // Dispatches |data| to the client.
  void DoneInternal(const std::vector<char>& data);

  // Reports a fatal error to the client and shuts down.
  void DispatchFatalError();

  mojo::Binding<serial::DataSource> binding_;
  mojo::InterfacePtr<serial::DataSourceClient> client_;

  // The callback to call when the client is ready for more data.
  ReadyCallback ready_callback_;

  // The callback to call if a fatal error occurs.
  ErrorCallback error_callback_;

  // The current pending send operation if there is one.
  std::unique_ptr<PendingSend> pending_send_;

  // The number of bytes available for buffering in the client.
  uint32_t available_buffer_capacity_;

  // Whether sending is paused due to an error.
  bool paused_;

  // Whether we have encountered a fatal error and shut down.
  bool shut_down_;

  base::WeakPtrFactory<DataSourceSender> weak_factory_;

  DISALLOW_COPY_AND_ASSIGN(DataSourceSender);
};

}  // namespace device

#endif  // DEVICE_SERIAL_DATA_SOURCE_SENDER_H_
