// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "cc/blimp/remote_compositor_bridge.h"

#include <utility>

namespace cc {

RemoteCompositorBridge::RemoteCompositorBridge(
    scoped_refptr<base::SingleThreadTaskRunner> compositor_main_task_runner)
    : compositor_main_task_runner_(std::move(compositor_main_task_runner)) {}

RemoteCompositorBridge::~RemoteCompositorBridge() = default;

}  // namespace cc
