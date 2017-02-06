// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_DATA_REDUCTION_PROXY_CORE_BROWSER_DATA_USE_GROUP_PROVIDER_H_
#define COMPONENTS_DATA_REDUCTION_PROXY_CORE_BROWSER_DATA_USE_GROUP_PROVIDER_H_

#include "base/memory/ref_counted.h"

namespace net {
class URLRequest;
}

namespace data_reduction_proxy {

class DataUseGroup;

// Abstract class that manages instances of |DataUsageGroup| and maps
// |URLRequest| instances to their appropriate |DataUsageGroup|. Applications
// should provide overrides if they are interested in tracking data usage
// and surfacing it to the end user.
class DataUseGroupProvider {
 public:
  // Returns the |DataUseGroup| to which data usage for the given URL should
  // be ascribed. If no existing |DataUseGroup| exists, a new one will be
  // created.
  virtual scoped_refptr<DataUseGroup> GetDataUseGroup(
      const net::URLRequest* request) = 0;
};

}  // namespace data_reduction_proxy

#endif  // COMPONENTS_DATA_REDUCTION_PROXY_CORE_BROWSER_DATA_USE_GROUP_PROVIDER_H_
