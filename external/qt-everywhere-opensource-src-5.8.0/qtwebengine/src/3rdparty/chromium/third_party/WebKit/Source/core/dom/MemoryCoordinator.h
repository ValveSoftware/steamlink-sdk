// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef MemoryCoordinator_h
#define MemoryCoordinator_h

#include "core/CoreExport.h"
#include "public/platform/WebMemoryPressureLevel.h"
#include "wtf/Noncopyable.h"

namespace blink {

// MemoryCoordinator listens to some events which could be opportunities
// for reducing memory consumption and notifies its clients.
class CORE_EXPORT MemoryCoordinator final {
    WTF_MAKE_NONCOPYABLE(MemoryCoordinator);
public:
    static MemoryCoordinator& instance();

    void onMemoryPressure(WebMemoryPressureLevel);

private:
    MemoryCoordinator();
    ~MemoryCoordinator();
};

} // namespace blink

#endif // MemoryCoordinator_h
