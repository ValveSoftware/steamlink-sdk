// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "device/serial/serial_connection.h"

#include <utility>

#include "base/bind.h"
#include "device/serial/buffer.h"
#include "device/serial/data_sink_receiver.h"
#include "device/serial/data_source_sender.h"
#include "device/serial/serial_io_handler.h"

namespace device {

SerialConnection::SerialConnection(
    scoped_refptr<SerialIoHandler> io_handler,
    mojo::InterfaceRequest<serial::DataSink> sink,
    mojo::InterfaceRequest<serial::DataSource> source,
    mojo::InterfacePtr<serial::DataSourceClient> source_client,
    mojo::InterfaceRequest<serial::Connection> request)
    : io_handler_(io_handler), binding_(this, std::move(request)) {
  receiver_ = new DataSinkReceiver(
      std::move(sink),
      base::Bind(&SerialConnection::OnSendPipeReady, base::Unretained(this)),
      base::Bind(&SerialConnection::OnSendCancelled, base::Unretained(this)),
      base::Bind(base::DoNothing));
  sender_ = new DataSourceSender(
      std::move(source), std::move(source_client),
      base::Bind(&SerialConnection::OnReceivePipeReady, base::Unretained(this)),
      base::Bind(base::DoNothing));
}

SerialConnection::~SerialConnection() {
  receiver_->ShutDown();
  sender_->ShutDown();
  io_handler_->CancelRead(serial::ReceiveError::DISCONNECTED);
  io_handler_->CancelWrite(serial::SendError::DISCONNECTED);
}

void SerialConnection::GetInfo(const GetInfoCallback& callback) {
  callback.Run(io_handler_->GetPortInfo());
}

void SerialConnection::SetOptions(serial::ConnectionOptionsPtr options,
                                  const SetOptionsCallback& callback) {
  callback.Run(io_handler_->ConfigurePort(*options));
  io_handler_->CancelRead(device::serial::ReceiveError::NONE);
}

void SerialConnection::SetControlSignals(
    serial::HostControlSignalsPtr signals,
    const SetControlSignalsCallback& callback) {
  callback.Run(io_handler_->SetControlSignals(*signals));
}

void SerialConnection::GetControlSignals(
    const GetControlSignalsCallback& callback) {
  callback.Run(io_handler_->GetControlSignals());
}

void SerialConnection::Flush(const FlushCallback& callback) {
  callback.Run(io_handler_->Flush());
}

void SerialConnection::OnSendCancelled(int32_t error) {
  io_handler_->CancelWrite(static_cast<serial::SendError>(error));
}

void SerialConnection::OnSendPipeReady(std::unique_ptr<ReadOnlyBuffer> buffer) {
  io_handler_->Write(std::move(buffer));
}

void SerialConnection::OnReceivePipeReady(
    std::unique_ptr<WritableBuffer> buffer) {
  io_handler_->Read(std::move(buffer));
}

}  // namespace device
