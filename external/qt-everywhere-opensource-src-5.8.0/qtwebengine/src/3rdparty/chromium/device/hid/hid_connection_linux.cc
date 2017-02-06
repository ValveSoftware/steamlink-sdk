// Copyright (c) 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "device/hid/hid_connection_linux.h"

#include <errno.h>
#include <linux/hidraw.h>
#include <sys/ioctl.h>

#include <memory>
#include <string>
#include <utility>

#include "base/bind.h"
#include "base/files/file_path.h"
#include "base/macros.h"
#include "base/message_loop/message_loop.h"
#include "base/message_loop/message_pump_libevent.h"
#include "base/posix/eintr_wrapper.h"
#include "base/threading/thread_restrictions.h"
#include "base/threading/thread_task_runner_handle.h"
#include "components/device_event_log/device_event_log.h"
#include "device/hid/hid_service.h"

// These are already defined in newer versions of linux/hidraw.h.
#ifndef HIDIOCSFEATURE
#define HIDIOCSFEATURE(len) _IOC(_IOC_WRITE | _IOC_READ, 'H', 0x06, len)
#endif
#ifndef HIDIOCGFEATURE
#define HIDIOCGFEATURE(len) _IOC(_IOC_WRITE | _IOC_READ, 'H', 0x07, len)
#endif

