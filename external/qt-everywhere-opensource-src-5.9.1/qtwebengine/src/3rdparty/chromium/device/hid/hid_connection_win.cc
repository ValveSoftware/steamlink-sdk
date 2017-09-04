// Copyright (c) 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "device/hid/hid_connection_win.h"

#include <cstring>
#include <utility>

#include "base/bind.h"
#include "base/files/file.h"
#include "base/macros.h"
#include "base/memory/ptr_util.h"
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

class PendingHidTransfer : public base::win::ObjectWatcher::Delegate,
                           public base::MessageLoop::DestructionObserver {
 public:
  typedef base::OnceCallback<void(PendingHidTransfer*, bool)> Callback;

  PendingHidTransfer(scoped_refptr<net::IOBuffer> buffer, Callback callback);
  ~PendingHidTransfer() override;

  void TakeResultFromWindowsAPI(BOOL result);

  OVERLAPPED* GetOverlapped() { return &overlapped_; }

  // Implements base::win::ObjectWatcher::Delegate.
  void OnObjectSignaled(HANDLE object) override;

  // Implements base::MessageLoop::DestructionObserver
  void WillDestroyCurrentMessageLoop() override;

 private:
  // The buffer isn't used by this object but it's important that a reference
  // to it is held until the transfer completes.
  scoped_refptr<net::IOBuffer> buffer_;
  Callback callback_;
  OVERLAPPED overlapped_;
  base::win::ScopedHandle event_;
  base::win::ObjectWatcher watcher_;

  DISALLOW_COPY_AND_ASSIGN(PendingHidTransfer);
};

PendingHidTransfer::PendingHidTransfer(scoped_refptr<net::IOBuffer> buffer,
                                       PendingHidTransfer::Callback callback)
    : buffer_(buffer),
      callback_(std::move(callback)),
      event_(CreateEvent(NULL, FALSE, FALSE, NULL)) {
  memset(&overlapped_, 0, sizeof(OVERLAPPED));
  overlapped_.hEvent = event_.Get();
}

PendingHidTransfer::~PendingHidTransfer() {
  base::MessageLoop::current()->RemoveDestructionObserver(this);
  if (callback_)
    std::move(callback_).Run(this, false);
}

void PendingHidTransfer::TakeResultFromWindowsAPI(BOOL result) {
  if (result) {
    std::move(callback_).Run(this, true);
  } else if (GetLastError() == ERROR_IO_PENDING) {
    base::MessageLoop::current()->AddDestructionObserver(this);
    watcher_.StartWatchingOnce(event_.Get(), this);
  } else {
    HID_PLOG(EVENT) << "HID transfer failed";
    std::move(callback_).Run(this, false);
  }
}

void PendingHidTransfer::OnObjectSignaled(HANDLE event_handle) {
  std::move(callback_).Run(this, true);
}

void PendingHidTransfer::WillDestroyCurrentMessageLoop() {
  watcher_.StopWatching();
  std::move(callback_).Run(this, false);
}

HidConnectionWin::HidConnectionWin(scoped_refptr<HidDeviceInfo> device_info,
                                   base::win::ScopedHandle file)
    : HidConnection(device_info), file_(std::move(file)) {}

HidConnectionWin::~HidConnectionWin() {
  DCHECK(!file_.IsValid());
  DCHECK(transfers_.empty());
}

void HidConnectionWin::PlatformClose() {
  CancelIo(file_.Get());
  file_.Close();
  transfers_.clear();
}

void HidConnectionWin::PlatformRead(
    const HidConnection::ReadCallback& callback) {
  // Windows will always include the report ID (including zero if report IDs
  // are not in use) in the buffer.
  scoped_refptr<net::IOBufferWithSize> buffer = new net::IOBufferWithSize(
      base::checked_cast<int>(device_info()->max_input_report_size() + 1));
  transfers_.push_back(base::MakeUnique<PendingHidTransfer>(
      buffer, base::BindOnce(&HidConnectionWin::OnReadComplete, this, buffer,
                             callback)));
  transfers_.back()->TakeResultFromWindowsAPI(
      ReadFile(file_.Get(), buffer->data(), static_cast<DWORD>(buffer->size()),
               NULL, transfers_.back()->GetOverlapped()));
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
  transfers_.push_back(base::MakeUnique<PendingHidTransfer>(
      buffer,
      base::BindOnce(&HidConnectionWin::OnWriteComplete, this, callback)));
  transfers_.back()->TakeResultFromWindowsAPI(
      WriteFile(file_.Get(), buffer->data(), static_cast<DWORD>(size), NULL,
                transfers_.back()->GetOverlapped()));
}

