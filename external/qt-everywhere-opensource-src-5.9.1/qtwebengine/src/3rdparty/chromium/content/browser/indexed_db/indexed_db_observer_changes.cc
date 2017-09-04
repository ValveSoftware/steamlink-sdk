// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/browser/indexed_db/indexed_db_observer_changes.h"

#include "content/browser/indexed_db/indexed_db_observation.h"

namespace content {

IndexedDBObserverChanges::IndexedDBObserverChanges() {}

IndexedDBObserverChanges::~IndexedDBObserverChanges() {}

void IndexedDBObserverChanges::RecordObserverForLastObservation(
    int32_t observer_id) {
  DCHECK(!observations_.empty());
  observation_indices_map_[observer_id].push_back(observations_.size() - 1);
}

void IndexedDBObserverChanges::AddObservation(
    std::unique_ptr<IndexedDBObservation> observation) {
  observations_.push_back(std::move(observation));
}

}  // namespace content