namespace device {

class HidConnectionLinux::FileThreadHelper
    : public base::MessagePumpLibevent::Watcher,
      public base::MessageLoop::DestructionObserver {
 public:
  FileThreadHelper(base::PlatformFile platform_file,
                   scoped_refptr<HidDeviceInfo> device_info,
                   base::WeakPtr<HidConnectionLinux> connection,
                   scoped_refptr<base::SingleThreadTaskRunner> task_runner)
      : platform_file_(platform_file),
        connection_(connection),
        task_runner_(task_runner) {
    // Report buffers must always have room for the report ID.
    report_buffer_size_ = device_info->max_input_report_size() + 1;
    has_report_id_ = device_info->has_report_id();
  }

  ~FileThreadHelper() override {
    DCHECK(thread_checker_.CalledOnValidThread());
    base::MessageLoop::current()->RemoveDestructionObserver(this);
  }

  // Starts the FileDescriptorWatcher that reads input events from the device.
  // Must be called on a thread that has a base::MessageLoopForIO.
  static void Start(std::unique_ptr<FileThreadHelper> self) {
    base::ThreadRestrictions::AssertIOAllowed();
    self->thread_checker_.DetachFromThread();

    if (!base::MessageLoopForIO::current()->WatchFileDescriptor(
            self->platform_file_, true, base::MessageLoopForIO::WATCH_READ,
            &self->file_watcher_, self.get())) {
      HID_LOG(ERROR) << "Failed to start watching device file.";
    }

    // |self| is now owned by the current message loop.
    base::MessageLoop::current()->AddDestructionObserver(self.release());
  }

 private:
  // base::MessagePumpLibevent::Watcher implementation.
  void OnFileCanReadWithoutBlocking(int fd) override {
    DCHECK(thread_checker_.CalledOnValidThread());
    DCHECK_EQ(fd, platform_file_);

    scoped_refptr<net::IOBuffer> buffer(new net::IOBuffer(report_buffer_size_));
    char* data = buffer->data();
    size_t length = report_buffer_size_;
    if (!has_report_id_) {
      // Linux will not prefix the buffer with a report ID if report IDs are not
      // used by the device. Prefix the buffer with 0.
      *data++ = 0;
      length--;
    }

    ssize_t bytes_read = HANDLE_EINTR(read(platform_file_, data, length));
    if (bytes_read < 0) {
      if (errno != EAGAIN) {
        HID_PLOG(EVENT) << "Read failed";
        // This assumes that the error is unrecoverable and disables reading
        // from the device until it has been re-opened.
        // TODO(reillyg): Investigate starting and stopping the file descriptor
        // watcher in response to pending read requests so that per-request
        // errors can be returned to the client.
        file_watcher_.StopWatchingFileDescriptor();
      }
      return;
    }
    if (!has_report_id_) {
      // Behave as if the byte prefixed above as the the report ID was read.
      bytes_read++;
    }

    task_runner_->PostTask(FROM_HERE,
                           base::Bind(&HidConnectionLinux::ProcessInputReport,
                                      connection_, buffer, bytes_read));
  }

  void OnFileCanWriteWithoutBlocking(int fd) override {
    NOTREACHED();  // Only listening for reads.
  }

  // base::MessageLoop::DestructionObserver:
  void WillDestroyCurrentMessageLoop() override {
    DCHECK(thread_checker_.CalledOnValidThread());
    delete this;
  }

  base::ThreadChecker thread_checker_;
  base::PlatformFile platform_file_;
  size_t report_buffer_size_;
  bool has_report_id_;
  base::WeakPtr<HidConnectionLinux> connection_;
  scoped_refptr<base::SingleThreadTaskRunner> task_runner_;
  base::MessagePumpLibevent::FileDescriptorWatcher file_watcher_;

  DISALLOW_COPY_AND_ASSIGN(FileThreadHelper);
};

HidConnectionLinux::HidConnectionLinux(
    scoped_refptr<HidDeviceInfo> device_info,
    base::File device_file,
    scoped_refptr<base::SingleThreadTaskRunner> file_task_runner)
    : HidConnection(device_info),
      file_task_runner_(file_task_runner),
      weak_factory_(this) {
  task_runner_ = base::ThreadTaskRunnerHandle::Get();
  device_file_ = std::move(device_file);

  // The helper is passed a weak pointer to this connection so that it can be
  // cleaned up after the connection is closed.
  std::unique_ptr<FileThreadHelper> helper(
      new FileThreadHelper(device_file_.GetPlatformFile(), device_info,
                           weak_factory_.GetWeakPtr(), task_runner_));
  helper_ = helper.get();
  file_task_runner_->PostTask(
      FROM_HERE, base::Bind(&FileThreadHelper::Start, base::Passed(&helper)));
}

HidConnectionLinux::~HidConnectionLinux() {
  DCHECK(helper_ == nullptr);
}

void HidConnectionLinux::PlatformClose() {
  // By closing the device file on the FILE thread (1) the requirement that
  // base::File::Close is called on a thread where I/O is allowed is satisfied
  // and (2) any tasks posted to this task runner that refer to this file will
  // complete before it is closed.
  file_task_runner_->DeleteSoon(FROM_HERE, helper_);
  helper_ = nullptr;
  file_task_runner_->PostTask(FROM_HERE,
                              base::Bind(&HidConnectionLinux::CloseDevice,
                                         base::Passed(&device_file_)));

  while (!pending_reads_.empty()) {
    pending_reads_.front().callback.Run(false, NULL, 0);
    pending_reads_.pop();
  }
}

void HidConnectionLinux::PlatformRead(const ReadCallback& callback) {
  PendingHidRead pending_read;
  pending_read.callback = callback;
  pending_reads_.push(pending_read);
  ProcessReadQueue();
}

void HidConnectionLinux::PlatformWrite(scoped_refptr<net::IOBuffer> buffer,
                                       size_t size,
                                       const WriteCallback& callback) {
  // Linux expects the first byte of the buffer to always be a report ID so the
  // buffer can be used directly.
  file_task_runner_->PostTask(
      FROM_HERE,
      base::Bind(&HidConnectionLinux::BlockingWrite,
                 device_file_.GetPlatformFile(), buffer, size,
                 base::Bind(&HidConnectionLinux::FinishWrite,
                            weak_factory_.GetWeakPtr(), size, callback),
                 task_runner_));
}

void HidConnectionLinux::PlatformGetFeatureReport(
    uint8_t report_id,
    const ReadCallback& callback) {
  // The first byte of the destination buffer is the report ID being requested
  // and is overwritten by the feature report.
  DCHECK_GT(device_info()->max_feature_report_size(), 0u);
  scoped_refptr<net::IOBufferWithSize> buffer(
      new net::IOBufferWithSize(device_info()->max_feature_report_size() + 1));
  buffer->data()[0] = report_id;

  file_task_runner_->PostTask(
      FROM_HERE,
      base::Bind(
          &HidConnectionLinux::BlockingIoctl, device_file_.GetPlatformFile(),
          HIDIOCGFEATURE(buffer->size()), buffer,
          base::Bind(&HidConnectionLinux::FinishGetFeatureReport,
                     weak_factory_.GetWeakPtr(), report_id, buffer, callback),
          task_runner_));
}

void HidConnectionLinux::PlatformSendFeatureReport(
    scoped_refptr<net::IOBuffer> buffer,
    size_t size,
    const WriteCallback& callback) {
  // Linux expects the first byte of the buffer to always be a report ID so the
  // buffer can be used directly.
  file_task_runner_->PostTask(
      FROM_HERE,
      base::Bind(&HidConnectionLinux::BlockingIoctl,
                 device_file_.GetPlatformFile(), HIDIOCSFEATURE(size), buffer,
                 base::Bind(&HidConnectionLinux::FinishSendFeatureReport,
                            weak_factory_.GetWeakPtr(), callback),
                 task_runner_));
}

void HidConnectionLinux::FinishWrite(size_t expected_size,
                                     const WriteCallback& callback,
                                     ssize_t result) {
  if (result < 0) {
    HID_PLOG(EVENT) << "Write failed";
    callback.Run(false);
  } else {
    if (static_cast<size_t>(result) != expected_size) {
      HID_LOG(EVENT) << "Incomplete HID write: " << result
                     << " != " << expected_size;
    }
    callback.Run(true);
  }
}

void HidConnectionLinux::FinishGetFeatureReport(
    uint8_t report_id,
    scoped_refptr<net::IOBuffer> buffer,
    const ReadCallback& callback,
    int result) {
  if (result < 0) {
    HID_PLOG(EVENT) << "Failed to get feature report";
    callback.Run(false, NULL, 0);
  } else if (result == 0) {
    HID_LOG(EVENT) << "Get feature result too short.";
    callback.Run(false, NULL, 0);
  } else if (report_id == 0) {
    // Linux adds a 0 to the beginning of the data received from the device.
    scoped_refptr<net::IOBuffer> copied_buffer(new net::IOBuffer(result - 1));
    memcpy(copied_buffer->data(), buffer->data() + 1, result - 1);
    callback.Run(true, copied_buffer, result - 1);
  } else {
    callback.Run(true, buffer, result);
  }
}

void HidConnectionLinux::FinishSendFeatureReport(const WriteCallback& callback,
                                                 int result) {
  if (result < 0) {
    HID_PLOG(EVENT) << "Failed to send feature report";
    callback.Run(false);
  } else {
    callback.Run(true);
  }
}

// static
void HidConnectionLinux::BlockingWrite(
    base::PlatformFile platform_file,
    scoped_refptr<net::IOBuffer> buffer,
    size_t size,
    const InternalWriteCallback& callback,
    scoped_refptr<base::SingleThreadTaskRunner> task_runner) {
  base::ThreadRestrictions::AssertIOAllowed();
  ssize_t result = HANDLE_EINTR(write(platform_file, buffer->data(), size));
  task_runner->PostTask(FROM_HERE, base::Bind(callback, result));
}

// static
void HidConnectionLinux::BlockingIoctl(
    base::PlatformFile platform_file,
    int request,
    scoped_refptr<net::IOBuffer> buffer,
    const IoctlCallback& callback,
    scoped_refptr<base::SingleThreadTaskRunner> task_runner) {
  base::ThreadRestrictions::AssertIOAllowed();
  int result = ioctl(platform_file, request, buffer->data());
  task_runner->PostTask(FROM_HERE, base::Bind(callback, result));
}

// static
void HidConnectionLinux::CloseDevice(base::File device_file) {
  device_file.Close();
}

void HidConnectionLinux::ProcessInputReport(scoped_refptr<net::IOBuffer> buffer,
                                            size_t size) {
  DCHECK(thread_checker().CalledOnValidThread());
  PendingHidReport report;
  report.buffer = buffer;
  report.size = size;
  pending_reports_.push(report);
  ProcessReadQueue();
}

void HidConnectionLinux::ProcessReadQueue() {
  DCHECK(thread_checker().CalledOnValidThread());
  while (pending_reads_.size() && pending_reports_.size()) {
    PendingHidRead read = pending_reads_.front();
    PendingHidReport report = pending_reports_.front();

    pending_reports_.pop();
    if (CompleteRead(report.buffer, report.size, read.callback)) {
      pending_reads_.pop();
    }
  }
}

}  // namespace device
