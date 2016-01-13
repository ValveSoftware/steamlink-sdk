// Copyright (c) 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "device/hid/hid_connection_win.h"

#include <cstring>

#include "base/files/file.h"
#include "base/message_loop/message_loop.h"
#include "base/stl_util.h"
#include "base/threading/thread_restrictions.h"
#include "base/win/object_watcher.h"
#include "base/win/scoped_handle.h"
#include "device/hid/hid_service.h"
#include "device/hid/hid_service_win.h"

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
  PendingHidTransfer(scoped_refptr<HidConnectionWin> connection,
                     scoped_refptr<net::IOBufferWithSize> target_buffer,
                     scoped_refptr<net::IOBufferWithSize> receive_buffer,
                     HidConnection::IOCallback callback);

  void TakeResultFromWindowsAPI(BOOL result);

  OVERLAPPED* GetOverlapped() { return &overlapped_; }

  // Implements base::win::ObjectWatcher::Delegate.
  virtual void OnObjectSignaled(HANDLE object) OVERRIDE;

  // Implements base::MessageLoop::DestructionObserver
  virtual void WillDestroyCurrentMessageLoop() OVERRIDE;

  scoped_refptr<HidConnectionWin> connection_;
  scoped_refptr<net::IOBufferWithSize> target_buffer_;
  scoped_refptr<net::IOBufferWithSize> receive_buffer_;
  HidConnection::IOCallback callback_;
  OVERLAPPED overlapped_;
  base::win::ScopedHandle event_;
  base::win::ObjectWatcher watcher_;

 private:
  friend class base::RefCounted<PendingHidTransfer>;

  virtual ~PendingHidTransfer();

  DISALLOW_COPY_AND_ASSIGN(PendingHidTransfer);
};

PendingHidTransfer::PendingHidTransfer(
    scoped_refptr<HidConnectionWin> connection,
    scoped_refptr<net::IOBufferWithSize> target_buffer,
    scoped_refptr<net::IOBufferWithSize> receive_buffer,
    HidConnection::IOCallback callback)
    : connection_(connection),
      target_buffer_(target_buffer),
      receive_buffer_(receive_buffer),
      callback_(callback),
      event_(CreateEvent(NULL, FALSE, FALSE, NULL)) {
  memset(&overlapped_, 0, sizeof(OVERLAPPED));
  overlapped_.hEvent = event_.Get();
}

PendingHidTransfer::~PendingHidTransfer() {
  base::MessageLoop::current()->RemoveDestructionObserver(this);
}

void PendingHidTransfer::TakeResultFromWindowsAPI(BOOL result) {
  if (result || GetLastError() != ERROR_IO_PENDING) {
    connection_->OnTransferFinished(this);
  } else {
    base::MessageLoop::current()->AddDestructionObserver(this);
    AddRef();
    watcher_.StartWatching(event_.Get(), this);
  }
}

void PendingHidTransfer::OnObjectSignaled(HANDLE event_handle) {
  connection_->OnTransferFinished(this);
  Release();
}

void PendingHidTransfer::WillDestroyCurrentMessageLoop() {
  watcher_.StopWatching();
  connection_->OnTransferCanceled(this);
}

HidConnectionWin::HidConnectionWin(const HidDeviceInfo& device_info)
    : HidConnection(device_info) {
  DCHECK(thread_checker_.CalledOnValidThread());
  file_.Set(CreateFileA(device_info.device_id.c_str(),
                        GENERIC_WRITE | GENERIC_READ,
                        FILE_SHARE_READ | FILE_SHARE_WRITE,
                        NULL,
                        OPEN_EXISTING,
                        FILE_FLAG_OVERLAPPED,
                        NULL));

  if (!file_.IsValid() &&
      GetLastError() == base::File::FILE_ERROR_ACCESS_DENIED) {
    file_.Set(CreateFileA(device_info.device_id.c_str(),
                          GENERIC_READ,
                          FILE_SHARE_READ,
                          NULL,
                          OPEN_EXISTING,
                          FILE_FLAG_OVERLAPPED,
                          NULL));
  }
}

bool HidConnectionWin::available() const {
  return file_.IsValid();
}

HidConnectionWin::~HidConnectionWin() {
  DCHECK(thread_checker_.CalledOnValidThread());
  CancelIo(file_.Get());
}

void HidConnectionWin::Read(scoped_refptr<net::IOBufferWithSize> buffer,
                            const HidConnection::IOCallback& callback) {
  DCHECK(thread_checker_.CalledOnValidThread());
  if (device_info().input_report_size == 0) {
    // The device does not support input reports.
    callback.Run(false, 0);
    return;
  }

  // This fairly awkward logic is correct: If Windows does not expect a device
  // to supply a report ID in its input reports, it requires the buffer to be
  // 1 byte larger than what the device actually sends.
  int receive_buffer_size = device_info().input_report_size;
  int expected_buffer_size = receive_buffer_size;
  if (!device_info().has_report_id)
    expected_buffer_size -= 1;

  if (buffer->size() < expected_buffer_size) {
    callback.Run(false, 0);
    return;
  }

  scoped_refptr<net::IOBufferWithSize> receive_buffer(buffer);
  if (receive_buffer_size != expected_buffer_size)
    receive_buffer = new net::IOBufferWithSize(receive_buffer_size);
  scoped_refptr<PendingHidTransfer> transfer(
      new PendingHidTransfer(this, buffer, receive_buffer, callback));
  transfers_.insert(transfer);
  transfer->TakeResultFromWindowsAPI(
      ReadFile(file_.Get(),
               receive_buffer->data(),
               static_cast<DWORD>(receive_buffer->size()),
               NULL,
               transfer->GetOverlapped()));
}

