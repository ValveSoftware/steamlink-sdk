// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef GlobalCacheStorage_h
#define GlobalCacheStorage_h

#include "wtf/Allocator.h"

namespace blink {

class CacheStorage;
class DOMWindow;
class ExceptionState;
class WorkerGlobalScope;

class GlobalCacheStorage {
    STATIC_ONLY(GlobalCacheStorage);
public:
    static CacheStorage* caches(DOMWindow&, ExceptionState&);
    static CacheStorage* caches(WorkerGlobalScope&, ExceptionState&);
};

} // namespace blink

#endif // GlobalCacheStorage_h
