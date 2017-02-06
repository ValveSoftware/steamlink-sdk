// Copyright (c) 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "public/web/WebMemoryCoordinator.h"

#include "core/dom/MemoryCoordinator.h"
#include "core/page/Page.h"

namespace blink {

void WebMemoryCoordinator::onMemoryPressure(WebMemoryPressureLevel pressureLevel)
{
    MemoryCoordinator::instance().onMemoryPressure(pressureLevel);
}

} // namespace blink
