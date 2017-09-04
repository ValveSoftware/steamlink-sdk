// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef AnimationWorkletGlobalScope_h
#define AnimationWorkletGlobalScope_h

#include "core/workers/ThreadedWorkletGlobalScope.h"

namespace blink {

class AnimationWorkletGlobalScope : public ThreadedWorkletGlobalScope {
  DEFINE_WRAPPERTYPEINFO();

 public:
  static AnimationWorkletGlobalScope* create(const KURL&,
                                             const String& userAgent,
                                             PassRefPtr<SecurityOrigin>,
                                             v8::Isolate*,
                                             WorkerThread*);
  ~AnimationWorkletGlobalScope() override;

 private:
  AnimationWorkletGlobalScope(const KURL&,
                              const String& userAgent,
                              PassRefPtr<SecurityOrigin>,
                              v8::Isolate*,
                              WorkerThread*);
};

}  // namespace blink

#endif  // AnimationWorkletGlobalScope_h
