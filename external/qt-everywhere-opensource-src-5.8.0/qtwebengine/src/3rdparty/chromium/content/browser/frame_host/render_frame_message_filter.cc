// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/browser/frame_host/render_frame_message_filter.h"

#include "base/command_line.h"
#include "base/macros.h"
#include "base/metrics/field_trial.h"
#include "base/strings/string_util.h"
#include "build/build_config.h"
#include "content/browser/bad_message.h"
#include "content/browser/blob_storage/chrome_blob_storage_context.h"
#include "content/browser/child_process_security_policy_impl.h"
#include "content/browser/download/download_stats.h"
#include "content/browser/frame_host/render_frame_host_impl.h"
#include "content/browser/gpu/gpu_data_manager_impl.h"
#include "content/browser/renderer_host/render_widget_helper.h"
#include "content/browser/resource_context_impl.h"
#include "content/common/content_constants_internal.h"
#include "content/common/frame_messages.h"
#include "content/common/view_messages.h"
#include "content/public/browser/browser_context.h"
#include "content/public/browser/browser_thread.h"
#include "content/public/browser/download_manager.h"
#include "content/public/browser/download_url_parameters.h"
#include "content/public/common/content_constants.h"
#include "content/public/common/content_switches.h"
#include "gpu/GLES2/gl2extchromium.h"
#include "net/base/registry_controlled_domains/registry_controlled_domain.h"
#include "net/cookies/cookie_options.h"
#include "net/cookies/cookie_store.h"
#include "net/url_request/url_request_context.h"
#include "net/url_request/url_request_context_getter.h"
#include "storage/browser/blob/blob_storage_context.h"
#include "url/gurl.h"

#if !defined(OS_MACOSX)
#include "third_party/khronos/GLES2/gl2.h"
#include "third_party/khronos/GLES2/gl2ext.h"
#endif

#if defined(ENABLE_PLUGINS)
#include "content/browser/plugin_service_impl.h"
#include "content/browser/ppapi_plugin_process_host.h"
#include "content/public/browser/plugin_service_filter.h"
#endif

namespace content {

namespace {

#if defined(ENABLE_PLUGINS)
const int kPluginsRefreshThresholdInSeconds = 3;
#endif

const char kEnforceStrictSecureExperiment[] = "StrictSecureCookies";

void CreateChildFrameOnUI(
    int process_id,
    int parent_routing_id,
    blink::WebTreeScopeType scope,
    const std::string& frame_name,
    const std::string& frame_unique_name,
    blink::WebSandboxFlags sandbox_flags,
    const blink::WebFrameOwnerProperties& frame_owner_properties,
    int new_routing_id) {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);
  RenderFrameHostImpl* render_frame_host =
      RenderFrameHostImpl::FromID(process_id, parent_routing_id);
  // Handles the RenderFrameHost being deleted on the UI thread while
  // processing a subframe creation message.
  if (render_frame_host) {
    render_frame_host->OnCreateChildFrame(new_routing_id, scope, frame_name,
                                          frame_unique_name, sandbox_flags,
                                          frame_owner_properties);
  }
}

void DownloadUrlOnUIThread(std::unique_ptr<DownloadUrlParameters> parameters) {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);

  RenderProcessHost* render_process_host =
      RenderProcessHost::FromID(parameters->render_process_host_id());
  if (!render_process_host)
    return;

  BrowserContext* browser_context = render_process_host->GetBrowserContext();
  DownloadManager* download_manager =
      BrowserContext::GetDownloadManager(browser_context);
  RecordDownloadSource(INITIATED_BY_RENDERER);
  download_manager->DownloadUrl(std::move(parameters));
}

// Common functionality for converting a sync renderer message to a callback
// function in the browser. Derive from this, create it on the heap when
// issuing your callback. When done, write your reply parameters into
// reply_msg(), and then call SendReplyAndDeleteThis().
class RenderMessageCompletionCallback {
 public:
  RenderMessageCompletionCallback(RenderFrameMessageFilter* filter,
                                  IPC::Message* reply_msg)
      : filter_(filter),
        reply_msg_(reply_msg) {
  }

  virtual ~RenderMessageCompletionCallback() {
    if (reply_msg_) {
      // If the owner of this class failed to call SendReplyAndDeleteThis(),
      // send an error reply to prevent the renderer from being hung.
      reply_msg_->set_reply_error();
      filter_->Send(reply_msg_);
    }
  }

