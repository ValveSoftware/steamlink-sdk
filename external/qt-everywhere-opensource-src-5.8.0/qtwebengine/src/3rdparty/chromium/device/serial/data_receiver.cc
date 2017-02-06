// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "device/serial/data_receiver.h"

#include <limits>
#include <memory>
#include <utility>

#include "base/bind.h"
#include "base/location.h"
#include "base/single_thread_task_runner.h"
#include "base/threading/thread_task_runner_handle.h"

namespace device {

// Represents a receive that is not yet fulfilled.
class DataReceiver::PendingReceive {
 public:
  PendingReceive(DataReceiver* receiver,
                 const ReceiveDataCallback& callback,
                 const ReceiveErrorCallback& error_callback,
                 int32_t fatal_error_value);

  // Dispatches |data| to |receive_callback_|. Returns whether this
  // PendingReceive is finished by this call.
  bool DispatchDataFrame(DataReceiver::DataFrame* data);

  // Reports |fatal_error_value_| to |receive_error_callback_|.
  void DispatchFatalError();

  bool buffer_in_use() { return buffer_in_use_; }

 private:
  class Buffer;

  // Invoked when the user is finished with the ReadOnlyBuffer provided to
  // |receive_callback_|.
  void Done(uint32_t num_bytes);

  // The DataReceiver that owns this.
  DataReceiver* receiver_;

  // The callback to dispatch data.
  ReceiveDataCallback receive_callback_;

  // The callback to report errors.
  ReceiveErrorCallback receive_error_callback_;

  // The error value to report when DispatchFatalError() is called.
  const int32_t fatal_error_value_;

  // True if the user owns a buffer passed to |receive_callback_| as part of
  // DispatchDataFrame().
  bool buffer_in_use_;
};

// A ReadOnlyBuffer implementation that provides a view of a buffer owned by a
// DataReceiver.
class DataReceiver::PendingReceive::Buffer : public ReadOnlyBuffer {
 public:
  Buffer(scoped_refptr<DataReceiver> pipe,
         PendingReceive* receive,
         const char* buffer,
         uint32_t buffer_size);
  ~Buffer() override;

  // ReadOnlyBuffer overrides.
  const char* GetData() override;
  uint32_t GetSize() override;
  void Done(uint32_t bytes_consumed) override;
  void DoneWithError(uint32_t bytes_consumed, int32_t error) override;

 private:
  // The DataReceiver of whose buffer we are providing a view.
  scoped_refptr<DataReceiver> receiver_;

  // The PendingReceive to which this buffer has been created in response.
  PendingReceive* pending_receive_;

  const char* buffer_;
  uint32_t buffer_size_;
};

// A buffer of data or an error received from the DataSource.
struct DataReceiver::DataFrame {
  explicit DataFrame(mojo::Array<uint8_t> data)
      : is_error(false),
        data(std::move(data)),
        offset(0),
        error(0),
        dispatched(false) {}

  explicit DataFrame(int32_t error)
      : is_error(true), offset(0), error(error), dispatched(false) {}

  // Whether this DataFrame represents an error.
  bool is_error;

  // The data received from the DataSource.
  mojo::Array<uint8_t> data;

  // The offset within |data| at which the next read should begin.
  uint32_t offset;

  // The value of the error that occurred.
  const int32_t error;

