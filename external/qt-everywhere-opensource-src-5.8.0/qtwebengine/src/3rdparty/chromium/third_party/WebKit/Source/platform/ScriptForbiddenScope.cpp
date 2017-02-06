// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "platform/ScriptForbiddenScope.h"

#include "wtf/Assertions.h"

namespace blink {

static unsigned s_scriptForbiddenCount = 0;

ScriptForbiddenScope::ScriptForbiddenScope()
{
    enter();
}

ScriptForbiddenScope::~ScriptForbiddenScope()
{
    exit();
}

void ScriptForbiddenScope::enter()
{
    ASSERT(isMainThread());
    ++s_scriptForbiddenCount;
}

void ScriptForbiddenScope::exit()
{
    ASSERT(isMainThread());
    ASSERT(s_scriptForbiddenCount);
    --s_scriptForbiddenCount;
}

bool ScriptForbiddenScope::isScriptForbidden()
{
    return isMainThread() && s_scriptForbiddenCount;
}

ScriptForbiddenScope::AllowUserAgentScript::AllowUserAgentScript()
{
    if (isMainThread())
        m_change.emplace(s_scriptForbiddenCount, 0);
}

ScriptForbiddenScope::AllowUserAgentScript::~AllowUserAgentScript()
{
    ASSERT(!isMainThread() || !s_scriptForbiddenCount);
}

ScriptForbiddenIfMainThreadScope::ScriptForbiddenIfMainThreadScope()
{
    if (isMainThread())
        ScriptForbiddenScope::enter();
}

ScriptForbiddenIfMainThreadScope::~ScriptForbiddenIfMainThreadScope()
{
    if (isMainThread())
        ScriptForbiddenScope::exit();
}
} // namespace blink