  RenderFrameMessageFilter* filter() { return filter_.get(); }
  IPC::Message* reply_msg() { return reply_msg_; }

  void SendReplyAndDeleteThis() {
    filter_->Send(reply_msg_);
    reply_msg_ = nullptr;
    delete this;
  }

 private:
  scoped_refptr<RenderFrameMessageFilter> filter_;
  IPC::Message* reply_msg_;
};

}  // namespace

#if defined(ENABLE_PLUGINS)

class RenderFrameMessageFilter::OpenChannelToPpapiBrokerCallback
    : public PpapiPluginProcessHost::BrokerClient {
 public:
  OpenChannelToPpapiBrokerCallback(RenderFrameMessageFilter* filter,
                                   int routing_id)
      : filter_(filter),
        routing_id_(routing_id) {
  }

  ~OpenChannelToPpapiBrokerCallback() override {}

  void GetPpapiChannelInfo(base::ProcessHandle* renderer_handle,
                           int* renderer_id) override {
    // base::kNullProcessHandle indicates that the channel will be used by the
    // browser itself. Make sure we never output that value here.
    CHECK_NE(base::kNullProcessHandle, filter_->PeerHandle());
    *renderer_handle = filter_->PeerHandle();
    *renderer_id = filter_->render_process_id_;
  }

  void OnPpapiChannelOpened(const IPC::ChannelHandle& channel_handle,
                            base::ProcessId plugin_pid,
                            int /* plugin_child_id */) override {
    filter_->Send(new ViewMsg_PpapiBrokerChannelCreated(routing_id_,
                                                        plugin_pid,
                                                        channel_handle));
    delete this;
  }

  bool OffTheRecord() override { return filter_->incognito_; }

 private:
  scoped_refptr<RenderFrameMessageFilter> filter_;
  int routing_id_;
};

class RenderFrameMessageFilter::OpenChannelToPpapiPluginCallback
    : public RenderMessageCompletionCallback,
      public PpapiPluginProcessHost::PluginClient {
 public:
  OpenChannelToPpapiPluginCallback(RenderFrameMessageFilter* filter,
                                   ResourceContext* context,
                                   IPC::Message* reply_msg)
      : RenderMessageCompletionCallback(filter, reply_msg),
        context_(context) {
  }

  void GetPpapiChannelInfo(base::ProcessHandle* renderer_handle,
                           int* renderer_id) override {
    // base::kNullProcessHandle indicates that the channel will be used by the
    // browser itself. Make sure we never output that value here.
    CHECK_NE(base::kNullProcessHandle, filter()->PeerHandle());
    *renderer_handle = filter()->PeerHandle();
    *renderer_id = filter()->render_process_id_;
  }

  void OnPpapiChannelOpened(const IPC::ChannelHandle& channel_handle,
                            base::ProcessId plugin_pid,
                            int plugin_child_id) override {
    FrameHostMsg_OpenChannelToPepperPlugin::WriteReplyParams(
        reply_msg(), channel_handle, plugin_pid, plugin_child_id);
    SendReplyAndDeleteThis();
  }

  bool OffTheRecord() override { return filter()->incognito_; }

  ResourceContext* GetResourceContext() override { return context_; }

 private:
  ResourceContext* context_;
};

#endif  // ENABLE_PLUGINS

RenderFrameMessageFilter::RenderFrameMessageFilter(
    int render_process_id,
    PluginServiceImpl* plugin_service,
    BrowserContext* browser_context,
    net::URLRequestContextGetter* request_context,
    RenderWidgetHelper* render_widget_helper)
    : BrowserMessageFilter(FrameMsgStart),
#if defined(ENABLE_PLUGINS)
      plugin_service_(plugin_service),
      profile_data_directory_(browser_context->GetPath()),
#endif  // ENABLE_PLUGINS
      request_context_(request_context),
      resource_context_(browser_context->GetResourceContext()),
      render_widget_helper_(render_widget_helper),
      incognito_(browser_context->IsOffTheRecord()),
      render_process_id_(render_process_id) {
}

RenderFrameMessageFilter::~RenderFrameMessageFilter() {
  // This function should be called on the IO thread.
  DCHECK_CURRENTLY_ON(BrowserThread::IO);
}

