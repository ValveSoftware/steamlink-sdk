// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_PUBLIC_BROWSER_GPU_INTERFACE_REGISTRY_H_
#define CONTENT_PUBLIC_BROWSER_GPU_INTERFACE_REGISTRY_H_

#include "content/common/content_export.h"

namespace shell {
class InterfaceProvider;
}

namespace content {

// Get shell::InterfaceProvider that can be used to bind interfaces registered
// via ContentGpuClient::ExposeInterfacesToBrowser().
// This must be called on IO thread.
CONTENT_EXPORT shell::InterfaceProvider* GetGpuRemoteInterfaces();

}  // namespace content

#endif  // CONTENT_PUBLIC_BROWSER_GPU_INTERFACE_REGISTRY_H_
