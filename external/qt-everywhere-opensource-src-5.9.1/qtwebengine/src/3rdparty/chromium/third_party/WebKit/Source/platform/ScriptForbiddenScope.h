// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ScriptForbiddenScope_h
#define ScriptForbiddenScope_h

#include "platform/PlatformExport.h"
#include "wtf/Allocator.h"
#include "wtf/AutoReset.h"
#include "wtf/Optional.h"

namespace blink {

// Scoped disabling of script execution on the main thread,
// and only to be used by the main thread.
class PLATFORM_EXPORT ScriptForbiddenScope final {
  STACK_ALLOCATED();
  WTF_MAKE_NONCOPYABLE(ScriptForbiddenScope);

 public:
  ScriptForbiddenScope() { enter(); }
  ~ScriptForbiddenScope() { exit(); }

  class PLATFORM_EXPORT AllowUserAgentScript final {
    STACK_ALLOCATED();
    WTF_MAKE_NONCOPYABLE(AllowUserAgentScript);

   public:
    AllowUserAgentScript() {
      if (isMainThread())
        m_change.emplace(&s_scriptForbiddenCount, 0);
    }
    ~AllowUserAgentScript() {
      DCHECK(!isMainThread() || !s_scriptForbiddenCount);
    }

   private:
    Optional<AutoReset<unsigned>> m_change;
  };

  static void enter() {
    DCHECK(isMainThread());
    ++s_scriptForbiddenCount;
  }
  static void exit() {
    DCHECK(s_scriptForbiddenCount);
    --s_scriptForbiddenCount;
  }
  static bool isScriptForbidden() {
    return isMainThread() && s_scriptForbiddenCount;
  }

 private:
  static unsigned s_scriptForbiddenCount;
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
  ScriptForbiddenIfMainThreadScope() {
    m_IsMainThread = isMainThread();
    if (m_IsMainThread)
      ScriptForbiddenScope::enter();
  }
  ~ScriptForbiddenIfMainThreadScope() {
    if (m_IsMainThread)
      ScriptForbiddenScope::exit();
  }
  bool m_IsMainThread;
};

}  // namespace blink

#endif  // ScriptForbiddenScope_h
