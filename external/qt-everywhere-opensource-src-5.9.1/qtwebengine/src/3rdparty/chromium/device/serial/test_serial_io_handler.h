// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef DEVICE_SERIAL_TEST_SERIAL_IO_HANDLER_H_
#define DEVICE_SERIAL_TEST_SERIAL_IO_HANDLER_H_

#include <string>

#include "base/callback.h"
#include "base/macros.h"
#include "device/serial/serial.mojom.h"
#include "device/serial/serial_io_handler.h"

namespace device {

class TestSerialIoHandler : public SerialIoHandler {
 public:
  TestSerialIoHandler();

  static scoped_refptr<SerialIoHandler> Create();

  // SerialIoHandler overrides.
  void Open(const std::string& port,
            const serial::ConnectionOptions& options,
            const OpenCompleteCallback& callback) override;
  void ReadImpl() override;
  void CancelReadImpl() override;
  void WriteImpl() override;
  void CancelWriteImpl() override;
  bool ConfigurePortImpl() override;
  serial::DeviceControlSignalsPtr GetControlSignals() const override;
  serial::ConnectionInfoPtr GetPortInfo() const override;
  bool Flush() const override;
  bool SetControlSignals(const serial::HostControlSignals& signals) override;
  bool SetBreak() override;
  bool ClearBreak() override;

  serial::ConnectionInfo* connection_info() { return &info_; }
  serial::DeviceControlSignals* device_control_signals() {
    return &device_control_signals_;
  }
  bool dtr() { return dtr_; }
  bool rts() { return rts_; }
  int flushes() { return flushes_; }
  // This callback will be called when this IoHandler processes its next write,
  // instead of the normal behavior of echoing the data to reads.
  void set_send_callback(const base::Closure& callback) {
    send_callback_ = callback;
  }

 protected:
  ~TestSerialIoHandler() override;

 private:
  bool opened_;
  serial::ConnectionInfo info_;
  serial::DeviceControlSignals device_control_signals_;
  bool dtr_;
  bool rts_;
  mutable int flushes_;
  std::string buffer_;
  base::Closure send_callback_;

  DISALLOW_COPY_AND_ASSIGN(TestSerialIoHandler);
};

}  // namespace device

#endif  // DEVICE_SERIAL_TEST_SERIAL_IO_HANDLER_H_
