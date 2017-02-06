// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef DEVICE_SERIAL_SERIAL_IO_HANDLER_POSIX_H_
#define DEVICE_SERIAL_SERIAL_IO_HANDLER_POSIX_H_

#include "base/macros.h"
#include "base/message_loop/message_loop.h"
#include "base/single_thread_task_runner.h"
#include "base/threading/thread_task_runner_handle.h"
#include "device/serial/serial_io_handler.h"

namespace device {

// Linux reports breaks and parity errors by inserting the sequence '\377\0x'
// into the byte stream where 'x' is '\0' for a break and the corrupted byte for
// a parity error.
enum class ErrorDetectState { NO_ERROR, MARK_377_SEEN, MARK_0_SEEN };

class SerialIoHandlerPosix : public SerialIoHandler,
                             public base::MessageLoopForIO::Watcher {
 protected:
  // SerialIoHandler impl.
  void ReadImpl() override;
  void WriteImpl() override;
  void CancelReadImpl() override;
  void CancelWriteImpl() override;
  bool ConfigurePortImpl() override;
  bool Flush() const override;
  serial::DeviceControlSignalsPtr GetControlSignals() const override;
  bool SetControlSignals(
      const serial::HostControlSignals& control_signals) override;
  serial::ConnectionInfoPtr GetPortInfo() const override;
  bool SetBreak() override;
  bool ClearBreak() override;
  int CheckReceiveError(char* buffer,
                        int buffer_len,
                        int bytes_read,
                        bool& break_detected,
                        bool& parity_error_detected);

 private:
  friend class SerialIoHandler;
  friend class SerialIoHandlerPosixTest;

  SerialIoHandlerPosix(
      scoped_refptr<base::SingleThreadTaskRunner> file_thread_task_runner,
      scoped_refptr<base::SingleThreadTaskRunner> ui_thread_task_runner);
  ~SerialIoHandlerPosix() override;

  // base::MessageLoopForIO::Watcher implementation.
  void OnFileCanWriteWithoutBlocking(int fd) override;
  void OnFileCanReadWithoutBlocking(int fd) override;

  void EnsureWatchingReads();
  void EnsureWatchingWrites();

  base::MessageLoopForIO::FileDescriptorWatcher file_read_watcher_;
  base::MessageLoopForIO::FileDescriptorWatcher file_write_watcher_;

  // Flags indicating if the message loop is watching the device for IO events.
  bool is_watching_reads_;
  bool is_watching_writes_;

  ErrorDetectState error_detect_state_;
  bool parity_check_enabled_;
  char chars_stashed_[2];
  int num_chars_stashed_;

  DISALLOW_COPY_AND_ASSIGN(SerialIoHandlerPosix);
};

}  // namespace device

#endif  // DEVICE_SERIAL_SERIAL_IO_HANDLER_POSIX_H_
