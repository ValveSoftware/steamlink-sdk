// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/navigation_metrics/origins_seen_service.h"

namespace navigation_metrics {
namespace {
const size_t kDefaultMRUCacheSize = 1000;
}  // namespace

OriginsSeenService::OriginsSeenService()
    : origins_seen_(kDefaultMRUCacheSize) {}

OriginsSeenService::~OriginsSeenService() {}

bool OriginsSeenService::Insert(const url::Origin& origin) {
  bool seen = origins_seen_.Peek(origin) != origins_seen_.end();
  origins_seen_.Put(origin, true);
  return seen;
}
}  // namespace navigation_metrics
