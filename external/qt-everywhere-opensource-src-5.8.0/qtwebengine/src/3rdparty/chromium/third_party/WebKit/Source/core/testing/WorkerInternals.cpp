// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "core/testing/WorkerInternals.h"

namespace blink {

// static
WorkerInternals* WorkerInternals::create(ScriptState* scriptState)
{
    return new WorkerInternals(scriptState);
}

WorkerInternals::~WorkerInternals()
{
}

WorkerInternals::WorkerInternals(ScriptState*)
{
}

} // namespace blink
