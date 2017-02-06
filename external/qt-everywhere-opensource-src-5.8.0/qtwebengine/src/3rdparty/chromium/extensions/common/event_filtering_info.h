// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef EXTENSIONS_COMMON_EVENT_FILTERING_INFO_H_
#define EXTENSIONS_COMMON_EVENT_FILTERING_INFO_H_

#include <memory>

#include "url/gurl.h"

namespace base {
class Value;
}

namespace extensions {

// Extra information about an event that is used in event filtering.
//
// This is the information that is matched against criteria specified in JS
// extension event listeners. Eg:
//
// chrome.someApi.onSomeEvent.addListener(cb,
//                                        {url: [{hostSuffix: 'google.com'}],
//                                         tabId: 1});
class EventFilteringInfo {
 public:
  EventFilteringInfo();
  EventFilteringInfo(const EventFilteringInfo& other);
  ~EventFilteringInfo();
  void SetWindowExposedByDefault(bool exposed);
  void SetWindowType(const std::string& window_type);
  void SetURL(const GURL& url);
  void SetInstanceID(int instance_id);
  void SetServiceType(const std::string& service_type) {
    service_type_ = service_type;
  }

  // Note: window type & visible are Chrome concepts, so arguably
  // doesn't belong in the extensions module. If the number of Chrome
  // concept grows, consider a delegation model with a
  // ChromeEventFilteringInfo class.
  bool has_window_type() const { return has_window_type_; }
  const std::string& window_type() const { return window_type_; }

  // By default events related to windows are filtered based on the
  // listener's extension. This parameter will be set if the listener
  // didn't set any filter on window types.
  bool has_window_exposed_by_default() const {
    return has_window_exposed_by_default_;
  }
  bool window_exposed_by_default() const { return window_exposed_by_default_; }

  bool has_url() const { return has_url_; }
  const GURL& url() const { return url_; }

  bool has_instance_id() const { return has_instance_id_; }
  int instance_id() const { return instance_id_; }

  bool has_service_type() const { return !service_type_.empty(); }
  const std::string& service_type() const { return service_type_; }

  std::unique_ptr<base::Value> AsValue() const;
  bool IsEmpty() const;

 private:
  bool has_url_;
  GURL url_;
  std::string service_type_;

  bool has_instance_id_;
  int instance_id_;

  bool has_window_type_;
  std::string window_type_;

  bool has_window_exposed_by_default_;
  bool window_exposed_by_default_;

  // Allow implicit copy and assignment.
};

}  // namespace extensions

#endif  // EXTENSIONS_COMMON_EVENT_FILTERING_INFO_H_
