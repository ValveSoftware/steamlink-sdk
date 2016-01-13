// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_BROWSER_BOOTSTRAP_SANDBOX_MAC_H_
#define CONTENT_BROWSER_BOOTSTRAP_SANDBOX_MAC_H_

namespace sandbox {
class BootstrapSandbox;
}

namespace content {

// Whether or not the bootstrap sandbox should be enabled.
bool ShouldEnableBootstrapSandbox();

// Returns the singleton instance of the BootstrapSandox. The returned object
// is thread-safe.
// On the first call to this function, the sandbox will be created and all
// the policies will be registered with it.
sandbox::BootstrapSandbox* GetBootstrapSandbox();

}  // namespace content

#endif  // CONTENT_BROWSER_BOOTSTRAP_SANDBOX_MAC_H_
