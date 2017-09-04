// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "extensions/common/event_filtering_info.h"

#include <utility>

#include "base/json/json_writer.h"
#include "base/memory/ptr_util.h"
#include "base/values.h"

namespace extensions {

EventFilteringInfo::EventFilteringInfo()
    : has_url_(false),
      has_instance_id_(false),
      instance_id_(0),
      has_window_type_(false),
      has_window_exposed_by_default_(false) {}

EventFilteringInfo::EventFilteringInfo(const EventFilteringInfo& other) =
    default;

EventFilteringInfo::~EventFilteringInfo() {
}

void EventFilteringInfo::SetWindowType(const std::string& window_type) {
  window_type_ = window_type;
  has_window_type_ = true;
}

void EventFilteringInfo::SetWindowExposedByDefault(const bool exposed) {
  window_exposed_by_default_ = exposed;
  has_window_exposed_by_default_ = true;
}

void EventFilteringInfo::SetURL(const GURL& url) {
  url_ = url;
  has_url_ = true;
}

void EventFilteringInfo::SetInstanceID(int instance_id) {
  instance_id_ = instance_id;
  has_instance_id_ = true;
}

std::unique_ptr<base::DictionaryValue> EventFilteringInfo::AsValue() const {
  auto result = base::MakeUnique<base::DictionaryValue>();
  if (has_url_)
    result->SetString("url", url_.spec());

  if (has_instance_id_)
    result->SetInteger("instanceId", instance_id_);

  if (!service_type_.empty())
    result->SetString("serviceType", service_type_);

  if (has_window_type_)
    result->SetString("windowType", window_type_);

  if (has_window_exposed_by_default_)
    result->SetBoolean("windowExposedByDefault", window_exposed_by_default_);

  return result;
}

}  // namespace extensions
