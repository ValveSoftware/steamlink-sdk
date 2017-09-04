// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_NAVIGATION_METRICS_ORIGINS_SEEN_SERVICE_H_
#define COMPONENTS_NAVIGATION_METRICS_ORIGINS_SEEN_SERVICE_H_

#include "base/containers/mru_cache.h"
#include "components/keyed_service/core/keyed_service.h"
#include "url/origin.h"

namespace navigation_metrics {

class OriginsSeenService : public KeyedService {
 public:
  OriginsSeenService();
  ~OriginsSeenService() override;

  // Used when deciding whether or not to record
  // Navigation.SchemePerUniqueOrigin[OTR]. Inserts a copy of |origin| into the
  // set |origins_seen_|, and returns whether or not |origin| was already in the
  // set.
  bool Insert(const url::Origin& origin);

 private:
  // Used by |HaveAlreadySeenOrigin|. This cache is in volatile storage because
  // Off The Record Profiles also use this Service.
  base::MRUCache<url::Origin, bool> origins_seen_;
};

}  // namespace navigation_metrics

#endif  // COMPONENTS_NAVIGATION_METRICS_ORIGINS_SEEN_SERVICE_H_
