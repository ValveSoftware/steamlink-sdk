// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "core/testing/DeathAwareScriptWrappable.h"

namespace blink {

DeathAwareScriptWrappable* DeathAwareScriptWrappable::s_instance;
bool DeathAwareScriptWrappable::s_hasDied;

}  // namespace blink
