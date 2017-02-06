// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "device/serial/data_source_sender.h"

#include <algorithm>
#include <limits>
#include <memory>
#include <utility>

#include "base/bind.h"
#include "base/location.h"
#include "base/single_thread_task_runner.h"
#include "base/threading/thread_task_runner_handle.h"

namespace device {

// Represents a send that is not yet fulfilled.
class DataSourceSender::PendingSend {
 public:
  PendingSend(DataSourceSender* sender, const ReadyCallback& callback);

  // Asynchronously fills |data_| with up to |num_bytes| of data. Following
  // this, one of Done() and DoneWithError() will be called with the result.
  void GetData(uint32_t num_bytes);

 private:
  class Buffer;
  // Reports a successful write of |bytes_written|.
  void Done(uint32_t bytes_written);

  // Reports a partially successful or unsuccessful write of |bytes_written|
  // with an error of |error|.
  void DoneWithError(uint32_t bytes_written, int32_t error);

  // The DataSourceSender that owns this.
  DataSourceSender* sender_;

  // The callback to call to get data.
  ReadyCallback callback_;

  // Whether the buffer specified by GetData() has been passed to |callback_|,
  // but has not yet called Done() or DoneWithError().
  bool buffer_in_use_;

  // The data obtained using |callback_| to be dispatched to the client.
  std::vector<char> data_;
};

// A Writable implementation that provides a view of a buffer owned by a
// DataSourceSender.
class DataSourceSender::PendingSend::Buffer : public WritableBuffer {
 public:
  Buffer(scoped_refptr<DataSourceSender> sender,
         PendingSend* send,
         char* buffer,
         uint32_t buffer_size);
  ~Buffer() override;

  // WritableBuffer overrides.
  char* GetData() override;
  uint32_t GetSize() override;
  void Done(uint32_t bytes_written) override;
  void DoneWithError(uint32_t bytes_written, int32_t error) override;

 private:
  // The DataSourceSender of whose buffer we are providing a view.
  scoped_refptr<DataSourceSender> sender_;

  // The PendingSend to which this buffer has been created in response.
  PendingSend* pending_send_;

