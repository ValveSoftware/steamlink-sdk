// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "device/serial/data_sender.h"

#include <algorithm>
#include <utility>

#include "base/bind.h"
#include "base/location.h"
#include "base/single_thread_task_runner.h"
#include "base/threading/thread_task_runner_handle.h"

namespace device {

// Represents a send that is not yet fulfilled.
class DataSender::PendingSend {
 public:
  PendingSend(const base::StringPiece& data,
              const DataSentCallback& callback,
              const SendErrorCallback& error_callback,
              DataSender* sender);

  // Reports |fatal_error_value_| to |receive_error_callback_|.
  void DispatchFatalError();

  // Attempts to send any data not yet sent to |sink|.
  void SendData();

 private:
  // Invoked to report that |num_bytes| of data have been sent and then an
  // error, |error| was encountered. Subtracts the number of bytes that were
  // part of this send from |num_bytes|. If this send was not completed before
  // the error, this calls |error_callback_| to report the error. Otherwise,
  // this calls |callback_|. Returns the number of bytes sent but not acked.
  void OnDataSent(uint32_t num_bytes, int32_t error);

  // The data to send.
  const base::StringPiece data_;

  // The callback to report success.
  const DataSentCallback callback_;

  // The callback to report errors.
  const SendErrorCallback error_callback_;

  // The DataSender that owns this PendingSend.
  DataSender* sender_;
};

DataSender::DataSender(mojo::InterfacePtr<serial::DataSink> sink,
                       uint32_t buffer_size,
                       int32_t fatal_error_value)
    : sink_(std::move(sink)),
      fatal_error_value_(fatal_error_value),
      shut_down_(false) {
  sink_.set_connection_error_handler(
      base::Bind(&DataSender::OnConnectionError, base::Unretained(this)));
}

DataSender::~DataSender() {
  ShutDown();
}

bool DataSender::Send(const base::StringPiece& data,
                      const DataSentCallback& callback,
                      const SendErrorCallback& error_callback) {
  DCHECK(!callback.is_null() && !error_callback.is_null());
  if (!pending_cancel_.is_null() || shut_down_)
    return false;

  linked_ptr<PendingSend> pending_send(
      new PendingSend(data, callback, error_callback, this));
  pending_send->SendData();
  sends_awaiting_ack_.push(pending_send);
  return true;
}

bool DataSender::Cancel(int32_t error, const CancelCallback& callback) {
  DCHECK(!callback.is_null());
  if (!pending_cancel_.is_null() || shut_down_)
    return false;
  if (sends_awaiting_ack_.empty()) {
    base::ThreadTaskRunnerHandle::Get()->PostTask(FROM_HERE, callback);
    return true;
  }

  pending_cancel_ = callback;
  sink_->Cancel(error);
  return true;
}

void DataSender::SendComplete() {
  if (shut_down_)
    return;

  DCHECK(!sends_awaiting_ack_.empty());
  sends_awaiting_ack_.pop();
  if (sends_awaiting_ack_.empty())
    RunCancelCallback();
}

void DataSender::SendFailed(int32_t error) {
  if (shut_down_)
    return;

  DCHECK(!sends_awaiting_ack_.empty());
  sends_awaiting_ack_.pop();
  if (!sends_awaiting_ack_.empty())
    return;
  sink_->ClearError();
  RunCancelCallback();
}

void DataSender::OnConnectionError() {
  ShutDown();
}

void DataSender::RunCancelCallback() {
  DCHECK(sends_awaiting_ack_.empty());
  if (pending_cancel_.is_null())
    return;

  base::ThreadTaskRunnerHandle::Get()->PostTask(FROM_HERE,
                                                base::Bind(pending_cancel_));
  pending_cancel_.Reset();
}

void DataSender::ShutDown() {
  shut_down_ = true;
  while (!sends_awaiting_ack_.empty()) {
    sends_awaiting_ack_.front()->DispatchFatalError();
    sends_awaiting_ack_.pop();
  }
  RunCancelCallback();
}

DataSender::PendingSend::PendingSend(const base::StringPiece& data,
                                     const DataSentCallback& callback,
                                     const SendErrorCallback& error_callback,
                                     DataSender* sender)
    : data_(data),
      callback_(callback),
      error_callback_(error_callback),
      sender_(sender) {
}

void DataSender::PendingSend::OnDataSent(uint32_t num_bytes, int32_t error) {
  if (error) {
    base::ThreadTaskRunnerHandle::Get()->PostTask(
        FROM_HERE, base::Bind(error_callback_, num_bytes, error));
    sender_->SendFailed(error);
  } else {
    DCHECK(num_bytes == data_.size());
    base::ThreadTaskRunnerHandle::Get()->PostTask(
        FROM_HERE, base::Bind(callback_, num_bytes));
    sender_->SendComplete();
  }
}

void DataSender::PendingSend::DispatchFatalError() {
  base::ThreadTaskRunnerHandle::Get()->PostTask(
      FROM_HERE, base::Bind(error_callback_, 0, sender_->fatal_error_value_));
}

void DataSender::PendingSend::SendData() {
  uint32_t num_bytes_to_send = static_cast<uint32_t>(data_.size());
  mojo::Array<uint8_t> bytes(num_bytes_to_send);
  memcpy(&bytes[0], data_.data(), num_bytes_to_send);
  sender_->sink_->OnData(
      std::move(bytes),
      base::Bind(&DataSender::PendingSend::OnDataSent, base::Unretained(this)));
}

}  // namespace device
