// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ScriptForbiddenScope_h
#define ScriptForbiddenScope_h

#include "platform/PlatformExport.h"
#include "wtf/Allocator.h"
#include "wtf/Optional.h"
#include "wtf/TemporaryChange.h"

namespace blink {

// Scoped disabling of script execution on the main thread,
// and only to be used by the main thread.
class PLATFORM_EXPORT ScriptForbiddenScope final {
    STACK_ALLOCATED();
    WTF_MAKE_NONCOPYABLE(ScriptForbiddenScope);
public:
    ScriptForbiddenScope();
    ~ScriptForbiddenScope();

    class PLATFORM_EXPORT AllowUserAgentScript final {
        STACK_ALLOCATED();
        WTF_MAKE_NONCOPYABLE(AllowUserAgentScript);
    public:
        AllowUserAgentScript();
        ~AllowUserAgentScript();
    private:
        Optional<TemporaryChange<unsigned>> m_change;
    };

    static void enter();
    static void exit();
    static bool isScriptForbidden();
};

// Scoped disabling of script execution on the main thread,
// if called on the main thread.
//
// No effect when used by from other threads -- simplifies
// call sites that might be used by multiple threads to have
// this scope object perform the is-main-thread check on
// its behalf.
class PLATFORM_EXPORT ScriptForbiddenIfMainThreadScope final {
    STACK_ALLOCATED();
    WTF_MAKE_NONCOPYABLE(ScriptForbiddenIfMainThreadScope);
public:
    ScriptForbiddenIfMainThreadScope();
    ~ScriptForbiddenIfMainThreadScope();
};

} // namespace blink

#endif // ScriptForbiddenScope_h
