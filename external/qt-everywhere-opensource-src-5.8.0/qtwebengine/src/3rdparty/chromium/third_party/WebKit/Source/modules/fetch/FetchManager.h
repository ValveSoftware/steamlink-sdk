// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef FetchManager_h
#define FetchManager_h

#include "bindings/core/v8/ScriptPromise.h"
#include "core/dom/ContextLifecycleObserver.h"
#include "platform/heap/Handle.h"

namespace blink {

class ExecutionContext;
class FetchRequestData;
class ScriptState;

class FetchManager final : public GarbageCollected<FetchManager>, public ContextLifecycleObserver {
    USING_GARBAGE_COLLECTED_MIXIN(FetchManager);
public:
    static FetchManager* create(ExecutionContext*);
    explicit FetchManager(ExecutionContext*);
    ScriptPromise fetch(ScriptState*, FetchRequestData*);
    void contextDestroyed() override;
    bool isStopped() const { return m_isStopped; }

    DECLARE_TRACE();

private:
    class Loader;

    // Removes loader from |m_loaders|.
    void onLoaderFinished(Loader*);

    HeapHashSet<Member<Loader>> m_loaders;
    bool m_isStopped;
};

} // namespace blink

#endif // FetchManager_h
