// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef GlobalIndexedDB_h
#define GlobalIndexedDB_h

#include "wtf/Allocator.h"

namespace blink {

class IDBFactory;
class DOMWindow;
class WorkerGlobalScope;

class GlobalIndexedDB {
    STATIC_ONLY(GlobalIndexedDB);
public:
    static IDBFactory* indexedDB(DOMWindow&);
    static IDBFactory* indexedDB(WorkerGlobalScope&);
};

} // namespace blink

#endif // GlobalIndexedDB_h