void HidConnectionWin::PlatformGetFeatureReport(uint8_t report_id,
                                                const ReadCallback& callback) {
  // The first byte of the destination buffer is the report ID being requested.
  scoped_refptr<net::IOBufferWithSize> buffer = new net::IOBufferWithSize(
      base::checked_cast<int>(device_info()->max_feature_report_size() + 1));
  buffer->data()[0] = report_id;

  transfers_.push_back(base::MakeUnique<PendingHidTransfer>(
      buffer, base::BindOnce(&HidConnectionWin::OnReadFeatureComplete, this,
                             buffer, callback)));
  transfers_.back()->TakeResultFromWindowsAPI(
      DeviceIoControl(file_.Get(), IOCTL_HID_GET_FEATURE, NULL, 0,
                      buffer->data(), static_cast<DWORD>(buffer->size()), NULL,
                      transfers_.back()->GetOverlapped()));
}

void HidConnectionWin::PlatformSendFeatureReport(
    scoped_refptr<net::IOBuffer> buffer,
    size_t size,
    const WriteCallback& callback) {
  // The Windows API always wants either a report ID (if supported) or
  // zero at the front of every output report.
  transfers_.push_back(base::MakeUnique<PendingHidTransfer>(
      buffer,
      base::BindOnce(&HidConnectionWin::OnWriteComplete, this, callback)));
  transfers_.back()->TakeResultFromWindowsAPI(
      DeviceIoControl(file_.Get(), IOCTL_HID_SET_FEATURE, buffer->data(),
                      static_cast<DWORD>(size), NULL, 0, NULL,
                      transfers_.back()->GetOverlapped()));
}

void HidConnectionWin::OnReadComplete(scoped_refptr<net::IOBuffer> buffer,
                                      const ReadCallback& callback,
                                      PendingHidTransfer* transfer_raw,
                                      bool signaled) {
  if (!file_.IsValid()) {
    callback.Run(false, nullptr, 0);
    return;
  }

  std::unique_ptr<PendingHidTransfer> transfer = UnlinkTransfer(transfer_raw);
  DWORD bytes_transferred;
  if (signaled && GetOverlappedResult(file_.Get(), transfer->GetOverlapped(),
                                      &bytes_transferred, FALSE)) {
    CompleteRead(buffer, bytes_transferred, callback);
  } else {
    HID_PLOG(EVENT) << "HID read failed";
    callback.Run(false, nullptr, 0);
  }
}

void HidConnectionWin::OnReadFeatureComplete(
    scoped_refptr<net::IOBuffer> buffer,
    const ReadCallback& callback,
    PendingHidTransfer* transfer_raw,
    bool signaled) {
  if (!file_.IsValid()) {
    callback.Run(false, nullptr, 0);
    return;
  }

  std::unique_ptr<PendingHidTransfer> transfer = UnlinkTransfer(transfer_raw);
  DWORD bytes_transferred;
  if (signaled && GetOverlappedResult(file_.Get(), transfer->GetOverlapped(),
                                      &bytes_transferred, FALSE)) {
    callback.Run(true, buffer, bytes_transferred);
  } else {
    HID_PLOG(EVENT) << "HID read failed";
    callback.Run(false, nullptr, 0);
  }
}

void HidConnectionWin::OnWriteComplete(const WriteCallback& callback,
                                       PendingHidTransfer* transfer_raw,
                                       bool signaled) {
  if (!file_.IsValid()) {
    callback.Run(false);
    return;
  }

  std::unique_ptr<PendingHidTransfer> transfer = UnlinkTransfer(transfer_raw);
  DWORD bytes_transferred;
  if (signaled && GetOverlappedResult(file_.Get(), transfer->GetOverlapped(),
                                      &bytes_transferred, FALSE)) {
    callback.Run(true);
  } else {
    HID_PLOG(EVENT) << "HID write failed";
    callback.Run(false);
  }
}

std::unique_ptr<PendingHidTransfer> HidConnectionWin::UnlinkTransfer(
    PendingHidTransfer* transfer) {
  auto it = std::find_if(
      transfers_.begin(), transfers_.end(),
      [transfer](const std::unique_ptr<PendingHidTransfer>& this_transfer) {
        return transfer == this_transfer.get();
      });
  DCHECK(it != transfers_.end());
  std::unique_ptr<PendingHidTransfer> saved_transfer = std::move(*it);
  transfers_.erase(it);
  return saved_transfer;
}

}  // namespace device
