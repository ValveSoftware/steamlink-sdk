// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef WorkerInternalsFrobulate_h
#define WorkerInternalsFrobulate_h

#include "wtf/Allocator.h"

namespace blink {

class WorkerInternals;

class WorkerInternalsFrobulate final {
    STATIC_ONLY(WorkerInternalsFrobulate);
public:
    static const unsigned short kFrobulateConst = 1;
    static bool frobulate(WorkerInternals&) { return true; }
};

} // namespace blink

#endif // WorkerInternalsFrobulate_h
