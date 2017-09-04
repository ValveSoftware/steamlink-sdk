// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "blimp/net/blimp_stats.h"

#include "base/logging.h"

namespace blimp {

// static
base::LazyInstance<BlimpStats> BlimpStats::instance_ =
    LAZY_INSTANCE_INITIALIZER;

// static
BlimpStats* BlimpStats::GetInstance() {
  return &instance_.Get();
}

BlimpStats::BlimpStats() {
  for (size_t i = 0; i <= EVENT_TYPE_MAX; ++i) {
    base::subtle::NoBarrier_Store(&values_[i], 0);
  }
}

BlimpStats::~BlimpStats() {}

void BlimpStats::Add(EventType type, base::subtle::Atomic32 amount) {
  if (amount < 0) {
    DLOG(FATAL) << "Illegal negative stat value: type=" << type
                << ", amount=" << amount << ".";
    return;
  }

  base::subtle::NoBarrier_AtomicIncrement(&values_[type], amount);
}

base::subtle::Atomic32 BlimpStats::Get(EventType type) const {
  return base::subtle::NoBarrier_Load(&values_[type]);
}

}  // namespace blimp
