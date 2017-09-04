// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef WorkerInternals_h
#define WorkerInternals_h

#include "bindings/core/v8/ScriptWrappable.h"
#include "platform/heap/GarbageCollected.h"

namespace blink {

class OriginTrialsTest;

class WorkerInternals final : public GarbageCollectedFinalized<WorkerInternals>,
                              public ScriptWrappable {
  DEFINE_WRAPPERTYPEINFO();

 public:
  static WorkerInternals* create() { return new WorkerInternals(); }
  virtual ~WorkerInternals();
  OriginTrialsTest* originTrialsTest() const;

  DEFINE_INLINE_TRACE() {}

 private:
  explicit WorkerInternals();
};

}  // namespace blink

#endif  // WorkerInternals_h
