// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "blimp/common/compositor/blimp_task_graph_runner.h"

namespace blimp {

BlimpTaskGraphRunner::BlimpTaskGraphRunner() {
  Start("BlimpCompositorWorker",
        base::SimpleThread::Options(base::ThreadPriority::BACKGROUND));
}

BlimpTaskGraphRunner::~BlimpTaskGraphRunner() {
  Shutdown();
}

}  // namespace blimp
