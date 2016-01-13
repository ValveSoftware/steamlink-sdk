// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CC_RESOURCES_RETURNED_RESOURCE_H_
#define CC_RESOURCES_RETURNED_RESOURCE_H_

#include <vector>

#include "base/basictypes.h"
#include "cc/base/cc_export.h"

namespace cc {

struct CC_EXPORT ReturnedResource {
  ReturnedResource() : id(0), sync_point(0), count(0), lost(false) {}
  unsigned id;
  unsigned sync_point;
  int count;
  bool lost;
};

typedef std::vector<ReturnedResource> ReturnedResourceArray;

}  // namespace cc

#endif  // CC_RESOURCES_RETURNED_RESOURCE_H_
