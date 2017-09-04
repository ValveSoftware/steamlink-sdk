// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_DATA_REDUCTION_PROXY_CORE_BROWSER_DATA_USE_GROUP_H_
#define COMPONENTS_DATA_REDUCTION_PROXY_CORE_BROWSER_DATA_USE_GROUP_H_

#include <string>

#include "base/memory/ref_counted.h"

namespace data_reduction_proxy {

// Abstract class that tracks data usage for a group of requests, e.g. those
// corresponding to the same page load. Applications are responsible for
// providing overrides that track data usage for groups they care about.
// This class is ref counted because their lifetimes can depends on the lifetime
// of frames as well as lifetime of URL requests whose data usage is being
// tracked.
class DataUseGroup : public base::RefCountedThreadSafe<DataUseGroup> {
 public:
  // Initialize this instance.
  virtual void Initialize() = 0;

  // The hostname that this data usage should be ascribed to.
  virtual std::string GetHostname() = 0;

 protected:
  friend class base::RefCountedThreadSafe<DataUseGroup>;
  virtual ~DataUseGroup() {}
};

}  // namespace data_reduction_proxy

#endif  // COMPONENTS_DATA_REDUCTION_PROXY_CORE_BROWSER_DATA_USE_GROUP_H_
