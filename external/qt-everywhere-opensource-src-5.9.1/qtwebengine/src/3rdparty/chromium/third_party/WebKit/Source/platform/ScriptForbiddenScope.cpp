// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "platform/ScriptForbiddenScope.h"

#include "wtf/Assertions.h"

namespace blink {

unsigned ScriptForbiddenScope::s_scriptForbiddenCount = 0;

}  // namespace blink
