// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "device/serial/test_serial_io_handler.h"

#include <stddef.h>
#include <stdint.h>

#include <algorithm>

#include "base/bind.h"
#include "device/serial/serial.mojom.h"

namespace device {

TestSerialIoHandler::TestSerialIoHandler()
    : SerialIoHandler(NULL, NULL),
      opened_(false),
      dtr_(false),
      rts_(false),
      flushes_(0) {
}

scoped_refptr<SerialIoHandler> TestSerialIoHandler::Create() {
  return scoped_refptr<SerialIoHandler>(new TestSerialIoHandler);
}

void TestSerialIoHandler::Open(const std::string& port,
                               const serial::ConnectionOptions& options,
                               const OpenCompleteCallback& callback) {
  DCHECK(!opened_);
  opened_ = true;
  ConfigurePort(options);
  callback.Run(true);
}

void TestSerialIoHandler::ReadImpl() {
  if (!pending_read_buffer())
    return;
  if (buffer_.empty())
    return;

  size_t num_bytes =
      std::min(buffer_.size(), static_cast<size_t>(pending_read_buffer_len()));
  memcpy(pending_read_buffer(), buffer_.c_str(), num_bytes);
  buffer_ = buffer_.substr(num_bytes);
  ReadCompleted(static_cast<uint32_t>(num_bytes), serial::ReceiveError::NONE);
}

void TestSerialIoHandler::CancelReadImpl() {
  ReadCompleted(0, read_cancel_reason());
}

void TestSerialIoHandler::WriteImpl() {
  if (!send_callback_.is_null()) {
    base::Closure callback = send_callback_;
    send_callback_.Reset();
    callback.Run();
    return;
  }
  buffer_ += std::string(pending_write_buffer(), pending_write_buffer_len());
  WriteCompleted(pending_write_buffer_len(), serial::SendError::NONE);
  if (pending_read_buffer())
    ReadImpl();
}

void TestSerialIoHandler::CancelWriteImpl() {
  WriteCompleted(0, write_cancel_reason());
}

bool TestSerialIoHandler::ConfigurePortImpl() {
  info_.bitrate = options().bitrate;
  info_.data_bits = options().data_bits;
  info_.parity_bit = options().parity_bit;
  info_.stop_bits = options().stop_bits;
  info_.cts_flow_control = options().cts_flow_control;
  return true;
}

serial::DeviceControlSignalsPtr TestSerialIoHandler::GetControlSignals() const {
  serial::DeviceControlSignalsPtr signals(serial::DeviceControlSignals::New());
  *signals = device_control_signals_;
  return signals;
}

serial::ConnectionInfoPtr TestSerialIoHandler::GetPortInfo() const {
  serial::ConnectionInfoPtr info(serial::ConnectionInfo::New());
  *info = info_;
  return info;
}

bool TestSerialIoHandler::Flush() const {
  flushes_++;
  return true;
}

bool TestSerialIoHandler::SetControlSignals(
    const serial::HostControlSignals& signals) {
  if (signals.has_dtr)
    dtr_ = signals.dtr;
  if (signals.has_rts)
    rts_ = signals.rts;
  return true;
}

bool TestSerialIoHandler::SetBreak() {
  return true;
}

bool TestSerialIoHandler::ClearBreak() {
  return true;
}

TestSerialIoHandler::~TestSerialIoHandler() {
}

}  // namespace device