  char* buffer_;
  uint32_t buffer_size_;
};

DataSourceSender::DataSourceSender(
    mojo::InterfaceRequest<serial::DataSource> source,
    mojo::InterfacePtr<serial::DataSourceClient> client,
    const ReadyCallback& ready_callback,
    const ErrorCallback& error_callback)
    : binding_(this, std::move(source)),
      client_(std::move(client)),
      ready_callback_(ready_callback),
      error_callback_(error_callback),
      available_buffer_capacity_(0),
      paused_(false),
      shut_down_(false),
      weak_factory_(this) {
  DCHECK(!ready_callback.is_null() && !error_callback.is_null());
  binding_.set_connection_error_handler(
      base::Bind(&DataSourceSender::OnConnectionError, base::Unretained(this)));
  client_.set_connection_error_handler(
      base::Bind(&DataSourceSender::OnConnectionError, base::Unretained(this)));
}

void DataSourceSender::ShutDown() {
  shut_down_ = true;
  ready_callback_.Reset();
  error_callback_.Reset();
}

DataSourceSender::~DataSourceSender() {
}

void DataSourceSender::Init(uint32_t buffer_size) {
  available_buffer_capacity_ = buffer_size;
  GetMoreData();
}

void DataSourceSender::Resume() {
  if (pending_send_) {
    DispatchFatalError();
    return;
  }

  paused_ = false;
  GetMoreData();
}

void DataSourceSender::ReportBytesReceived(uint32_t bytes_sent) {
  available_buffer_capacity_ += bytes_sent;
  if (!pending_send_ && !paused_)
    GetMoreData();
}

void DataSourceSender::OnConnectionError() {
  DispatchFatalError();
}

void DataSourceSender::GetMoreData() {
  if (shut_down_ || paused_ || pending_send_ || !available_buffer_capacity_)
    return;

  pending_send_.reset(new PendingSend(this, ready_callback_));
  pending_send_->GetData(available_buffer_capacity_);
}

void DataSourceSender::Done(const std::vector<char>& data) {
  DoneInternal(data);
  if (!shut_down_ && available_buffer_capacity_) {
    base::ThreadTaskRunnerHandle::Get()->PostTask(
        FROM_HERE,
        base::Bind(&DataSourceSender::GetMoreData, weak_factory_.GetWeakPtr()));
  }
}

void DataSourceSender::DoneWithError(const std::vector<char>& data,
                                     int32_t error) {
  DoneInternal(data);
  if (!shut_down_)
    client_->OnError(error);
  paused_ = true;
  // We don't call GetMoreData here so we don't send any additional data until
  // Resume() is called.
}

void DataSourceSender::DoneInternal(const std::vector<char>& data) {
  DCHECK(pending_send_);
  if (shut_down_)
    return;

  available_buffer_capacity_ -= static_cast<uint32_t>(data.size());
  if (!data.empty()) {
    mojo::Array<uint8_t> data_to_send(data.size());
    std::copy(data.begin(), data.end(), &data_to_send[0]);
    client_->OnData(std::move(data_to_send));
  }
  pending_send_.reset();
}

void DataSourceSender::DispatchFatalError() {
  if (shut_down_)
    return;

  error_callback_.Run();
  ShutDown();
}

DataSourceSender::PendingSend::PendingSend(DataSourceSender* sender,
                                           const ReadyCallback& callback)
    : sender_(sender), callback_(callback), buffer_in_use_(false) {
}

void DataSourceSender::PendingSend::GetData(uint32_t num_bytes) {
  DCHECK(num_bytes);
  DCHECK(!buffer_in_use_);
  buffer_in_use_ = true;
  data_.resize(num_bytes);
  callback_.Run(std::unique_ptr<WritableBuffer>(
      new Buffer(sender_, this, &data_[0], num_bytes)));
}

void DataSourceSender::PendingSend::Done(uint32_t bytes_written) {
  DCHECK(buffer_in_use_);
  DCHECK_LE(bytes_written, data_.size());
  buffer_in_use_ = false;
  data_.resize(bytes_written);
  sender_->Done(data_);
}

void DataSourceSender::PendingSend::DoneWithError(uint32_t bytes_written,
                                                  int32_t error) {
  DCHECK(buffer_in_use_);
  DCHECK_LE(bytes_written, data_.size());
  buffer_in_use_ = false;
  data_.resize(bytes_written);
  sender_->DoneWithError(data_, error);
}

DataSourceSender::PendingSend::Buffer::Buffer(
    scoped_refptr<DataSourceSender> sender,
    PendingSend* send,
    char* buffer,
    uint32_t buffer_size)
    : sender_(sender),
      pending_send_(send),
      buffer_(buffer),
      buffer_size_(buffer_size) {
}

DataSourceSender::PendingSend::Buffer::~Buffer() {
  if (pending_send_)
    pending_send_->Done(0);
}

char* DataSourceSender::PendingSend::Buffer::GetData() {
  return buffer_;
}

uint32_t DataSourceSender::PendingSend::Buffer::GetSize() {
  return buffer_size_;
}

void DataSourceSender::PendingSend::Buffer::Done(uint32_t bytes_written) {
  DCHECK(sender_.get());
  PendingSend* send = pending_send_;
  pending_send_ = nullptr;
  send->Done(bytes_written);
  sender_ = nullptr;
}

void DataSourceSender::PendingSend::Buffer::DoneWithError(
    uint32_t bytes_written,
    int32_t error) {
  DCHECK(sender_.get());
  PendingSend* send = pending_send_;
  pending_send_ = nullptr;
  send->DoneWithError(bytes_written, error);
  sender_ = nullptr;
}

}  // namespace device