bool RenderFrameMessageFilter::OnMessageReceived(const IPC::Message& message) {
  bool handled = true;
  IPC_BEGIN_MESSAGE_MAP(RenderFrameMessageFilter, message)
    IPC_MESSAGE_HANDLER(FrameHostMsg_CreateChildFrame, OnCreateChildFrame)
    IPC_MESSAGE_HANDLER(FrameHostMsg_SetCookie, OnSetCookie)
    IPC_MESSAGE_HANDLER_DELAY_REPLY(FrameHostMsg_GetCookies, OnGetCookies)
    IPC_MESSAGE_HANDLER(FrameHostMsg_CookiesEnabled, OnCookiesEnabled)
    IPC_MESSAGE_HANDLER(FrameHostMsg_DownloadUrl, OnDownloadUrl)
    IPC_MESSAGE_HANDLER(FrameHostMsg_SaveImageFromDataURL,
                        OnSaveImageFromDataURL)
    IPC_MESSAGE_HANDLER(FrameHostMsg_Are3DAPIsBlocked, OnAre3DAPIsBlocked)
    IPC_MESSAGE_HANDLER_GENERIC(FrameHostMsg_RenderProcessGone,
                                OnRenderProcessGone())
#if defined(ENABLE_PLUGINS)
    IPC_MESSAGE_HANDLER_DELAY_REPLY(FrameHostMsg_GetPlugins, OnGetPlugins)
    IPC_MESSAGE_HANDLER(FrameHostMsg_GetPluginInfo, OnGetPluginInfo)
    IPC_MESSAGE_HANDLER_DELAY_REPLY(FrameHostMsg_OpenChannelToPepperPlugin,
                                    OnOpenChannelToPepperPlugin)
    IPC_MESSAGE_HANDLER(FrameHostMsg_DidCreateOutOfProcessPepperInstance,
                        OnDidCreateOutOfProcessPepperInstance)
    IPC_MESSAGE_HANDLER(FrameHostMsg_DidDeleteOutOfProcessPepperInstance,
                        OnDidDeleteOutOfProcessPepperInstance)
    IPC_MESSAGE_HANDLER(FrameHostMsg_OpenChannelToPpapiBroker,
                        OnOpenChannelToPpapiBroker)
    IPC_MESSAGE_HANDLER(FrameHostMsg_PluginInstanceThrottleStateChange,
                        OnPluginInstanceThrottleStateChange)
#endif  // ENABLE_PLUGINS
    IPC_MESSAGE_UNHANDLED(handled = false)
  IPC_END_MESSAGE_MAP()

  return handled;
}

void RenderFrameMessageFilter::DownloadUrl(int render_view_id,
                                           int render_frame_id,
                                           const GURL& url,
                                           const Referrer& referrer,
                                           const base::string16& suggested_name,
                                           const bool use_prompt) const {
  if (!resource_context_)
    return;

  std::unique_ptr<DownloadUrlParameters> parameters(
      new DownloadUrlParameters(url, render_process_id_, render_view_id,
                                render_frame_id, request_context_.get()));
  parameters->set_content_initiated(true);
  parameters->set_suggested_name(suggested_name);
  parameters->set_prompt(use_prompt);
  parameters->set_referrer(referrer);

  if (url.SchemeIsBlob()) {
    ChromeBlobStorageContext* blob_context =
        GetChromeBlobStorageContextForResourceContext(resource_context_);
    parameters->set_blob_data_handle(
        blob_context->context()->GetBlobDataFromPublicURL(url));
    // Don't care if the above fails. We are going to let the download go
    // through and allow it to be interrupted so that the embedder can deal.
  }

  BrowserThread::PostTask(
      BrowserThread::UI, FROM_HERE,
      base::Bind(&DownloadUrlOnUIThread, base::Passed(&parameters)));
}

void RenderFrameMessageFilter::OnCreateChildFrame(
    const FrameHostMsg_CreateChildFrame_Params& params,
    int* new_routing_id) {
  *new_routing_id = render_widget_helper_->GetNextRoutingID();
  BrowserThread::PostTask(
      BrowserThread::UI, FROM_HERE,
      base::Bind(&CreateChildFrameOnUI, render_process_id_,
                 params.parent_routing_id, params.scope, params.frame_name,
                 params.frame_unique_name, params.sandbox_flags,
                 params.frame_owner_properties, *new_routing_id));
}

