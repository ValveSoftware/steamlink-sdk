// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef PPAPI_NACL_IRT_PLUGIN_STARTUP_H_
#define PPAPI_NACL_IRT_PLUGIN_STARTUP_H_

#include "ppapi/proxy/ppapi_proxy_export.h"

namespace base {
class Thread;
class WaitableEvent;
}  // namespace base

namespace ppapi {

class ManifestService;

// Sets the IPC channels for the browser and the renderer by the given FD
// numbers. This will be used for non-SFI mode. Must be called before the
// ppapi_start() IRT interface is called.
PPAPI_PROXY_EXPORT void SetIPCFileDescriptors(
    int browser_ipc_fd, int renderer_ipc_fd, int manifest_service_fd);

// Runs start up procedure for the plugin.
// Specifically, start background IO thread for IPC, and prepare
// shutdown event instance.
PPAPI_PROXY_EXPORT void StartUpPlugin();

// Returns IPC file descriptor for PPAPI to the browser.
int GetBrowserIPCFileDescriptor();

// Returns IPC file descriptor for PPAPI to the renderer.
int GetRendererIPCFileDescriptor();

// Returns the shutdown event instance for the plugin.
// Must be called after StartUpPlugin().
base::WaitableEvent* GetShutdownEvent();

// Returns the IOThread for the plugin. Must be called after StartUpPlugin().
base::Thread* GetIOThread();

// Returns the ManifestService interface. To use this, manifest_service_fd
// needs to be set via SetIPCFileDescriptors. Must be called after
// StartUpPlugin().
// If not available, returns NULL.
ManifestService* GetManifestService();

}  // namespace ppapi

#endif  // PPAPI_NACL_IRT_PLUGIN_STARTUP_H_
