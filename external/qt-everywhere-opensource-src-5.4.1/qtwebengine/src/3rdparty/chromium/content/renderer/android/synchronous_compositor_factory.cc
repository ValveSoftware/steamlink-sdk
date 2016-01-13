// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/renderer/android/synchronous_compositor_factory.h"

#include "base/command_line.h"
#include "base/logging.h"
#include "content/public/common/content_switches.h"

namespace content {

namespace {
SynchronousCompositorFactory* g_instance = NULL;
}  // namespace

// static
void SynchronousCompositorFactory::SetInstance(
    SynchronousCompositorFactory* instance) {
  DCHECK(g_instance == NULL);

  // This feature only makes sense in single process mode.
  CHECK(CommandLine::ForCurrentProcess()->HasSwitch(switches::kSingleProcess));

  g_instance = instance;
}

// static
SynchronousCompositorFactory* SynchronousCompositorFactory::GetInstance() {
  return g_instance;
}

}  // namespace content
