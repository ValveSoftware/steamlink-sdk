// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/mus/common/switches.h"

namespace mus {
namespace switches {

// Use mojo GPU command buffer instead of Chrome GPU command buffer.
const char kUseMojoGpuCommandBufferInMus[] =
    "use-mojo-gpu-command-buffer-in-mus";

// Initializes X11 in threaded mode, and sets the |override_redirect| flag when
// creating X11 windows. Also, exposes the WindowServerTest interface to clients
// when launched with this flag.
const char kUseTestConfig[] = "use-test-config";

}  // namespace switches
}  // namespace mus
