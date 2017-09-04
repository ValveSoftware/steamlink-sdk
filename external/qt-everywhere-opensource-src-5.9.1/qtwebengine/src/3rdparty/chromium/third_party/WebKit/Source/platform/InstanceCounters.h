/*
* Copyright (C) 2012 Google Inc. All rights reserved.
*
* Redistribution and use in source and binary forms, with or without
* modification, are permitted provided that the following conditions are
* met:
*
*     * Redistributions of source code must retain the above copyright
* notice, this list of conditions and the following disclaimer.
*     * Redistributions in binary form must reproduce the above
* copyright notice, this list of conditions and the following disclaimer
* in the documentation and/or other materials provided with the
* distribution.
*     * Neither the name of Google Inc. nor the names of its
* contributors may be used to endorse or promote products derived from
* this software without specific prior written permission.
*
* THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS
* "AS IS" AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT
* LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR
* A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT
* OWNER OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL,
* SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT
* LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE,
* DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY
* THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
* (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
* OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
*/

#ifndef InstanceCounters_h
#define InstanceCounters_h

#include "platform/PlatformExport.h"
#include "wtf/Allocator.h"
#include "wtf/Atomics.h"

namespace blink {

class InstanceCounters {
  STATIC_ONLY(InstanceCounters);

 public:
  enum CounterType {
    ActiveDOMObjectCounter,
    AudioHandlerCounter,
    DocumentCounter,
    FrameCounter,
    JSEventListenerCounter,
    LayoutObjectCounter,
    NodeCounter,
    ResourceCounter,
    ScriptPromiseCounter,
    V8PerContextDataCounter,
    WorkerGlobalScopeCounter,

    // This value must be the last.
    CounterTypeLength,
  };

  static inline void incrementCounter(CounterType type) {
    DCHECK_NE(type, NodeCounter);
    atomicIncrement(&s_counters[type]);
  }

  static inline void decrementCounter(CounterType type) {
    DCHECK_NE(type, NodeCounter);
    atomicDecrement(&s_counters[type]);
  }

  static inline void incrementNodeCounter() {
    DCHECK(isMainThread());
    s_nodeCounter++;
  }

  static inline void decrementNodeCounter() {
    DCHECK(isMainThread());
    s_nodeCounter--;
  }

  PLATFORM_EXPORT static int counterValue(CounterType);

 private:
  PLATFORM_EXPORT static int s_counters[CounterTypeLength];
  PLATFORM_EXPORT static int s_nodeCounter;
};

}  // namespace blink

#endif  // !defined(InstanceCounters_h)
