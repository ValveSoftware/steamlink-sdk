// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/browser/gpu/gpu_process_host_ui_shim.h"

#include <algorithm>

#include "base/bind.h"
#include "base/callback_helpers.h"
#include "base/id_map.h"
#include "base/lazy_instance.h"
#include "base/strings/string_number_conversions.h"
#include "base/trace_event/trace_event.h"
#include "build/build_config.h"
#include "content/browser/compositor/gpu_process_transport_factory.h"
#include "content/browser/gpu/compositor_util.h"
#include "content/browser/gpu/gpu_data_manager_impl.h"
#include "content/browser/gpu/gpu_process_host.h"
#include "content/browser/renderer_host/render_process_host_impl.h"
#include "content/browser/renderer_host/render_view_host_impl.h"
#include "content/browser/renderer_host/render_widget_helper.h"
#include "content/browser/renderer_host/render_widget_host_view_base.h"
#include "content/common/gpu_host_messages.h"
#include "content/public/browser/browser_thread.h"
#include "gpu/ipc/common/memory_stats.h"
#include "ui/gfx/swap_result.h"

#if defined(USE_OZONE)
#include "ui/ozone/public/gpu_platform_support_host.h"
#include "ui/ozone/public/ozone_platform.h"
#endif

namespace content {

namespace {

// One of the linux specific headers defines this as a macro.
#ifdef DestroyAll
#undef DestroyAll
#endif

base::LazyInstance<IDMap<GpuProcessHostUIShim> > g_hosts_by_id =
    LAZY_INSTANCE_INITIALIZER;

void SendOnIOThreadTask(int host_id, IPC::Message* msg) {
  GpuProcessHost* host = GpuProcessHost::FromID(host_id);
  if (host)
    host->Send(msg);
  else
    delete msg;
}

void StopGpuProcessOnIO(int host_id) {
  GpuProcessHost* host = GpuProcessHost::FromID(host_id);
  if (host)
    host->StopGpuProcess();
}

}  // namespace

void RouteToGpuProcessHostUIShimTask(int host_id, const IPC::Message& msg) {
  GpuProcessHostUIShim* ui_shim = GpuProcessHostUIShim::FromID(host_id);
  if (ui_shim)
    ui_shim->OnMessageReceived(msg);
}

GpuProcessHostUIShim::GpuProcessHostUIShim(int host_id)
    : host_id_(host_id) {
  g_hosts_by_id.Pointer()->AddWithID(this, host_id_);
#if defined(USE_OZONE)
  ui::OzonePlatform::GetInstance()
      ->GetGpuPlatformSupportHost()
      ->OnChannelEstablished(
          host_id,
          BrowserThread::GetMessageLoopProxyForThread(BrowserThread::IO),
          base::Bind(&SendOnIOThreadTask, host_id_));
#endif
}

// static
GpuProcessHostUIShim* GpuProcessHostUIShim::Create(int host_id) {
  DCHECK(!FromID(host_id));
  return new GpuProcessHostUIShim(host_id);
}

// static
void GpuProcessHostUIShim::Destroy(int host_id, const std::string& message) {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);

  GpuDataManagerImpl::GetInstance()->AddLogMessage(
      logging::LOG_ERROR, "GpuProcessHostUIShim",
      message);

#if defined(USE_OZONE)
  ui::OzonePlatform::GetInstance()
      ->GetGpuPlatformSupportHost()
      ->OnChannelDestroyed(host_id);
#endif

  delete FromID(host_id);
}

// static
void GpuProcessHostUIShim::DestroyAll() {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);
  while (!g_hosts_by_id.Pointer()->IsEmpty()) {
    IDMap<GpuProcessHostUIShim>::iterator it(g_hosts_by_id.Pointer());
    delete it.GetCurrentValue();
  }
}

// static
GpuProcessHostUIShim* GpuProcessHostUIShim::FromID(int host_id) {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);
  return g_hosts_by_id.Pointer()->Lookup(host_id);
}

// static
GpuProcessHostUIShim* GpuProcessHostUIShim::GetOneInstance() {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);
  if (g_hosts_by_id.Pointer()->IsEmpty())
    return NULL;
  IDMap<GpuProcessHostUIShim>::iterator it(g_hosts_by_id.Pointer());
  return it.GetCurrentValue();
}

bool GpuProcessHostUIShim::Send(IPC::Message* msg) {
  DCHECK(CalledOnValidThread());
  return BrowserThread::PostTask(BrowserThread::IO,
                                 FROM_HERE,
                                 base::Bind(&SendOnIOThreadTask,
                                            host_id_,
                                            msg));
}

bool GpuProcessHostUIShim::OnMessageReceived(const IPC::Message& message) {
  DCHECK(CalledOnValidThread());

#if defined(USE_OZONE)
  if (ui::OzonePlatform::GetInstance()
          ->GetGpuPlatformSupportHost()
          ->OnMessageReceived(message))
    return true;
#endif

  if (message.routing_id() != MSG_ROUTING_CONTROL)
    return false;

  return OnControlMessageReceived(message);
}

void GpuProcessHostUIShim::StopGpuProcess(const base::Closure& callback) {
  close_callback_ = callback;

  BrowserThread::PostTask(
      BrowserThread::IO, FROM_HERE, base::Bind(&StopGpuProcessOnIO, host_id_));
}

void GpuProcessHostUIShim::SimulateRemoveAllContext() {
  Send(new GpuMsg_Clean());
}

void GpuProcessHostUIShim::SimulateCrash() {
  Send(new GpuMsg_Crash());
}

void GpuProcessHostUIShim::SimulateHang() {
  Send(new GpuMsg_Hang());
}

GpuProcessHostUIShim::~GpuProcessHostUIShim() {
  DCHECK(CalledOnValidThread());
  if (!close_callback_.is_null())
    base::ResetAndReturn(&close_callback_).Run();
  g_hosts_by_id.Pointer()->Remove(host_id_);
}

bool GpuProcessHostUIShim::OnControlMessageReceived(
    const IPC::Message& message) {
  DCHECK(CalledOnValidThread());

  IPC_BEGIN_MESSAGE_MAP(GpuProcessHostUIShim, message)
    IPC_MESSAGE_HANDLER(GpuHostMsg_OnLogMessage, OnLogMessage)
    IPC_MESSAGE_HANDLER(GpuHostMsg_GraphicsInfoCollected,
                        OnGraphicsInfoCollected)
    IPC_MESSAGE_HANDLER(GpuHostMsg_VideoMemoryUsageStats,
                        OnVideoMemoryUsageStatsReceived);

    IPC_MESSAGE_UNHANDLED_ERROR()
  IPC_END_MESSAGE_MAP()

  return true;
}

void GpuProcessHostUIShim::OnLogMessage(
    int level,
    const std::string& header,
    const std::string& message) {
  GpuDataManagerImpl::GetInstance()->AddLogMessage(
      level, header, message);
}

void GpuProcessHostUIShim::OnGraphicsInfoCollected(
    const gpu::GPUInfo& gpu_info) {
  // OnGraphicsInfoCollected is sent back after the GPU process successfully
  // initializes GL.
  TRACE_EVENT0("test_gpu", "OnGraphicsInfoCollected");

  GpuDataManagerImpl::GetInstance()->UpdateGpuInfo(gpu_info);
}

void GpuProcessHostUIShim::OnVideoMemoryUsageStatsReceived(
    const gpu::VideoMemoryUsageStats& video_memory_usage_stats) {
  GpuDataManagerImpl::GetInstance()->UpdateVideoMemoryUsageStats(
      video_memory_usage_stats);
}

}  // namespace content
