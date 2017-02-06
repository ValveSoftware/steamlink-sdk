// Copyright (c) 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "device/hid/hid_connection.h"

#include <algorithm>

#include "components/device_event_log/device_event_log.h"

namespace device {

namespace {

// Functor used to filter collections by report ID.
struct CollectionHasReportId {
  explicit CollectionHasReportId(uint8_t report_id) : report_id_(report_id) {}

  bool operator()(const HidCollectionInfo& info) const {
    if (info.report_ids.size() == 0 ||
        report_id_ == HidConnection::kNullReportId)
      return false;

    if (report_id_ == HidConnection::kAnyReportId)
      return true;

    return std::find(info.report_ids.begin(),
                     info.report_ids.end(),
                     report_id_) != info.report_ids.end();
  }

 private:
  const uint8_t report_id_;
};

// Functor returning true if collection has a protected usage.
struct CollectionIsProtected {
  bool operator()(const HidCollectionInfo& info) const {
    return info.usage.IsProtected();
  }
};

bool FindCollectionByReportId(const std::vector<HidCollectionInfo>& collections,
                              uint8_t report_id,
                              HidCollectionInfo* collection_info) {
  std::vector<HidCollectionInfo>::const_iterator collection_iter = std::find_if(
      collections.begin(), collections.end(), CollectionHasReportId(report_id));
  if (collection_iter != collections.end()) {
    if (collection_info) {
      *collection_info = *collection_iter;
    }
    return true;
  }

  return false;
}

bool HasProtectedCollection(const std::vector<HidCollectionInfo>& collections) {
  return std::find_if(collections.begin(), collections.end(),
                      CollectionIsProtected()) != collections.end();
}

}  // namespace

HidConnection::HidConnection(scoped_refptr<HidDeviceInfo> device_info)
    : device_info_(device_info), closed_(false) {
  has_protected_collection_ =
      HasProtectedCollection(device_info->collections());
}

HidConnection::~HidConnection() {
  DCHECK(thread_checker_.CalledOnValidThread());
  DCHECK(closed_);
}

void HidConnection::Close() {
  DCHECK(thread_checker_.CalledOnValidThread());
  DCHECK(!closed_);

  PlatformClose();
  closed_ = true;
}

void HidConnection::Read(const ReadCallback& callback) {
  DCHECK(thread_checker_.CalledOnValidThread());
  if (device_info_->max_input_report_size() == 0) {
    HID_LOG(USER) << "This device does not support input reports.";
    callback.Run(false, NULL, 0);
    return;
  }

  PlatformRead(callback);
}

void HidConnection::Write(scoped_refptr<net::IOBuffer> buffer,
                          size_t size,
                          const WriteCallback& callback) {
  DCHECK(thread_checker_.CalledOnValidThread());
  if (device_info_->max_output_report_size() == 0) {
    HID_LOG(USER) << "This device does not support output reports.";
    callback.Run(false);
    return;
  }
  if (size > device_info_->max_output_report_size() + 1) {
    HID_LOG(USER) << "Output report buffer too long (" << size << " > "
                  << (device_info_->max_output_report_size() + 1) << ").";
    callback.Run(false);
    return;
  }
  DCHECK_GE(size, 1u);
  uint8_t report_id = buffer->data()[0];
  if (device_info_->has_report_id() != (report_id != 0)) {
    HID_LOG(USER) << "Invalid output report ID.";
    callback.Run(false);
    return;
  }
  if (IsReportIdProtected(report_id)) {
    HID_LOG(USER) << "Attempt to set a protected output report.";
    callback.Run(false);
    return;
  }

  PlatformWrite(buffer, size, callback);
}

void HidConnection::GetFeatureReport(uint8_t report_id,
                                     const ReadCallback& callback) {
  DCHECK(thread_checker_.CalledOnValidThread());
  if (device_info_->max_feature_report_size() == 0) {
    HID_LOG(USER) << "This device does not support feature reports.";
    callback.Run(false, NULL, 0);
    return;
  }
  if (device_info_->has_report_id() != (report_id != 0)) {
    HID_LOG(USER) << "Invalid feature report ID.";
    callback.Run(false, NULL, 0);
    return;
  }
  if (IsReportIdProtected(report_id)) {
    HID_LOG(USER) << "Attempt to get a protected feature report.";
    callback.Run(false, NULL, 0);
    return;
  }

  PlatformGetFeatureReport(report_id, callback);
}

void HidConnection::SendFeatureReport(scoped_refptr<net::IOBuffer> buffer,
                                      size_t size,
                                      const WriteCallback& callback) {
  DCHECK(thread_checker_.CalledOnValidThread());
  if (device_info_->max_feature_report_size() == 0) {
    HID_LOG(USER) << "This device does not support feature reports.";
    callback.Run(false);
    return;
  }
  DCHECK_GE(size, 1u);
  uint8_t report_id = buffer->data()[0];
  if (device_info_->has_report_id() != (report_id != 0)) {
    HID_LOG(USER) << "Invalid feature report ID.";
    callback.Run(false);
    return;
  }
  if (IsReportIdProtected(report_id)) {
    HID_LOG(USER) << "Attempt to set a protected feature report.";
    callback.Run(false);
    return;
  }

  PlatformSendFeatureReport(buffer, size, callback);
}

bool HidConnection::CompleteRead(scoped_refptr<net::IOBuffer> buffer,
                                 size_t size,
                                 const ReadCallback& callback) {
  DCHECK_GE(size, 1u);
  uint8_t report_id = buffer->data()[0];
  if (IsReportIdProtected(report_id)) {
    HID_LOG(EVENT) << "Filtered a protected input report.";
    return false;
  }

  callback.Run(true, buffer, size);
  return true;
}

bool HidConnection::IsReportIdProtected(uint8_t report_id) {
  HidCollectionInfo collection_info;
  if (FindCollectionByReportId(device_info_->collections(), report_id,
                               &collection_info)) {
    return collection_info.usage.IsProtected();
  }

  return has_protected_collection();
}

PendingHidReport::PendingHidReport() {}

PendingHidReport::PendingHidReport(const PendingHidReport& other) = default;

PendingHidReport::~PendingHidReport() {}

PendingHidRead::PendingHidRead() {}

PendingHidRead::PendingHidRead(const PendingHidRead& other) = default;

PendingHidRead::~PendingHidRead() {}

}  // namespace device
