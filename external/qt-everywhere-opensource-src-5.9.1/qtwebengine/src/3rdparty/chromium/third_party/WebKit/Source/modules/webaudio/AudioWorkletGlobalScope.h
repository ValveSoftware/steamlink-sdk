// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef AudioWorkletGlobalScope_h
#define AudioWorkletGlobalScope_h

#include "core/workers/ThreadedWorkletGlobalScope.h"

namespace blink {

class AudioWorkletGlobalScope final : public ThreadedWorkletGlobalScope {
  DEFINE_WRAPPERTYPEINFO();

 public:
  static AudioWorkletGlobalScope* create(const KURL&,
                                         const String& userAgent,
                                         PassRefPtr<SecurityOrigin>,
                                         v8::Isolate*,
                                         WorkerThread*);
  ~AudioWorkletGlobalScope() override;

 private:
  AudioWorkletGlobalScope(const KURL&,
                          const String& userAgent,
                          PassRefPtr<SecurityOrigin>,
                          v8::Isolate*,
                          WorkerThread*);
};

}  // namespace blink

#endif  // AudioWorkletGlobalScope_h