void HidConnectionWin::Write(uint8_t report_id,
                             scoped_refptr<net::IOBufferWithSize> buffer,
                             const HidConnection::IOCallback& callback) {
  DCHECK(thread_checker_.CalledOnValidThread());
  if (device_info().output_report_size == 0) {
    // The device does not support output reports.
    callback.Run(false, 0);
    return;
  }

  // The Windows API always wants either a report ID (if supported) or
  // zero at the front of every output report.
  scoped_refptr<net::IOBufferWithSize> output_buffer(buffer);
  output_buffer = new net::IOBufferWithSize(buffer->size() + 1);
  output_buffer->data()[0] = report_id;
  memcpy(output_buffer->data() + 1, buffer->data(), buffer->size());

  scoped_refptr<PendingHidTransfer> transfer(
      new PendingHidTransfer(this, output_buffer, NULL, callback));
  transfers_.insert(transfer);
  transfer->TakeResultFromWindowsAPI(
      WriteFile(file_.Get(),
                output_buffer->data(),
                static_cast<DWORD>(output_buffer->size()),
                NULL,
                transfer->GetOverlapped()));
}

void HidConnectionWin::GetFeatureReport(
    uint8_t report_id,
    scoped_refptr<net::IOBufferWithSize> buffer,
    const IOCallback& callback) {
  DCHECK(thread_checker_.CalledOnValidThread());
  if (device_info().feature_report_size == 0) {
    // The device does not support feature reports.
    callback.Run(false, 0);
    return;
  }

  int receive_buffer_size = device_info().feature_report_size;
  int expected_buffer_size = receive_buffer_size;
  if (!device_info().has_report_id)
    expected_buffer_size -= 1;
  if (buffer->size() < expected_buffer_size) {
    callback.Run(false, 0);
    return;
  }

  scoped_refptr<net::IOBufferWithSize> receive_buffer(buffer);
  if (receive_buffer_size != expected_buffer_size)
    receive_buffer = new net::IOBufferWithSize(receive_buffer_size);

  // The first byte of the destination buffer is the report ID being requested.
  receive_buffer->data()[0] = report_id;
  scoped_refptr<PendingHidTransfer> transfer(
      new PendingHidTransfer(this, buffer, receive_buffer, callback));
  transfers_.insert(transfer);
  transfer->TakeResultFromWindowsAPI(
      DeviceIoControl(file_.Get(),
                      IOCTL_HID_GET_FEATURE,
                      NULL,
                      0,
                      receive_buffer->data(),
                      static_cast<DWORD>(receive_buffer->size()),
                      NULL,
                      transfer->GetOverlapped()));
}

void HidConnectionWin::SendFeatureReport(
    uint8_t report_id,
    scoped_refptr<net::IOBufferWithSize> buffer,
    const IOCallback& callback) {
  DCHECK(thread_checker_.CalledOnValidThread());
  if (device_info().feature_report_size == 0) {
    // The device does not support feature reports.
    callback.Run(false, 0);
    return;
  }

  // The Windows API always wants either a report ID (if supported) or
  // zero at the front of every output report.
  scoped_refptr<net::IOBufferWithSize> output_buffer(buffer);
  output_buffer = new net::IOBufferWithSize(buffer->size() + 1);
  output_buffer->data()[0] = report_id;
  memcpy(output_buffer->data() + 1, buffer->data(), buffer->size());

  scoped_refptr<PendingHidTransfer> transfer(
      new PendingHidTransfer(this, output_buffer, NULL, callback));
  transfer->TakeResultFromWindowsAPI(
      DeviceIoControl(file_.Get(),
                      IOCTL_HID_SET_FEATURE,
                      output_buffer->data(),
                      static_cast<DWORD>(output_buffer->size()),
                      NULL,
                      0,
                      NULL,
                      transfer->GetOverlapped()));
}

void HidConnectionWin::OnTransferFinished(
    scoped_refptr<PendingHidTransfer> transfer) {
  DWORD bytes_transferred;
  transfers_.erase(transfer);
  if (GetOverlappedResult(
          file_, transfer->GetOverlapped(), &bytes_transferred, FALSE)) {
    if (bytes_transferred == 0)
      transfer->callback_.Run(true, 0);
    // If this is an input transfer and the receive buffer is not the same as
    // the target buffer, we need to copy the receive buffer into the target
    // buffer, discarding the first byte. This is because the target buffer's
    // owner is not expecting a report ID but Windows will always provide one.
    if (transfer->receive_buffer_ &&
        transfer->receive_buffer_ != transfer->target_buffer_) {
      // Move one byte forward.
      --bytes_transferred;
      memcpy(transfer->target_buffer_->data(),
             transfer->receive_buffer_->data() + 1,
             bytes_transferred);
    }
    transfer->callback_.Run(true, bytes_transferred);
  } else {
    transfer->callback_.Run(false, 0);
  }
}

void HidConnectionWin::OnTransferCanceled(
    scoped_refptr<PendingHidTransfer> transfer) {
  transfers_.erase(transfer);
  transfer->callback_.Run(false, 0);
}

}  // namespace device
