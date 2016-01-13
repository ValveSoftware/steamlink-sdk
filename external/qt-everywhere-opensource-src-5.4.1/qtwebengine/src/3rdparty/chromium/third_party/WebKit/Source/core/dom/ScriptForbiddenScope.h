// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ScriptForbiddenScope_h
#define ScriptForbiddenScope_h

#include "wtf/Assertions.h"
#include "wtf/TemporaryChange.h"

namespace WebCore {

#if ASSERT_ENABLED

class ScriptForbiddenScope {
public:
    ScriptForbiddenScope();
    ~ScriptForbiddenScope();

    class AllowUserAgentScript {
    public:
        AllowUserAgentScript();
        ~AllowUserAgentScript();
    private:
        TemporaryChange<unsigned> m_change;
    };

    static bool isScriptForbidden();
};

#else

class ScriptForbiddenScope {
public:
    ScriptForbiddenScope() { }
    class AllowUserAgentScript {
    public:
        AllowUserAgentScript() { }
    };
    static bool isScriptForbidden() { return false; }
};

#endif

} // namespace WebCore

#endif // ScriptForbiddenScope_h
