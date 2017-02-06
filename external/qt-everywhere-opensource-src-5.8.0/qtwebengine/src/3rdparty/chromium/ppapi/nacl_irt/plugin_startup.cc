// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ppapi/nacl_irt/plugin_startup.h"

#include "base/bind.h"
#include "base/file_descriptor_posix.h"
#include "base/location.h"
#include "base/logging.h"
#include "base/single_thread_task_runner.h"
#include "base/synchronization/waitable_event.h"
#include "base/threading/thread.h"
#include "ipc/ipc_channel_handle.h"
#include "mojo/edk/embedder/embedder.h"
#include "ppapi/nacl_irt/manifest_service.h"
#include "ppapi/shared_impl/ppb_audio_shared.h"

namespace ppapi {
namespace {

int g_nacl_browser_ipc_fd = -1;
int g_nacl_renderer_ipc_fd = -1;
int g_manifest_service_fd = -1;

base::WaitableEvent* g_shutdown_event = NULL;
base::Thread* g_io_thread = NULL;
ManifestService* g_manifest_service = NULL;

// Creates the manifest service on IO thread so that its Listener's thread and
// IO thread are shared. Upon completion of the manifest service creation,
// event is signaled.
void StartUpManifestServiceOnIOThread(base::WaitableEvent* event) {
  // The start up must be called only once.
  DCHECK(!g_manifest_service);
  // manifest_service_fd must be set.
  DCHECK_NE(g_manifest_service_fd, -1);
  // IOThread and shutdown event must be initialized in advance.
  DCHECK(g_io_thread);
  DCHECK(g_shutdown_event);

  g_manifest_service = new ManifestService(
      IPC::ChannelHandle("NaCl IPC",
                         base::FileDescriptor(g_manifest_service_fd, false)),
      g_io_thread->task_runner(), g_shutdown_event);
  event->Signal();
}

}  // namespace

void SetIPCFileDescriptors(
    int browser_ipc_fd, int renderer_ipc_fd, int manifest_service_fd) {
  // The initialization must be only once.
  DCHECK_EQ(g_nacl_browser_ipc_fd, -1);
  DCHECK_EQ(g_nacl_renderer_ipc_fd, -1);
  DCHECK_EQ(g_manifest_service_fd, -1);
  g_nacl_browser_ipc_fd = browser_ipc_fd;
  g_nacl_renderer_ipc_fd = renderer_ipc_fd;
  g_manifest_service_fd = manifest_service_fd;
}

void StartUpPlugin() {
  // The start up must be called only once.
  DCHECK(!g_shutdown_event);
  DCHECK(!g_io_thread);

  // The Mojo EDK must be initialized before using IPC.
  mojo::edk::Init();

  g_shutdown_event =
      new base::WaitableEvent(base::WaitableEvent::ResetPolicy::MANUAL,
                              base::WaitableEvent::InitialState::NOT_SIGNALED);
  g_io_thread = new base::Thread("Chrome_NaClIOThread");
  g_io_thread->StartWithOptions(
      base::Thread::Options(base::MessageLoop::TYPE_IO, 0));

  if (g_manifest_service_fd != -1) {
    // Manifest service must be created on IOThread so that the main message
    // handling will be done on the thread, which has a message loop
    // even before irt_ppapi_start invocation.
    // TODO(hidehiko,dmichael): This works, but is probably not well designed
    // usage. Once a better approach is made, replace this by that way.
    // (crbug.com/364241).
    base::WaitableEvent event(base::WaitableEvent::ResetPolicy::MANUAL,
                              base::WaitableEvent::InitialState::NOT_SIGNALED);
    g_io_thread->task_runner()->PostTask(
        FROM_HERE, base::Bind(StartUpManifestServiceOnIOThread, &event));
    event.Wait();
  }

  PPB_Audio_Shared::SetNaClMode();
}

int GetBrowserIPCFileDescriptor() {
  // The descriptor must be initialized in advance.
  DCHECK_NE(g_nacl_browser_ipc_fd, -1);
  return g_nacl_browser_ipc_fd;
}

int GetRendererIPCFileDescriptor() {
  // The descriptor must be initialized in advance.
  DCHECK_NE(g_nacl_renderer_ipc_fd, -1);
  return g_nacl_renderer_ipc_fd;
}

base::WaitableEvent* GetShutdownEvent() {
  // The shutdown event must be initialized in advance.
  DCHECK(g_shutdown_event);
  return g_shutdown_event;
}

base::Thread* GetIOThread() {
  // The IOThread must be initialized in advance.
  DCHECK(g_io_thread);
  return g_io_thread;
}

ManifestService* GetManifestService() {
  return g_manifest_service;
}

}  // namespace ppapi
