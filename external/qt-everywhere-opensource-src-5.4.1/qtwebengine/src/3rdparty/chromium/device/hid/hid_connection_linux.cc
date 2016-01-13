// Copyright (c) 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "device/hid/hid_connection_linux.h"

#include <errno.h>
#include <fcntl.h>
#include <libudev.h>
#include <linux/hidraw.h>
#include <sys/ioctl.h>

#include <string>

#include "base/files/file_path.h"
#include "base/posix/eintr_wrapper.h"
#include "base/threading/thread_restrictions.h"
#include "base/tuple.h"
#include "device/hid/hid_service.h"

// These are already defined in newer versions of linux/hidraw.h.
#ifndef HIDIOCSFEATURE
#define HIDIOCSFEATURE(len) _IOC(_IOC_WRITE | _IOC_READ, 'H', 0x06, len)
#endif
#ifndef HIDIOCGFEATURE
#define HIDIOCGFEATURE(len) _IOC(_IOC_WRITE | _IOC_READ, 'H', 0x07, len)
#endif

namespace device {

namespace {

// Copies a buffer into a new one with a report ID byte inserted at the front.
scoped_refptr<net::IOBufferWithSize> CopyBufferWithReportId(
    scoped_refptr<net::IOBufferWithSize> buffer,
    uint8_t report_id) {
  scoped_refptr<net::IOBufferWithSize> new_buffer(
      new net::IOBufferWithSize(buffer->size() + 1));
  new_buffer->data()[0] = report_id;
  memcpy(new_buffer->data() + 1, buffer->data(), buffer->size());
  return new_buffer;
}

}  // namespace

HidConnectionLinux::HidConnectionLinux(HidDeviceInfo device_info,
                                       std::string dev_node)
    : HidConnection(device_info) {
  DCHECK(thread_checker_.CalledOnValidThread());

  int flags = base::File::FLAG_OPEN |
              base::File::FLAG_READ |
              base::File::FLAG_WRITE;

  base::File device_file(base::FilePath(dev_node), flags);
  if (!device_file.IsValid()) {
    base::File::Error file_error = device_file.error_details();

    if (file_error == base::File::FILE_ERROR_ACCESS_DENIED) {
      VLOG(1) << "Access denied opening device read-write, trying read-only.";

      flags = base::File::FLAG_OPEN | base::File::FLAG_READ;

      device_file = base::File(base::FilePath(dev_node), flags);
    }
  }
  if (!device_file.IsValid()) {
    LOG(ERROR) << "Failed to open '" << dev_node << "': "
        << base::File::ErrorToString(device_file.error_details());
    return;
  }

  if (fcntl(device_file.GetPlatformFile(), F_SETFL,
            fcntl(device_file.GetPlatformFile(), F_GETFL) | O_NONBLOCK)) {
    PLOG(ERROR) << "Failed to set non-blocking flag to device file.";
    return;
  }
  device_file_ = device_file.Pass();

  if (!base::MessageLoopForIO::current()->WatchFileDescriptor(
      device_file_.GetPlatformFile(),
      true,
      base::MessageLoopForIO::WATCH_READ_WRITE,
      &device_file_watcher_,
      this)) {
    LOG(ERROR) << "Failed to start watching device file.";
  }
}

HidConnectionLinux::~HidConnectionLinux() {
  DCHECK(thread_checker_.CalledOnValidThread());
  Disconnect();
}

void HidConnectionLinux::OnFileCanReadWithoutBlocking(int fd) {
  DCHECK(thread_checker_.CalledOnValidThread());
  DCHECK_EQ(fd, device_file_.GetPlatformFile());

  uint8 buffer[1024] = {0};
  int bytes_read =
      HANDLE_EINTR(read(device_file_.GetPlatformFile(), buffer, 1024));
  if (bytes_read < 0) {
    if (errno == EAGAIN) {
      return;
    }
    Disconnect();
    return;
  }

  PendingHidReport report;
  report.buffer = new net::IOBufferWithSize(bytes_read);
  memcpy(report.buffer->data(), buffer, bytes_read);
  pending_reports_.push(report);
  ProcessReadQueue();
}

void HidConnectionLinux::OnFileCanWriteWithoutBlocking(int fd) {}

void HidConnectionLinux::Disconnect() {
  DCHECK(thread_checker_.CalledOnValidThread());
  device_file_watcher_.StopWatchingFileDescriptor();
  device_file_.Close();
  while (!pending_reads_.empty()) {
    PendingHidRead pending_read = pending_reads_.front();
    pending_reads_.pop();
    pending_read.callback.Run(false, 0);
  }
}

void HidConnectionLinux::Read(scoped_refptr<net::IOBufferWithSize> buffer,
                              const IOCallback& callback) {
  DCHECK(thread_checker_.CalledOnValidThread());
  PendingHidRead pending_read;
  pending_read.buffer = buffer;
  pending_read.callback = callback;
  pending_reads_.push(pending_read);
  ProcessReadQueue();
}

void HidConnectionLinux::Write(uint8_t report_id,
                               scoped_refptr<net::IOBufferWithSize> buffer,
                               const IOCallback& callback) {
  DCHECK(thread_checker_.CalledOnValidThread());
  // If report ID is non-zero, insert it into a new copy of the buffer.
  if (report_id != 0)
    buffer = CopyBufferWithReportId(buffer, report_id);
  int bytes_written = HANDLE_EINTR(
      write(device_file_.GetPlatformFile(), buffer->data(), buffer->size()));
  if (bytes_written < 0) {
    Disconnect();
    callback.Run(false, 0);
  } else {
    callback.Run(true, bytes_written);
  }
}

void HidConnectionLinux::GetFeatureReport(
    uint8_t report_id,
    scoped_refptr<net::IOBufferWithSize> buffer,
    const IOCallback& callback) {
  DCHECK(thread_checker_.CalledOnValidThread());

  if (buffer->size() == 0) {
    callback.Run(false, 0);
    return;
  }

  // The first byte of the destination buffer is the report ID being requested.
  buffer->data()[0] = report_id;
  int result = ioctl(device_file_.GetPlatformFile(),
                     HIDIOCGFEATURE(buffer->size()),
                     buffer->data());
  if (result < 0)
    callback.Run(false, 0);
  else
    callback.Run(true, result);
}

void HidConnectionLinux::SendFeatureReport(
    uint8_t report_id,
    scoped_refptr<net::IOBufferWithSize> buffer,
    const IOCallback& callback) {
  DCHECK(thread_checker_.CalledOnValidThread());
  if (report_id != 0)
    buffer = CopyBufferWithReportId(buffer, report_id);
  int result = ioctl(device_file_.GetPlatformFile(),
                     HIDIOCSFEATURE(buffer->size()),
                     buffer->data());
  if (result < 0)
    callback.Run(false, 0);
  else
    callback.Run(true, result);
}

void HidConnectionLinux::ProcessReadQueue() {
  while (pending_reads_.size() && pending_reports_.size()) {
    PendingHidRead read = pending_reads_.front();
    pending_reads_.pop();
    PendingHidReport report = pending_reports_.front();
    if (report.buffer->size() > read.buffer->size()) {
      read.callback.Run(false, report.buffer->size());
    } else {
      memcpy(read.buffer->data(), report.buffer->data(), report.buffer->size());
      pending_reports_.pop();
      read.callback.Run(true, report.buffer->size());
    }
  }
}

}  // namespace device
