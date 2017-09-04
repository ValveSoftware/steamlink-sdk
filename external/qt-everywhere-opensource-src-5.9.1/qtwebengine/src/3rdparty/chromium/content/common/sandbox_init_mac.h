// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_COMMON_SANDBOX_INIT_MAC_H_
#define CONTENT_COMMON_SANDBOX_INIT_MAC_H_

namespace content {

// Initialize the sandbox for renderer, gpu, utility, worker, and plugin
// processes, depending on the command line flags. For the browser process which
// is not sandboxed, this call is a no-op.
// Returns true if the sandbox was initialized succesfully, false if an error
// occurred.  If process_type isn't one that needs sandboxing, true is always
// returned.
bool InitializeSandbox();

}  // namespace content

#endif  // CONTENT_COMMON_SANDBOX_INIT_MAC_H_
