// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "config.h"
#include "core/dom/ScriptForbiddenScope.h"

#include "wtf/Assertions.h"
#include "wtf/MainThread.h"

namespace WebCore {

#if ASSERT_ENABLED

static unsigned s_scriptForbiddenCount = 0;

ScriptForbiddenScope::ScriptForbiddenScope()
{
    ASSERT(isMainThread());
    ++s_scriptForbiddenCount;
}

ScriptForbiddenScope::~ScriptForbiddenScope()
{
    ASSERT(isMainThread());
    --s_scriptForbiddenCount;
}

bool ScriptForbiddenScope::isScriptForbidden()
{
    return isMainThread() && s_scriptForbiddenCount;
}

ScriptForbiddenScope::AllowUserAgentScript::AllowUserAgentScript()
    : m_change(s_scriptForbiddenCount, 0)
{
}

ScriptForbiddenScope::AllowUserAgentScript::~AllowUserAgentScript()
{
    ASSERT(!s_scriptForbiddenCount);
}

#endif

} // namespace WebCore
