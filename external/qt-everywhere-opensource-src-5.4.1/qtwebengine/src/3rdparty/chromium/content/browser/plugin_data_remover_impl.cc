// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/browser/plugin_data_remover_impl.h"

#include <limits>

#include "base/bind.h"
#include "base/metrics/histogram.h"
#include "base/sequenced_task_runner_helpers.h"
#include "base/strings/utf_string_conversions.h"
#include "base/synchronization/waitable_event.h"
#include "base/version.h"
#include "content/browser/plugin_process_host.h"
#include "content/browser/plugin_service_impl.h"
#include "content/browser/renderer_host/pepper/pepper_flash_file_message_filter.h"
#include "content/common/child_process_host_impl.h"
#include "content/common/plugin_process_messages.h"
#include "content/public/browser/browser_context.h"
#include "content/public/browser/browser_thread.h"
#include "content/public/common/content_constants.h"
#include "content/public/common/pepper_plugin_info.h"
#include "ppapi/proxy/ppapi_messages.h"

namespace content {

namespace {

// The minimum Flash Player version that implements NPP_ClearSiteData.
const char kMinFlashVersion[] = "10.3";
const int64 kRemovalTimeoutMs = 10000;
const uint64 kClearAllData = 0;

}  // namespace

// static
PluginDataRemover* PluginDataRemover::Create(BrowserContext* browser_context) {
  return new PluginDataRemoverImpl(browser_context);
}

// static
void PluginDataRemover::GetSupportedPlugins(
    std::vector<WebPluginInfo>* supported_plugins) {
  bool allow_wildcard = false;
  std::vector<WebPluginInfo> plugins;
  PluginService::GetInstance()->GetPluginInfoArray(
      GURL(), kFlashPluginSwfMimeType, allow_wildcard, &plugins, NULL);
  Version min_version(kMinFlashVersion);
  for (std::vector<WebPluginInfo>::iterator it = plugins.begin();
       it != plugins.end(); ++it) {
    Version version;
    WebPluginInfo::CreateVersionFromString(it->version, &version);
    if (version.IsValid() && min_version.CompareTo(version) == -1)
      supported_plugins->push_back(*it);
  }
}

class PluginDataRemoverImpl::Context
    : public PluginProcessHost::Client,
      public PpapiPluginProcessHost::BrokerClient,
      public IPC::Listener,
      public base::RefCountedThreadSafe<Context,
                                        BrowserThread::DeleteOnIOThread> {
 public:
  Context(base::Time begin_time, BrowserContext* browser_context)
      : event_(new base::WaitableEvent(true, false)),
        begin_time_(begin_time),
        is_removing_(false),
        browser_context_path_(browser_context->GetPath()),
        resource_context_(browser_context->GetResourceContext()) {
    DCHECK(BrowserThread::CurrentlyOn(BrowserThread::UI));
  }

  void Init(const std::string& mime_type) {
    BrowserThread::PostTask(
        BrowserThread::IO,
        FROM_HERE,
        base::Bind(&Context::InitOnIOThread, this, mime_type));
    BrowserThread::PostDelayedTask(
        BrowserThread::IO,
        FROM_HERE,
        base::Bind(&Context::OnTimeout, this),
        base::TimeDelta::FromMilliseconds(kRemovalTimeoutMs));
  }

  void InitOnIOThread(const std::string& mime_type) {
    PluginServiceImpl* plugin_service = PluginServiceImpl::GetInstance();

    // Get the plugin file path.
    std::vector<WebPluginInfo> plugins;
    plugin_service->GetPluginInfoArray(
        GURL(), mime_type, false, &plugins, NULL);
    base::FilePath plugin_path;
    if (!plugins.empty())  // May be empty for some tests.
      plugin_path = plugins[0].path;

    DCHECK(BrowserThread::CurrentlyOn(BrowserThread::IO));
    remove_start_time_ = base::Time::Now();
    is_removing_ = true;
    // Balanced in On[Ppapi]ChannelOpened or OnError. Exactly one them will
    // eventually be called, so we need to keep this object around until then.
    AddRef();

    PepperPluginInfo* pepper_info =
        plugin_service->GetRegisteredPpapiPluginInfo(plugin_path);
    if (pepper_info) {
      plugin_name_ = pepper_info->name;
      // Use the broker since we run this function outside the sandbox.
      plugin_service->OpenChannelToPpapiBroker(0, plugin_path, this);
    } else {
      plugin_service->OpenChannelToNpapiPlugin(
          0, 0, GURL(), GURL(), mime_type, this);
    }
  }

  // Called when a timeout happens in order not to block the client
  // indefinitely.
  void OnTimeout() {
    LOG_IF(ERROR, is_removing_) << "Timed out";
    SignalDone();
  }

  // PluginProcessHost::Client methods.
  virtual int ID() OVERRIDE {
    // Generate a unique identifier for this PluginProcessHostClient.
    return ChildProcessHostImpl::GenerateChildProcessUniqueId();
  }

  virtual bool OffTheRecord() OVERRIDE {
    return false;
  }

  virtual ResourceContext* GetResourceContext() OVERRIDE {
    return resource_context_;
  }

  virtual void SetPluginInfo(const WebPluginInfo& info) OVERRIDE {}

  virtual void OnFoundPluginProcessHost(PluginProcessHost* host) OVERRIDE {}

  virtual void OnSentPluginChannelRequest() OVERRIDE {}

  virtual void OnChannelOpened(const IPC::ChannelHandle& handle) OVERRIDE {
    ConnectToChannel(handle, false);
    // Balancing the AddRef call.
    Release();
  }

  virtual void OnError() OVERRIDE {
    LOG(ERROR) << "Couldn't open plugin channel";
    SignalDone();
    // Balancing the AddRef call.
    Release();
  }

  // PpapiPluginProcessHost::BrokerClient implementation.
  virtual void GetPpapiChannelInfo(base::ProcessHandle* renderer_handle,
                                   int* renderer_id) OVERRIDE {
    *renderer_handle = base::kNullProcessHandle;
    *renderer_id = 0;
  }

  virtual void OnPpapiChannelOpened(
      const IPC::ChannelHandle& channel_handle,
      base::ProcessId  /* peer_pid */,
      int /* child_id */) OVERRIDE {
    if (!channel_handle.name.empty())
      ConnectToChannel(channel_handle, true);

    // Balancing the AddRef call.
    Release();
  }

  // IPC::Listener methods.
  virtual bool OnMessageReceived(const IPC::Message& message) OVERRIDE {
    IPC_BEGIN_MESSAGE_MAP(Context, message)
      IPC_MESSAGE_HANDLER(PluginProcessHostMsg_ClearSiteDataResult,
                          OnClearSiteDataResult)
      IPC_MESSAGE_HANDLER(PpapiHostMsg_ClearSiteDataResult,
                          OnPpapiClearSiteDataResult)
      IPC_MESSAGE_UNHANDLED_ERROR()
    IPC_END_MESSAGE_MAP()

    return true;
  }

  virtual void OnChannelError() OVERRIDE {
    if (is_removing_) {
      NOTREACHED() << "Channel error";
      SignalDone();
    }
  }

  base::WaitableEvent* event() { return event_.get(); }

 private:
  friend struct BrowserThread::DeleteOnThread<BrowserThread::IO>;
  friend class base::DeleteHelper<Context>;
  virtual ~Context() {}

  IPC::Message* CreatePpapiClearSiteDataMsg(uint64 max_age) {
    base::FilePath profile_path =
        PepperFlashFileMessageFilter::GetDataDirName(browser_context_path_);
    // TODO(vtl): This "duplicates" logic in webkit/plugins/ppapi/file_path.cc
    // (which prepends the plugin name to the relative part of the path
    // instead, with the absolute, profile-dependent part being enforced by
    // the browser).
#if defined(OS_WIN)
    base::FilePath plugin_data_path =
        profile_path.Append(base::FilePath(base::UTF8ToUTF16(plugin_name_)));
#else
    base::FilePath plugin_data_path =
        profile_path.Append(base::FilePath(plugin_name_));
#endif  // defined(OS_WIN)
    return new PpapiMsg_ClearSiteData(0u, plugin_data_path, std::string(),
                                      kClearAllData, max_age);
  }

  // Connects the client side of a newly opened plug-in channel.
  void ConnectToChannel(const IPC::ChannelHandle& handle, bool is_ppapi) {
    DCHECK(BrowserThread::CurrentlyOn(BrowserThread::IO));

    // If we timed out, don't bother connecting.
    if (!is_removing_)
      return;

    DCHECK(!channel_.get());
    channel_ = IPC::Channel::CreateClient(handle, this);
    if (!channel_->Connect()) {
      NOTREACHED() << "Couldn't connect to plugin";
      SignalDone();
      return;
    }

    uint64 max_age = begin_time_.is_null() ?
        std::numeric_limits<uint64>::max() :
        (base::Time::Now() - begin_time_).InSeconds();

    IPC::Message* msg;
    if (is_ppapi) {
      msg = CreatePpapiClearSiteDataMsg(max_age);
    } else {
      msg = new PluginProcessMsg_ClearSiteData(
          std::string(), kClearAllData, max_age);
    }
    if (!channel_->Send(msg)) {
      NOTREACHED() << "Couldn't send ClearSiteData message";
      SignalDone();
      return;
    }
  }

  // Handles the PpapiHostMsg_ClearSiteDataResult message by delegating to the
  // PluginProcessHostMsg_ClearSiteDataResult handler.
  void OnPpapiClearSiteDataResult(uint32 request_id, bool success) {
    DCHECK_EQ(0u, request_id);
    OnClearSiteDataResult(success);
  }

  // Handles the PluginProcessHostMsg_ClearSiteDataResult message.
  void OnClearSiteDataResult(bool success) {
    LOG_IF(ERROR, !success) << "ClearSiteData returned error";
    UMA_HISTOGRAM_TIMES("ClearPluginData.time",
                        base::Time::Now() - remove_start_time_);
    SignalDone();
  }

  // Signals that we are finished with removing data (successful or not). This
  // method is safe to call multiple times.
  void SignalDone() {
    DCHECK(BrowserThread::CurrentlyOn(BrowserThread::IO));
    if (!is_removing_)
      return;
    is_removing_ = false;
    event_->Signal();
  }

  scoped_ptr<base::WaitableEvent> event_;
  // The point in time when we start removing data.
  base::Time remove_start_time_;
  // The point in time from which on we remove data.
  base::Time begin_time_;
  bool is_removing_;

  // Path for the current profile. Must be retrieved on the UI thread from the
  // browser context when we start so we can use it later on the I/O thread.
  base::FilePath browser_context_path_;

  // The resource context for the profile. Use only on the I/O thread.
  ResourceContext* resource_context_;

  // The name of the plugin. Use only on the I/O thread.
  std::string plugin_name_;

  // The channel is NULL until we have opened a connection to the plug-in
  // process.
  scoped_ptr<IPC::Channel> channel_;
};


PluginDataRemoverImpl::PluginDataRemoverImpl(BrowserContext* browser_context)
    : mime_type_(kFlashPluginSwfMimeType),
      browser_context_(browser_context) {
}

PluginDataRemoverImpl::~PluginDataRemoverImpl() {
}

base::WaitableEvent* PluginDataRemoverImpl::StartRemoving(
    base::Time begin_time) {
  DCHECK(!context_.get());
  context_ = new Context(begin_time, browser_context_);
  context_->Init(mime_type_);
  return context_->event();
}

}  // namespace content
