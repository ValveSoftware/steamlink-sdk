// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "blimp/net/blimp_connection_statistics.h"

namespace blimp {

BlimpConnectionStatistics::BlimpConnectionStatistics()
    : histogram_(
          base::SparseHistogram::FactoryGet("BlimpSparseHistogram",
                                            base::HistogramBase::kNoFlags)) {}

BlimpConnectionStatistics::~BlimpConnectionStatistics() {}

void BlimpConnectionStatistics::Add(EventType type, int data) {
  histogram_->AddCount(type, data);
}

int BlimpConnectionStatistics::Get(EventType type) {
  std::unique_ptr<base::HistogramSamples> snapshot =
      histogram_->SnapshotSamples();
  return snapshot->GetCount(type);
}

}  // namespace blimp