void RenderFrameMessageFilter::OnSetCookie(int render_frame_id,
                                           const GURL& url,
                                           const GURL& first_party_for_cookies,
                                           const std::string& cookie) {
  ChildProcessSecurityPolicyImpl* policy =
      ChildProcessSecurityPolicyImpl::GetInstance();
  if (!policy->CanAccessDataForOrigin(render_process_id_, url)) {
    bad_message::ReceivedBadMessage(this,
                                    bad_message::RFMF_SET_COOKIE_BAD_ORIGIN);
    return;
  }

  net::CookieOptions options;
  bool experimental_web_platform_features_enabled =
      base::CommandLine::ForCurrentProcess()->HasSwitch(
          switches::kEnableExperimentalWebPlatformFeatures);
  const std::string enforce_strict_secure_group =
      base::FieldTrialList::FindFullName(kEnforceStrictSecureExperiment);
  if (experimental_web_platform_features_enabled ||
      base::StartsWith(enforce_strict_secure_group, "Enabled",
                       base::CompareCase::INSENSITIVE_ASCII)) {
    options.set_enforce_strict_secure();
  }
  if (GetContentClient()->browser()->AllowSetCookie(
          url, first_party_for_cookies, cookie, resource_context_,
          render_process_id_, render_frame_id, options)) {
    net::URLRequestContext* context = GetRequestContextForURL(url);
    // Pass a null callback since we don't care about when the 'set' completes.
    context->cookie_store()->SetCookieWithOptionsAsync(
        url, cookie, options, net::CookieStore::SetCookiesCallback());
  }
}

void RenderFrameMessageFilter::OnGetCookies(int render_frame_id,
                                            const GURL& url,
                                            const GURL& first_party_for_cookies,
                                            IPC::Message* reply_msg) {
  ChildProcessSecurityPolicyImpl* policy =
      ChildProcessSecurityPolicyImpl::GetInstance();
  if (!policy->CanAccessDataForOrigin(render_process_id_, url)) {
    bad_message::ReceivedBadMessage(this,
                                    bad_message::RFMF_GET_COOKIES_BAD_ORIGIN);
    delete reply_msg;
    return;
  }

  // If we crash here, figure out what URL the renderer was requesting.
  // http://crbug.com/99242
  char url_buf[128];
  base::strlcpy(url_buf, url.spec().c_str(), arraysize(url_buf));
  base::debug::Alias(url_buf);

  net::URLRequestContext* context = GetRequestContextForURL(url);

  net::CookieOptions options;
  if (net::registry_controlled_domains::SameDomainOrHost(
          url, first_party_for_cookies,
          net::registry_controlled_domains::INCLUDE_PRIVATE_REGISTRIES)) {
    // TODO(mkwst): This check ought to further distinguish between frames
    // initiated in a strict or lax same-site context.
    options.set_same_site_cookie_mode(
        net::CookieOptions::SameSiteCookieMode::INCLUDE_STRICT_AND_LAX);
  } else {
    options.set_same_site_cookie_mode(
        net::CookieOptions::SameSiteCookieMode::DO_NOT_INCLUDE);
  }

  context->cookie_store()->GetCookieListWithOptionsAsync(
      url, options,
      base::Bind(&RenderFrameMessageFilter::CheckPolicyForCookies, this,
                 render_frame_id, url, first_party_for_cookies, reply_msg));
}

void RenderFrameMessageFilter::OnCookiesEnabled(
    int render_frame_id,
    const GURL& url,
    const GURL& first_party_for_cookies,
    bool* cookies_enabled) {
  // TODO(ananta): If this render frame is associated with an automation
  // channel, aka ChromeFrame then we need to retrieve cookie settings from the
  // external host.
  *cookies_enabled = GetContentClient()->browser()->AllowGetCookie(
      url, first_party_for_cookies, net::CookieList(), resource_context_,
      render_process_id_, render_frame_id);
}