  // Whether the error has been dispatched to the user.
  bool dispatched;
};

DataReceiver::DataReceiver(
    mojo::InterfacePtr<serial::DataSource> source,
    mojo::InterfaceRequest<serial::DataSourceClient> client,
    uint32_t buffer_size,
    int32_t fatal_error_value)
    : source_(std::move(source)),
      client_(this, std::move(client)),
      fatal_error_value_(fatal_error_value),
      shut_down_(false),
      weak_factory_(this) {
  source_.set_connection_error_handler(
      base::Bind(&DataReceiver::OnConnectionError, base::Unretained(this)));
  source_->Init(buffer_size);
  client_.set_connection_error_handler(
      base::Bind(&DataReceiver::OnConnectionError, base::Unretained(this)));
}

bool DataReceiver::Receive(const ReceiveDataCallback& callback,
                           const ReceiveErrorCallback& error_callback) {
  DCHECK(!callback.is_null() && !error_callback.is_null());
  if (pending_receive_ || shut_down_)
    return false;
  // When the DataSource encounters an error, it pauses transmission. When the
  // user starts a new receive following notification of the error (via
  // |error_callback| of the previous Receive call) of the error we can tell the
  // DataSource to resume transmission of data.
  if (!pending_data_frames_.empty() && pending_data_frames_.front()->is_error &&
      pending_data_frames_.front()->dispatched) {
    source_->Resume();
    pending_data_frames_.pop();
  }

  pending_receive_.reset(
      new PendingReceive(this, callback, error_callback, fatal_error_value_));
  ReceiveInternal();
  return true;
}

DataReceiver::~DataReceiver() {
  ShutDown();
}

void DataReceiver::OnError(int32_t error) {
  if (shut_down_)
    return;

  pending_data_frames_.push(linked_ptr<DataFrame>(new DataFrame(error)));
  if (pending_receive_)
    ReceiveInternal();
}

void DataReceiver::OnData(mojo::Array<uint8_t> data) {
  pending_data_frames_.push(
      linked_ptr<DataFrame>(new DataFrame(std::move(data))));
  if (pending_receive_)
    ReceiveInternal();
}

void DataReceiver::OnConnectionError() {
  ShutDown();
}

void DataReceiver::Done(uint32_t bytes_consumed) {
  if (shut_down_)
    return;

  DCHECK(pending_receive_);
  DataFrame& pending_data = *pending_data_frames_.front();
  pending_data.offset += bytes_consumed;
  DCHECK_LE(pending_data.offset, pending_data.data.size());
  if (pending_data.offset == pending_data.data.size()) {
    source_->ReportBytesReceived(
        static_cast<uint32_t>(pending_data.data.size()));
    pending_data_frames_.pop();
  }
  pending_receive_.reset();
}

void DataReceiver::ReceiveInternal() {
  if (shut_down_)
    return;
  DCHECK(pending_receive_);
  if (pending_receive_->buffer_in_use())
    return;

  if (!pending_data_frames_.empty() &&
      pending_receive_->DispatchDataFrame(pending_data_frames_.front().get())) {
    pending_receive_.reset();
  }
}

void DataReceiver::ShutDown() {
  shut_down_ = true;
  if (pending_receive_)
    pending_receive_->DispatchFatalError();
}

DataReceiver::PendingReceive::PendingReceive(
    DataReceiver* receiver,
    const ReceiveDataCallback& callback,
    const ReceiveErrorCallback& error_callback,
    int32_t fatal_error_value)
    : receiver_(receiver),
      receive_callback_(callback),
      receive_error_callback_(error_callback),
      fatal_error_value_(fatal_error_value),
      buffer_in_use_(false) {
}

bool DataReceiver::PendingReceive::DispatchDataFrame(
    DataReceiver::DataFrame* data) {
  DCHECK(!buffer_in_use_);
  DCHECK(!data->dispatched);

  if (data->is_error) {
    data->dispatched = true;
    base::ThreadTaskRunnerHandle::Get()->PostTask(
        FROM_HERE, base::Bind(receive_error_callback_, data->error));
    return true;
  }
  buffer_in_use_ = true;
  base::ThreadTaskRunnerHandle::Get()->PostTask(
      FROM_HERE,
      base::Bind(
          receive_callback_,
          base::Passed(std::unique_ptr<ReadOnlyBuffer>(new Buffer(
              receiver_, this,
              reinterpret_cast<char*>(&data->data[0]) + data->offset,
              static_cast<uint32_t>(data->data.size() - data->offset))))));
  return false;
}

void DataReceiver::PendingReceive::DispatchFatalError() {
  receive_error_callback_.Run(fatal_error_value_);
}

void DataReceiver::PendingReceive::Done(uint32_t bytes_consumed) {
  DCHECK(buffer_in_use_);
  buffer_in_use_ = false;
  receiver_->Done(bytes_consumed);
}

DataReceiver::PendingReceive::Buffer::Buffer(
    scoped_refptr<DataReceiver> receiver,
    PendingReceive* receive,
    const char* buffer,
    uint32_t buffer_size)
    : receiver_(receiver),
      pending_receive_(receive),
      buffer_(buffer),
      buffer_size_(buffer_size) {
}

DataReceiver::PendingReceive::Buffer::~Buffer() {
  if (pending_receive_)
    pending_receive_->Done(0);
}

const char* DataReceiver::PendingReceive::Buffer::GetData() {
  return buffer_;
}

uint32_t DataReceiver::PendingReceive::Buffer::GetSize() {
  return buffer_size_;
}

void DataReceiver::PendingReceive::Buffer::Done(uint32_t bytes_consumed) {
  pending_receive_->Done(bytes_consumed);
  pending_receive_ = NULL;
  receiver_ = NULL;
  buffer_ = NULL;
  buffer_size_ = 0;
}

void DataReceiver::PendingReceive::Buffer::DoneWithError(
    uint32_t bytes_consumed,
    int32_t error) {
  Done(bytes_consumed);
}

}  // namespace device
