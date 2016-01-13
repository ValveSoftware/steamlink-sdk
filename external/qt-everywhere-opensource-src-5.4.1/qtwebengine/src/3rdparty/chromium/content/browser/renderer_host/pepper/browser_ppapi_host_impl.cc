// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/browser/renderer_host/pepper/browser_ppapi_host_impl.h"

#include "base/metrics/sparse_histogram.h"
#include "content/browser/renderer_host/pepper/pepper_message_filter.h"
#include "content/browser/tracing/trace_message_filter.h"
#include "content/common/pepper_renderer_instance_data.h"
#include "content/public/common/process_type.h"
#include "ipc/ipc_message_macros.h"
#include "ppapi/proxy/ppapi_messages.h"

namespace content {

// static
BrowserPpapiHost* BrowserPpapiHost::CreateExternalPluginProcess(
    IPC::Sender* sender,
    ppapi::PpapiPermissions permissions,
    base::ProcessHandle plugin_child_process,
    IPC::ChannelProxy* channel,
    int render_process_id,
    int render_view_id,
    const base::FilePath& profile_directory) {
  // The plugin name and path shouldn't be needed for external plugins.
  BrowserPpapiHostImpl* browser_ppapi_host =
      new BrowserPpapiHostImpl(sender,
                               permissions,
                               std::string(),
                               base::FilePath(),
                               profile_directory,
                               false /* in_process */,
                               true /* external_plugin */);
  browser_ppapi_host->set_plugin_process_handle(plugin_child_process);

  scoped_refptr<PepperMessageFilter> pepper_message_filter(
      new PepperMessageFilter());
  channel->AddFilter(pepper_message_filter->GetFilter());
  channel->AddFilter(browser_ppapi_host->message_filter());
  channel->AddFilter((new TraceMessageFilter())->GetFilter());

  return browser_ppapi_host;
}

BrowserPpapiHostImpl::BrowserPpapiHostImpl(
    IPC::Sender* sender,
    const ppapi::PpapiPermissions& permissions,
    const std::string& plugin_name,
    const base::FilePath& plugin_path,
    const base::FilePath& profile_data_directory,
    bool in_process,
    bool external_plugin)
    : ppapi_host_(new ppapi::host::PpapiHost(sender, permissions)),
      plugin_process_handle_(base::kNullProcessHandle),
      plugin_name_(plugin_name),
      plugin_path_(plugin_path),
      profile_data_directory_(profile_data_directory),
      in_process_(in_process),
      external_plugin_(external_plugin),
      ssl_context_helper_(new SSLContextHelper()) {
  message_filter_ = new HostMessageFilter(ppapi_host_.get(), this);
  ppapi_host_->AddHostFactoryFilter(scoped_ptr<ppapi::host::HostFactory>(
      new ContentBrowserPepperHostFactory(this)));
}

BrowserPpapiHostImpl::~BrowserPpapiHostImpl() {
  // Notify the filter so it won't foward messages to us.
  message_filter_->OnHostDestroyed();

  // Delete the host explicitly first. This shutdown will destroy the
  // resources, which may want to do cleanup in their destructors and expect
  // their pointers to us to be valid.
  ppapi_host_.reset();
}

ppapi::host::PpapiHost* BrowserPpapiHostImpl::GetPpapiHost() {
  return ppapi_host_.get();
}

base::ProcessHandle BrowserPpapiHostImpl::GetPluginProcessHandle() const {
  // Handle should previously have been set before use.
  DCHECK(in_process_ || plugin_process_handle_ != base::kNullProcessHandle);
  return plugin_process_handle_;
}

bool BrowserPpapiHostImpl::IsValidInstance(PP_Instance instance) const {
  return instance_map_.find(instance) != instance_map_.end();
}

bool BrowserPpapiHostImpl::GetRenderFrameIDsForInstance(
    PP_Instance instance,
    int* render_process_id,
    int* render_frame_id) const {
  InstanceMap::const_iterator found = instance_map_.find(instance);
  if (found == instance_map_.end()) {
    *render_process_id = 0;
    *render_frame_id = 0;
    return false;
  }

  *render_process_id = found->second.render_process_id;
  *render_frame_id = found->second.render_frame_id;
  return true;
}

const std::string& BrowserPpapiHostImpl::GetPluginName() {
  return plugin_name_;
}

const base::FilePath& BrowserPpapiHostImpl::GetPluginPath() {
  return plugin_path_;
}

const base::FilePath& BrowserPpapiHostImpl::GetProfileDataDirectory() {
  return profile_data_directory_;
}

GURL BrowserPpapiHostImpl::GetDocumentURLForInstance(PP_Instance instance) {
  InstanceMap::const_iterator found = instance_map_.find(instance);
  if (found == instance_map_.end())
    return GURL();
  return found->second.document_url;
}

GURL BrowserPpapiHostImpl::GetPluginURLForInstance(PP_Instance instance) {
  InstanceMap::const_iterator found = instance_map_.find(instance);
  if (found == instance_map_.end())
    return GURL();
  return found->second.plugin_url;
}

void BrowserPpapiHostImpl::SetOnKeepaliveCallback(
    const BrowserPpapiHost::OnKeepaliveCallback& callback) {
  on_keepalive_callback_ = callback;
}

void BrowserPpapiHostImpl::AddInstance(
    PP_Instance instance,
    const PepperRendererInstanceData& instance_data) {
  DCHECK(instance_map_.find(instance) == instance_map_.end());
  instance_map_[instance] = instance_data;
}

void BrowserPpapiHostImpl::DeleteInstance(PP_Instance instance) {
  InstanceMap::iterator found = instance_map_.find(instance);
  if (found == instance_map_.end()) {
    NOTREACHED();
    return;
  }
  instance_map_.erase(found);
}

BrowserPpapiHostImpl::HostMessageFilter::HostMessageFilter(
    ppapi::host::PpapiHost* ppapi_host,
    BrowserPpapiHostImpl* browser_ppapi_host_impl)
    : ppapi_host_(ppapi_host),
      browser_ppapi_host_impl_(browser_ppapi_host_impl) {}

bool BrowserPpapiHostImpl::HostMessageFilter::OnMessageReceived(
    const IPC::Message& msg) {
  // Don't forward messages if our owner object has been destroyed.
  if (!ppapi_host_)
    return false;

  bool handled = true;
  IPC_BEGIN_MESSAGE_MAP(BrowserPpapiHostImpl::HostMessageFilter, msg)
  // Add necessary message handlers here.
  IPC_MESSAGE_HANDLER(PpapiHostMsg_Keepalive, OnKeepalive)
  IPC_MESSAGE_HANDLER(PpapiHostMsg_LogInterfaceUsage,
                      OnHostMsgLogInterfaceUsage)
  IPC_MESSAGE_UNHANDLED(handled = ppapi_host_->OnMessageReceived(msg))
  IPC_END_MESSAGE_MAP();
  return handled;
}

void BrowserPpapiHostImpl::HostMessageFilter::OnHostDestroyed() {
  DCHECK(ppapi_host_);
  ppapi_host_ = NULL;
  browser_ppapi_host_impl_ = NULL;
}

BrowserPpapiHostImpl::HostMessageFilter::~HostMessageFilter() {}

void BrowserPpapiHostImpl::HostMessageFilter::OnKeepalive() {
  if (browser_ppapi_host_impl_)
    browser_ppapi_host_impl_->OnKeepalive();
}

void BrowserPpapiHostImpl::HostMessageFilter::OnHostMsgLogInterfaceUsage(
    int hash) const {
  UMA_HISTOGRAM_SPARSE_SLOWLY("Pepper.InterfaceUsed", hash);
}

void BrowserPpapiHostImpl::OnKeepalive() {
  // An instance has been active. The on_keepalive_callback_ will be
  // used to permit the content embedder to handle this, e.g. by tracking
  // activity and shutting down processes that go idle.
  //
  // Currently embedders do not need to distinguish between instances having
  // different idle state, and thus this implementation handles all instances
  // for this module together.

  if (on_keepalive_callback_.is_null())
    return;

  BrowserPpapiHost::OnKeepaliveInstanceData instance_data(instance_map_.size());

  InstanceMap::iterator instance = instance_map_.begin();
  int i = 0;
  while (instance != instance_map_.end()) {
    instance_data[i].render_process_id = instance->second.render_process_id;
    instance_data[i].render_frame_id = instance->second.render_frame_id;
    instance_data[i].document_url = instance->second.document_url;
    ++instance;
    ++i;
  }
  on_keepalive_callback_.Run(instance_data, profile_data_directory_);
}

}  // namespace content