void RenderFrameMessageFilter::CheckPolicyForCookies(
    int render_frame_id,
    const GURL& url,
    const GURL& first_party_for_cookies,
    IPC::Message* reply_msg,
    const net::CookieList& cookie_list) {
  net::URLRequestContext* context = GetRequestContextForURL(url);
  // Check the policy for get cookies, and pass cookie_list to the
  // TabSpecificContentSetting for logging purpose.
  if (context &&
      GetContentClient()->browser()->AllowGetCookie(
          url, first_party_for_cookies, cookie_list, resource_context_,
          render_process_id_, render_frame_id)) {
    SendGetCookiesResponse(reply_msg,
                           net::CookieStore::BuildCookieLine(cookie_list));
  } else {
    SendGetCookiesResponse(reply_msg, std::string());
  }
}

void RenderFrameMessageFilter::SendGetCookiesResponse(
    IPC::Message* reply_msg,
    const std::string& cookies) {
  FrameHostMsg_GetCookies::WriteReplyParams(reply_msg, cookies);
  Send(reply_msg);
}

void RenderFrameMessageFilter::OnDownloadUrl(
    int render_view_id,
    int render_frame_id,
    const GURL& url,
    const Referrer& referrer,
    const base::string16& suggested_name) {
  DownloadUrl(render_view_id, render_frame_id, url, referrer, suggested_name,
              false);
}

void RenderFrameMessageFilter::OnSaveImageFromDataURL(
    int render_view_id,
    int render_frame_id,
    const std::string& url_str) {
  // Please refer to RenderFrameImpl::saveImageFromDataURL().
  if (url_str.length() >= kMaxLengthOfDataURLString)
    return;

  GURL data_url(url_str);
  if (!data_url.is_valid() || !data_url.SchemeIs(url::kDataScheme))
    return;

  DownloadUrl(render_view_id, render_frame_id, data_url, Referrer(),
              base::string16(), true);
}

void RenderFrameMessageFilter::OnAre3DAPIsBlocked(int render_frame_id,
                                                  const GURL& top_origin_url,
                                                  ThreeDAPIType requester,
                                                  bool* blocked) {
  *blocked = GpuDataManagerImpl::GetInstance()->Are3DAPIsBlocked(
      top_origin_url, render_process_id_, render_frame_id, requester);
}

void RenderFrameMessageFilter::OnRenderProcessGone() {
  // FrameHostMessage_RenderProcessGone is a synthetic IPC message used by
  // RenderProcessHostImpl to clean things up after a crash (it's injected
  // downstream of this filter). Allowing it to proceed would enable a renderer
  // to fake its own death; instead, actually kill the renderer.
  bad_message::ReceivedBadMessage(
      this, bad_message::RFMF_RENDERER_FAKED_ITS_OWN_DEATH);
}

#if defined(ENABLE_PLUGINS)

void RenderFrameMessageFilter::OnGetPlugins(
    bool refresh,
    IPC::Message* reply_msg) {
  // Don't refresh if the specified threshold has not been passed.  Note that
  // this check is performed before off-loading to the file thread.  The reason
  // we do this is that some pages tend to request that the list of plugins be
  // refreshed at an excessive rate.  This instigates disk scanning, as the list
  // is accumulated by doing multiple reads from disk.  This effect is
  // multiplied when we have several pages requesting this operation.
  if (refresh) {
    const base::TimeDelta threshold = base::TimeDelta::FromSeconds(
        kPluginsRefreshThresholdInSeconds);
    const base::TimeTicks now = base::TimeTicks::Now();
    if (now - last_plugin_refresh_time_ >= threshold) {
      // Only refresh if the threshold hasn't been exceeded yet.
      PluginServiceImpl::GetInstance()->RefreshPlugins();
      last_plugin_refresh_time_ = now;
    }
  }

  PluginServiceImpl::GetInstance()->GetPlugins(base::Bind(
      &RenderFrameMessageFilter::GetPluginsCallback, this, reply_msg));
}

void RenderFrameMessageFilter::GetPluginsCallback(
    IPC::Message* reply_msg,
    const std::vector<WebPluginInfo>& all_plugins) {
  // Filter the plugin list.
  PluginServiceFilter* filter = PluginServiceImpl::GetInstance()->GetFilter();
  std::vector<WebPluginInfo> plugins;

  int child_process_id = -1;
  int routing_id = MSG_ROUTING_NONE;
  // In this loop, copy the WebPluginInfo (and do not use a reference) because
  // the filter might mutate it.
  for (WebPluginInfo plugin : all_plugins) {
    if (!filter || filter->IsPluginAvailable(child_process_id,
                                             routing_id,
                                             resource_context_,
                                             GURL(),
                                             GURL(),
                                             &plugin)) {
      plugins.push_back(plugin);
    }
  }

  FrameHostMsg_GetPlugins::WriteReplyParams(reply_msg, plugins);
  Send(reply_msg);
}

