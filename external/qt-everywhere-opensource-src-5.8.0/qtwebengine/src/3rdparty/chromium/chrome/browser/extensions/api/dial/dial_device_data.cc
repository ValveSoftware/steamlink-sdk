// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/extensions/api/dial/dial_device_data.h"

#include "chrome/common/extensions/api/dial.h"

namespace extensions {

DialDeviceData::DialDeviceData() : max_age_(-1), config_id_(-1) { }

DialDeviceData::DialDeviceData(const std::string& device_id,
                               const GURL& device_description_url,
                               const base::Time& response_time)
    : device_id_(device_id), device_description_url_(device_description_url),
      response_time_(response_time), max_age_(-1), config_id_(-1) {
}

DialDeviceData::DialDeviceData(const DialDeviceData& other) = default;

DialDeviceData::~DialDeviceData() { }

const GURL& DialDeviceData::device_description_url() const {
  return device_description_url_;
}

void DialDeviceData::set_device_description_url(const GURL& url) {
  device_description_url_ = url;
}

// static
bool DialDeviceData::IsDeviceDescriptionUrl(const GURL& url) {
  return url.is_valid() && !url.is_empty() && url.SchemeIsHTTPOrHTTPS();
}

bool DialDeviceData::UpdateFrom(const DialDeviceData& new_data) {
  DCHECK(new_data.device_id() == device_id_);
  DCHECK(new_data.label().empty());
  std::string label_tmp(label_);
  bool updated_api_visible_field =
      (new_data.device_description_url() != device_description_url_) ||
      (new_data.config_id() != config_id_);
  *this = new_data;
  label_ = label_tmp;
  return updated_api_visible_field;
}

void DialDeviceData::FillDialDevice(api::dial::DialDevice* device) const {
  DCHECK(!device_id_.empty());
  DCHECK(IsDeviceDescriptionUrl(device_description_url_));
  device->device_label = label_;
  device->device_description_url = device_description_url_.spec();
  if (has_config_id())
    device->config_id.reset(new int(config_id_));
}

}  // namespace extensions
