// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef StorageManager_h
#define StorageManager_h

#include "bindings/core/v8/ScriptWrappable.h"
#include "platform/heap/Heap.h"

namespace blink {

class ScriptPromise;
class ScriptState;

class StorageManager final : public GarbageCollected<StorageManager>, public ScriptWrappable {
DEFINE_WRAPPERTYPEINFO();
public:
    ScriptPromise persisted(ScriptState*);
    ScriptPromise persist(ScriptState*);

    ScriptPromise estimate(ScriptState*);
    DECLARE_TRACE();
};

} // namespace blink

#endif // StorageManager_h