void RenderFrameMessageFilter::OnGetPluginInfo(
    int render_frame_id,
    const GURL& url,
    const GURL& page_url,
    const std::string& mime_type,
    bool* found,
    WebPluginInfo* info,
    std::string* actual_mime_type) {
  bool allow_wildcard = true;
  *found = plugin_service_->GetPluginInfo(
      render_process_id_, render_frame_id, resource_context_,
      url, page_url, mime_type, allow_wildcard,
      nullptr, info, actual_mime_type);
}

void RenderFrameMessageFilter::OnOpenChannelToPepperPlugin(
    const base::FilePath& path,
    IPC::Message* reply_msg) {
  plugin_service_->OpenChannelToPpapiPlugin(
      render_process_id_,
      path,
      profile_data_directory_,
      new OpenChannelToPpapiPluginCallback(this, resource_context_, reply_msg));
}

void RenderFrameMessageFilter::OnDidCreateOutOfProcessPepperInstance(
    int plugin_child_id,
    int32_t pp_instance,
    PepperRendererInstanceData instance_data,
    bool is_external) {
  // It's important that we supply the render process ID ourselves based on the
  // channel the message arrived on. We use the
  //   PP_Instance -> (process id, frame id)
  // mapping to decide how to handle messages received from the (untrusted)
  // plugin, so an exploited renderer must not be able to insert fake mappings
  // that may allow it access to other render processes.
  DCHECK_EQ(0, instance_data.render_process_id);
  instance_data.render_process_id = render_process_id_;
  if (is_external) {
    // We provide the BrowserPpapiHost to the embedder, so it's safe to cast.
    BrowserPpapiHostImpl* host = static_cast<BrowserPpapiHostImpl*>(
        GetContentClient()->browser()->GetExternalBrowserPpapiHost(
            plugin_child_id));
    if (host)
      host->AddInstance(pp_instance, instance_data);
  } else {
    PpapiPluginProcessHost::DidCreateOutOfProcessInstance(
        plugin_child_id, pp_instance, instance_data);
  }
}

void RenderFrameMessageFilter::OnDidDeleteOutOfProcessPepperInstance(
    int plugin_child_id,
    int32_t pp_instance,
    bool is_external) {
  if (is_external) {
    // We provide the BrowserPpapiHost to the embedder, so it's safe to cast.
    BrowserPpapiHostImpl* host = static_cast<BrowserPpapiHostImpl*>(
        GetContentClient()->browser()->GetExternalBrowserPpapiHost(
            plugin_child_id));
    if (host)
      host->DeleteInstance(pp_instance);
  } else {
    PpapiPluginProcessHost::DidDeleteOutOfProcessInstance(
        plugin_child_id, pp_instance);
  }
}

void RenderFrameMessageFilter::OnOpenChannelToPpapiBroker(
    int routing_id,
    const base::FilePath& path) {
  plugin_service_->OpenChannelToPpapiBroker(
      render_process_id_,
      path,
      new OpenChannelToPpapiBrokerCallback(this, routing_id));
}

void RenderFrameMessageFilter::OnPluginInstanceThrottleStateChange(
    int plugin_child_id,
    int32_t pp_instance,
    bool is_throttled) {
  // Feature is only implemented for non-external Plugins.
  PpapiPluginProcessHost::OnPluginInstanceThrottleStateChange(
      plugin_child_id, pp_instance, is_throttled);
}

#endif  // ENABLE_PLUGINS

net::URLRequestContext* RenderFrameMessageFilter::GetRequestContextForURL(
    const GURL& url) {
  DCHECK_CURRENTLY_ON(BrowserThread::IO);

  net::URLRequestContext* context =
      GetContentClient()->browser()->OverrideRequestContextForURL(
          url, resource_context_);
  if (!context)
    context = request_context_->GetURLRequestContext();

  return context;
}

}  // namespace content
