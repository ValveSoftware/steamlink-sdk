// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef SyncError_h
#define SyncError_h

#include "core/dom/DOMException.h"
#include "platform/heap/Handle.h"
#include "public/platform/modules/background_sync/WebSyncError.h"
#include "wtf/Allocator.h"

namespace blink {

class ScriptPromiseResolver;

class SyncError {
    STATIC_ONLY(SyncError);
public:
    static DOMException* take(ScriptPromiseResolver*, const WebSyncError&);
};

} // namespace blink

#endif // SyncError_h
