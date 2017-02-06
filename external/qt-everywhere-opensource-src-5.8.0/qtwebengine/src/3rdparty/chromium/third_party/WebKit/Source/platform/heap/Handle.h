/*
 * Copyright (C) 2014 Google Inc. All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions are
 * met:
 *
 *     * Redistributions of source code must retain the above copyright
 * notice, this list of conditions and the following disclaimer.
 *     * Redistributions in binary form must reproduce the above
 * copyright notice, this list of conditions and the following disclaimer
 * in the documentation and/or other materials provided with the
 * distribution.
 *     * Neither the name of Google Inc. nor the names of its
 * contributors may be used to endorse or promote products derived from
 * this software without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS
 * "AS IS" AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT
 * LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR
 * A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT
 * OWNER OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL,
 * SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT
 * LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE,
 * DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY
 * THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
 * OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

#ifndef Handle_h
#define Handle_h

#include "platform/heap/Heap.h"
#include "platform/heap/HeapAllocator.h"
#include "platform/heap/InlinedGlobalMarkingVisitor.h"
#include "platform/heap/Member.h"
#include "platform/heap/Persistent.h"
#include "platform/heap/ThreadState.h"
#include "platform/heap/TraceTraits.h"
#include "platform/heap/Visitor.h"
#include "wtf/Allocator.h"

#if defined(LEAK_SANITIZER)
#include "wtf/LeakAnnotations.h"
#endif

namespace blink {

// LEAK_SANITIZER_DISABLED_SCOPE: all allocations made in the current scope
// will be exempted from LSan consideration.
//
// TODO(sof): move this to wtf/LeakAnnotations.h (LeakSanitizer.h?) once
// wtf/ can freely call upon Oilpan functionality.
#if defined(LEAK_SANITIZER)
class LeakSanitizerDisableScope {
    STACK_ALLOCATED();
    WTF_MAKE_NONCOPYABLE(LeakSanitizerDisableScope);
public:
    LeakSanitizerDisableScope()
    {
        __lsan_disable();
        if (ThreadState::current())
            ThreadState::current()->enterStaticReferenceRegistrationDisabledScope();
    }

    ~LeakSanitizerDisableScope()
    {
        __lsan_enable();
        if (ThreadState::current())
            ThreadState::current()->leaveStaticReferenceRegistrationDisabledScope();
    }
};
#define LEAK_SANITIZER_DISABLED_SCOPE LeakSanitizerDisableScope lsanDisabledScope
#else
#define LEAK_SANITIZER_DISABLED_SCOPE
#endif

} // namespace blink

#endif
