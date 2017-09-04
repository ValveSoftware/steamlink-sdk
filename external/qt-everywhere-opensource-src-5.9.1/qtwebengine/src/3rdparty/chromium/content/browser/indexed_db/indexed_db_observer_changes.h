// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_BROWSER_INDEXED_DB_INDEXED_DB_OBSERVER_CHANGES_H_
#define CONTENT_BROWSER_INDEXED_DB_INDEXED_DB_OBSERVER_CHANGES_H_

#include <stddef.h>
#include <stdint.h>

#include <map>
#include <memory>
#include <utility>
#include <vector>

#include "base/macros.h"
#include "content/browser/indexed_db/indexed_db_observation.h"
#include "content/common/content_export.h"

namespace content {

class IndexedDBObservation;

// Holds observations corresponding to a transaction for a single connection.
class CONTENT_EXPORT IndexedDBObserverChanges {
 public:
  IndexedDBObserverChanges();
  ~IndexedDBObserverChanges();

  void RecordObserverForLastObservation(int32_t observer_id);
  void AddObservation(std::unique_ptr<IndexedDBObservation> observation);

  std::map<int32_t, std::vector<int32_t>> release_observation_indices_map() {
    return std::move(observation_indices_map_);
  }
  std::vector<std::unique_ptr<IndexedDBObservation>> release_observations() {
    return std::move(observations_);
  }

 private:
  // Maps observer_id to corresponding set of indices in observations.
  std::map<int32_t, std::vector<int32_t>> observation_indices_map_;
  std::vector<std::unique_ptr<IndexedDBObservation>> observations_;

  DISALLOW_COPY_AND_ASSIGN(IndexedDBObserverChanges);
};

}  // namespace content

#endif  // CONTENT_BROWSER_INDEXED_DB_INDEXED_DB_OBSERVER_CHANGES_H_
