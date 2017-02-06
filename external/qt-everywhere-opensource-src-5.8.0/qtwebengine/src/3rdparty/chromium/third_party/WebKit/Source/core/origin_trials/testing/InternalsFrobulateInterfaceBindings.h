// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef InternalsFrobulateInterfaceBindings_h
#define InternalsFrobulateInterfaceBindings_h

#include "wtf/Allocator.h"

namespace blink {

class Internals;

class InternalsFrobulateInterfaceBindings final {
    STATIC_ONLY(InternalsFrobulateInterfaceBindings);
public:
    static bool frobulatePartial(Internals&) { return true; }
    static bool frobulateMethodPartial(Internals&) { return true; }
};

} // namespace blink

#endif // InternalsFrobulateInterfaceBindings_h
