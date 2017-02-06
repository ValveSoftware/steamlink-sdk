// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "web/ContextMenuAllowedScope.h"

namespace blink {

static unsigned s_ContextMenuAllowedCount = 0;

ContextMenuAllowedScope::ContextMenuAllowedScope()
{
    s_ContextMenuAllowedCount++;
}

ContextMenuAllowedScope::~ContextMenuAllowedScope()
{
    s_ContextMenuAllowedCount--;
}

bool ContextMenuAllowedScope::isContextMenuAllowed()
{
    return s_ContextMenuAllowedCount;
}

} // namespace blink
