// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef InternalsFrobulate_h
#define InternalsFrobulate_h

#include "wtf/Allocator.h"

namespace blink {

class ExceptionState;
class Internals;
class ScriptState;

class InternalsFrobulate final {
    STATIC_ONLY(InternalsFrobulate);
public:
    static bool frobulate(ScriptState*, Internals&, ExceptionState&);
    static bool frobulateNoEnabledCheck(Internals&);
};

} // namespace blink

#endif // InternalsFrobulate_h
