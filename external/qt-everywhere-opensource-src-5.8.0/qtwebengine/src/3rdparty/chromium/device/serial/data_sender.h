// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef DEVICE_SERIAL_DATA_SENDER_H_
#define DEVICE_SERIAL_DATA_SENDER_H_

#include <stdint.h>

#include <queue>

#include "base/callback.h"
#include "base/macros.h"
#include "base/memory/linked_ptr.h"
#include "base/strings/string_piece.h"
#include "device/serial/buffer.h"
#include "device/serial/data_stream.mojom.h"
#include "mojo/public/cpp/system/data_pipe.h"

namespace device {

// A DataSender sends data to a DataSink.
class DataSender {
 public:
  typedef base::Callback<void(uint32_t bytes_sent)> DataSentCallback;
  typedef base::Callback<void(uint32_t bytes_sent, int32_t error)>
      SendErrorCallback;
  typedef base::Callback<void()> CancelCallback;

  // Constructs a DataSender to send data to |sink|, using a buffer size of
  // |buffer_size|, with connection errors reported as |fatal_error_value|.
  DataSender(mojo::InterfacePtr<serial::DataSink> sink,
             uint32_t buffer_size,
             int32_t fatal_error_value);

  ~DataSender();

  // Starts an asynchronous send of |data|. If the send completes successfully,
  // |callback| will be called. Otherwise, |error_callback| will be called with
  // the error. If there is a cancel in progress or there has been a connection
  // error, this will not start a send and returns false. It is the caller's
  // responsibility to ensure |data| remains valid until one of |callback| and
  // |error_callback| is called.
  bool Send(const base::StringPiece& data,
            const DataSentCallback& callback,
            const SendErrorCallback& error_callback);

  // Requests the cancellation of any in-progress sends. Calls to Send() will
  // fail until |callback| is called.
  bool Cancel(int32_t error, const CancelCallback& callback);

 private:
  class PendingSend;

  // Invoked when a PendingSend completes.
  void SendComplete();

  // Invoked when a PendingSend fails with |error|.
  void SendFailed(int32_t error);

  // mojo error handler
  void OnConnectionError();

  // Dispatches a cancel callback if one is pending.
  void RunCancelCallback();

  // Shuts down this DataSender and dispatches fatal errors to all pending
  // operations.
  void ShutDown();

  // The control connection to the data sink.
  mojo::InterfacePtr<serial::DataSink> sink_;

  // The error value to report in the event of a fatal error.
  const int32_t fatal_error_value_;

  // A queue of PendingSend that have been sent to |sink_|, but have not yet
  // been acked by the DataSink.
  std::queue<linked_ptr<PendingSend>> sends_awaiting_ack_;

  // The callback to report cancel completion if a cancel operation is in
  // progress.
  CancelCallback pending_cancel_;

  // Whether we have encountered a fatal error and shut down.
  bool shut_down_;

  DISALLOW_COPY_AND_ASSIGN(DataSender);
};

}  // namespace device

#endif  // DEVICE_SERIAL_DATA_SENDER_H_
