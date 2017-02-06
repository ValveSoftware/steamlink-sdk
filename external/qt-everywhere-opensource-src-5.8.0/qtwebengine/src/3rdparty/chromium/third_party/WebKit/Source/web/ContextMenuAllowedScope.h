// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ContextMenuAllowedScope_h
#define ContextMenuAllowedScope_h

#include "wtf/Allocator.h"

namespace blink {

class ContextMenuAllowedScope {
    STACK_ALLOCATED();

public:
    ContextMenuAllowedScope();
    ~ContextMenuAllowedScope();

    static bool isContextMenuAllowed();
};

} // namespace blink

#endif // ContextMenuAllowedScope_h
