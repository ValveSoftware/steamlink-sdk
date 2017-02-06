// Copyright (c) 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "device/hid/hid_connection_win.h"

#include <cstring>
#include <utility>

#include "base/bind.h"
#include "base/files/file.h"
#include "base/macros.h"
#include "base/message_loop/message_loop.h"
#include "base/numerics/safe_conversions.h"
#include "base/win/object_watcher.h"
#include "components/device_event_log/device_event_log.h"

#define INITGUID

#include <windows.h>
#include <hidclass.h>

extern "C" {
#include <hidsdi.h>
}

#include <setupapi.h>
#include <winioctl.h>

namespace device {

struct PendingHidTransfer : public base::RefCounted<PendingHidTransfer>,
                            public base::win::ObjectWatcher::Delegate,
                            public base::MessageLoop::DestructionObserver {
  typedef base::Callback<void(PendingHidTransfer*, bool)> Callback;

  PendingHidTransfer(scoped_refptr<net::IOBuffer> buffer,
                     const Callback& callback);

  void TakeResultFromWindowsAPI(BOOL result);

  OVERLAPPED* GetOverlapped() { return &overlapped_; }

  // Implements base::win::ObjectWatcher::Delegate.
  void OnObjectSignaled(HANDLE object) override;

  // Implements base::MessageLoop::DestructionObserver
  void WillDestroyCurrentMessageLoop() override;

  // The buffer isn't used by this object but it's important that a reference
  // to it is held until the transfer completes.
  scoped_refptr<net::IOBuffer> buffer_;
  Callback callback_;
  OVERLAPPED overlapped_;
  base::win::ScopedHandle event_;
  base::win::ObjectWatcher watcher_;

 private:
  friend class base::RefCounted<PendingHidTransfer>;

  ~PendingHidTransfer() override;

  DISALLOW_COPY_AND_ASSIGN(PendingHidTransfer);
};

PendingHidTransfer::PendingHidTransfer(
    scoped_refptr<net::IOBuffer> buffer,
    const PendingHidTransfer::Callback& callback)
    : buffer_(buffer),
      callback_(callback),
      event_(CreateEvent(NULL, FALSE, FALSE, NULL)) {
  memset(&overlapped_, 0, sizeof(OVERLAPPED));
  overlapped_.hEvent = event_.Get();
}

PendingHidTransfer::~PendingHidTransfer() {
  base::MessageLoop::current()->RemoveDestructionObserver(this);
}

void PendingHidTransfer::TakeResultFromWindowsAPI(BOOL result) {
  if (result) {
    callback_.Run(this, true);
  } else if (GetLastError() == ERROR_IO_PENDING) {
    base::MessageLoop::current()->AddDestructionObserver(this);
    AddRef();
    watcher_.StartWatchingOnce(event_.Get(), this);
  } else {
    HID_PLOG(EVENT) << "HID transfer failed";
    callback_.Run(this, false);
  }
}

void PendingHidTransfer::OnObjectSignaled(HANDLE event_handle) {
  callback_.Run(this, true);
  Release();
}

void PendingHidTransfer::WillDestroyCurrentMessageLoop() {
  watcher_.StopWatching();
  callback_.Run(this, false);
}

HidConnectionWin::HidConnectionWin(scoped_refptr<HidDeviceInfo> device_info,
                                   base::win::ScopedHandle file)
    : HidConnection(device_info) {
  file_ = std::move(file);
}

HidConnectionWin::~HidConnectionWin() {
}

void HidConnectionWin::PlatformClose() {
  CancelIo(file_.Get());
}

void HidConnectionWin::PlatformRead(
    const HidConnection::ReadCallback& callback) {
  // Windows will always include the report ID (including zero if report IDs
  // are not in use) in the buffer.
  scoped_refptr<net::IOBufferWithSize> buffer = new net::IOBufferWithSize(
      base::checked_cast<int>(device_info()->max_input_report_size() + 1));
  scoped_refptr<PendingHidTransfer> transfer(new PendingHidTransfer(
      buffer,
      base::Bind(&HidConnectionWin::OnReadComplete, this, buffer, callback)));
  transfers_.insert(transfer);
  transfer->TakeResultFromWindowsAPI(
      ReadFile(file_.Get(),
               buffer->data(),
               static_cast<DWORD>(buffer->size()),
               NULL,
               transfer->GetOverlapped()));
}

void HidConnectionWin::PlatformWrite(scoped_refptr<net::IOBuffer> buffer,
                                     size_t size,
                                     const WriteCallback& callback) {
  size_t expected_size = device_info()->max_output_report_size() + 1;
  DCHECK(size <= expected_size);
  // The Windows API always wants either a report ID (if supported) or zero at
  // the front of every output report and requires that the buffer size be equal
  // to the maximum output report size supported by this collection.
  if (size < expected_size) {
    scoped_refptr<net::IOBuffer> tmp_buffer = new net::IOBuffer(
        base::checked_cast<int>(expected_size));
    memcpy(tmp_buffer->data(), buffer->data(), size);
    memset(tmp_buffer->data() + size, 0, expected_size - size);
    buffer = tmp_buffer;
    size = expected_size;
  }
  scoped_refptr<PendingHidTransfer> transfer(new PendingHidTransfer(
      buffer, base::Bind(&HidConnectionWin::OnWriteComplete, this, callback)));
  transfers_.insert(transfer);
  transfer->TakeResultFromWindowsAPI(WriteFile(file_.Get(),
                                               buffer->data(),
                                               static_cast<DWORD>(size),
                                               NULL,
                                               transfer->GetOverlapped()));
}

void HidConnectionWin::PlatformGetFeatureReport(uint8_t report_id,
                                                const ReadCallback& callback) {
  // The first byte of the destination buffer is the report ID being requested.
  scoped_refptr<net::IOBufferWithSize> buffer = new net::IOBufferWithSize(
      base::checked_cast<int>(device_info()->max_feature_report_size() + 1));
  buffer->data()[0] = report_id;

  scoped_refptr<PendingHidTransfer> transfer(new PendingHidTransfer(
      buffer,
      base::Bind(
          &HidConnectionWin::OnReadFeatureComplete, this, buffer, callback)));
  transfers_.insert(transfer);
  transfer->TakeResultFromWindowsAPI(
      DeviceIoControl(file_.Get(),
                      IOCTL_HID_GET_FEATURE,
                      NULL,
                      0,
                      buffer->data(),
                      static_cast<DWORD>(buffer->size()),
                      NULL,
                      transfer->GetOverlapped()));
}

void HidConnectionWin::PlatformSendFeatureReport(
    scoped_refptr<net::IOBuffer> buffer,
    size_t size,
    const WriteCallback& callback) {
  // The Windows API always wants either a report ID (if supported) or
  // zero at the front of every output report.
  scoped_refptr<PendingHidTransfer> transfer(new PendingHidTransfer(
      buffer, base::Bind(&HidConnectionWin::OnWriteComplete, this, callback)));
  transfer->TakeResultFromWindowsAPI(
      DeviceIoControl(file_.Get(),
                      IOCTL_HID_SET_FEATURE,
                      buffer->data(),
                      static_cast<DWORD>(size),
                      NULL,
                      0,
                      NULL,
                      transfer->GetOverlapped()));
}

void HidConnectionWin::OnReadComplete(scoped_refptr<net::IOBuffer> buffer,
                                      const ReadCallback& callback,
                                      PendingHidTransfer* transfer,
                                      bool signaled) {
  if (!signaled) {
    callback.Run(false, NULL, 0);
    return;
  }

  DWORD bytes_transferred;
  if (GetOverlappedResult(
          file_.Get(), transfer->GetOverlapped(), &bytes_transferred, FALSE)) {
    CompleteRead(buffer, bytes_transferred, callback);
  } else {
    HID_PLOG(EVENT) << "HID read failed";
    callback.Run(false, NULL, 0);
  }
}

void HidConnectionWin::OnReadFeatureComplete(
    scoped_refptr<net::IOBuffer> buffer,
    const ReadCallback& callback,
    PendingHidTransfer* transfer,
    bool signaled) {
  if (!signaled) {
    callback.Run(false, NULL, 0);
    return;
  }

  DWORD bytes_transferred;
  if (GetOverlappedResult(
          file_.Get(), transfer->GetOverlapped(), &bytes_transferred, FALSE)) {
    callback.Run(true, buffer, bytes_transferred);
  } else {
    HID_PLOG(EVENT) << "HID read failed";
    callback.Run(false, NULL, 0);
  }
}

void HidConnectionWin::OnWriteComplete(const WriteCallback& callback,
                                       PendingHidTransfer* transfer,
                                       bool signaled) {
  if (!signaled) {
    callback.Run(false);
    return;
  }

  DWORD bytes_transferred;
  if (GetOverlappedResult(
          file_.Get(), transfer->GetOverlapped(), &bytes_transferred, FALSE)) {
    callback.Run(true);
  } else {
    HID_PLOG(EVENT) << "HID write failed";
    callback.Run(false);
  }
}

}  // namespace device
