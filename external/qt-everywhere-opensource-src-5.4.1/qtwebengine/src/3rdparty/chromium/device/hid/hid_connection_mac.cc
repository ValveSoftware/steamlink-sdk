// Copyright (c) 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "device/hid/hid_connection_mac.h"

#include "base/bind.h"
#include "base/mac/foundation_util.h"
#include "base/message_loop/message_loop.h"
#include "base/threading/thread_restrictions.h"
#include "device/hid/hid_connection_mac.h"

namespace device {

HidConnectionMac::HidConnectionMac(HidDeviceInfo device_info)
    : HidConnection(device_info),
      device_(device_info.device_id, base::scoped_policy::RETAIN) {
  DCHECK(thread_checker_.CalledOnValidThread());

  message_loop_ = base::MessageLoopProxy::current();

  DCHECK(device_.get());
  inbound_buffer_.reset((uint8_t*)malloc(device_info.input_report_size));
  IOHIDDeviceRegisterInputReportCallback(device_.get(),
                                         inbound_buffer_.get(),
                                         device_info.input_report_size,
                                         &HidConnectionMac::InputReportCallback,
                                         this);
  IOHIDDeviceOpen(device_, kIOHIDOptionsTypeNone);
}

HidConnectionMac::~HidConnectionMac() {
  DCHECK(thread_checker_.CalledOnValidThread());

  while (!pending_reads_.empty()) {
    pending_reads_.front().callback.Run(false, 0);
    pending_reads_.pop();
  }

  IOHIDDeviceClose(device_, kIOHIDOptionsTypeNone);
}

void HidConnectionMac::Read(scoped_refptr<net::IOBufferWithSize> buffer,
                            const IOCallback& callback) {
  DCHECK(thread_checker_.CalledOnValidThread());
  if (!device_) {
    callback.Run(false, 0);
    return;
  }
  PendingHidRead read;
  read.buffer = buffer;
  read.callback = callback;
  pending_reads_.push(read);
  ProcessReadQueue();
}

void HidConnectionMac::Write(uint8_t report_id,
                             scoped_refptr<net::IOBufferWithSize> buffer,
                             const IOCallback& callback) {
  DCHECK(thread_checker_.CalledOnValidThread());
  WriteReport(kIOHIDReportTypeOutput, report_id, buffer, callback);
}

void HidConnectionMac::GetFeatureReport(
    uint8_t report_id,
    scoped_refptr<net::IOBufferWithSize> buffer,
    const IOCallback& callback) {
  DCHECK(thread_checker_.CalledOnValidThread());
  if (device_info().feature_report_size == 0) {
    callback.Run(false, 0);
    return;
  }

  if (buffer->size() < device_info().feature_report_size) {
    callback.Run(false, 0);
    return;
  }

  uint8_t* feature_report_buffer = reinterpret_cast<uint8_t*>(buffer->data());
  CFIndex feature_report_size = device_info().feature_report_size;
  IOReturn result = IOHIDDeviceGetReport(device_,
                                         kIOHIDReportTypeFeature,
                                         report_id,
                                         feature_report_buffer,
                                         &feature_report_size);
  if (result == kIOReturnSuccess)
    callback.Run(true, feature_report_size);
  else
    callback.Run(false, 0);
}

void HidConnectionMac::SendFeatureReport(
    uint8_t report_id,
    scoped_refptr<net::IOBufferWithSize> buffer,
    const IOCallback& callback) {
  DCHECK(thread_checker_.CalledOnValidThread());
  WriteReport(kIOHIDReportTypeFeature, report_id, buffer, callback);
}

void HidConnectionMac::InputReportCallback(void* context,
                                           IOReturn result,
                                           void* sender,
                                           IOHIDReportType type,
                                           uint32_t report_id,
                                           uint8_t* report_bytes,
                                           CFIndex report_length) {
  HidConnectionMac* connection = static_cast<HidConnectionMac*>(context);
  // report_id is already contained in report_bytes
  scoped_refptr<net::IOBufferWithSize> buffer;
  buffer = new net::IOBufferWithSize(report_length);
  memcpy(buffer->data(), report_bytes, report_length);

  connection->message_loop_->PostTask(
      FROM_HERE,
      base::Bind(
          &HidConnectionMac::ProcessInputReport, connection, type, buffer));
}

void HidConnectionMac::ProcessReadQueue() {
  DCHECK(thread_checker_.CalledOnValidThread());
  while (pending_reads_.size() && pending_reports_.size()) {
    PendingHidRead read = pending_reads_.front();
    pending_reads_.pop();
    PendingHidReport report = pending_reports_.front();
    if (read.buffer->size() < report.buffer->size()) {
      read.callback.Run(false, report.buffer->size());
    } else {
      memcpy(read.buffer->data(), report.buffer->data(), report.buffer->size());
      pending_reports_.pop();
      read.callback.Run(true, report.buffer->size());
    }
  }
}

void HidConnectionMac::ProcessInputReport(
    IOHIDReportType type,
    scoped_refptr<net::IOBufferWithSize> buffer) {
  DCHECK(thread_checker_.CalledOnValidThread());
  PendingHidReport report;
  report.buffer = buffer;
  pending_reports_.push(report);
  ProcessReadQueue();
}

void HidConnectionMac::WriteReport(IOHIDReportType type,
                                   uint8_t report_id,
                                   scoped_refptr<net::IOBufferWithSize> buffer,
                                   const IOCallback& callback) {
  DCHECK(thread_checker_.CalledOnValidThread());
  if (!device_) {
    callback.Run(false, 0);
    return;
  }
  scoped_refptr<net::IOBufferWithSize> output_buffer;
  if (report_id != 0) {
    output_buffer = new net::IOBufferWithSize(buffer->size() + 1);
    output_buffer->data()[0] = static_cast<uint8_t>(report_id);
    memcpy(output_buffer->data() + 1, buffer->data(), buffer->size());
  } else {
    output_buffer = new net::IOBufferWithSize(buffer->size());
    memcpy(output_buffer->data(), buffer->data(), buffer->size());
  }
  IOReturn res =
      IOHIDDeviceSetReport(device_.get(),
                           type,
                           report_id,
                           reinterpret_cast<uint8_t*>(output_buffer->data()),
                           output_buffer->size());
  if (res != kIOReturnSuccess) {
    callback.Run(false, 0);
  } else {
    callback.Run(true, output_buffer->size());
  }
}

}  // namespace device
