// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "extensions/browser/process_manager.h"

#include <vector>

#include "base/bind.h"
#include "base/location.h"
#include "base/logging.h"
#include "base/macros.h"
#include "base/metrics/histogram_macros.h"
#include "base/single_thread_task_runner.h"
#include "base/stl_util.h"
#include "base/threading/thread_task_runner_handle.h"
#include "base/time/time.h"
#include "content/public/browser/browser_context.h"
#include "content/public/browser/devtools_agent_host.h"
#include "content/public/browser/notification_service.h"
#include "content/public/browser/render_frame_host.h"
#include "content/public/browser/render_process_host.h"
#include "content/public/browser/render_view_host.h"
#include "content/public/browser/site_instance.h"
#include "content/public/browser/web_contents.h"
#include "content/public/common/url_constants.h"
#include "extensions/browser/extension_host.h"
#include "extensions/browser/extension_registry.h"
#include "extensions/browser/extension_system.h"
#include "extensions/browser/extensions_browser_client.h"
#include "extensions/browser/lazy_background_task_queue.h"
#include "extensions/browser/notification_types.h"
#include "extensions/browser/process_manager_delegate.h"
#include "extensions/browser/process_manager_factory.h"
#include "extensions/browser/process_manager_observer.h"
#include "extensions/browser/view_type_utils.h"
#include "extensions/common/constants.h"
#include "extensions/common/extension.h"
#include "extensions/common/extension_messages.h"
#include "extensions/common/manifest_handlers/background_info.h"
#include "extensions/common/manifest_handlers/incognito_info.h"
#include "extensions/common/one_shot_event.h"

using content::BrowserContext;

namespace extensions {

namespace {

// The time to delay between an extension becoming idle and
// sending a ShouldSuspend message.
// Note: Must be sufficiently larger (e.g. 2x) than
// kKeepaliveThrottleIntervalInSeconds in ppapi/proxy/plugin_globals.
unsigned g_event_page_idle_time_msec = 10000;

// The time to delay between sending a ShouldSuspend message and
// sending a Suspend message.
unsigned g_event_page_suspending_time_msec = 5000;

std::string GetExtensionIdForSiteInstance(
    content::SiteInstance* site_instance) {
  if (!site_instance)
    return std::string();

  // This works for both apps and extensions because the site has been
  // normalized to the extension URL for hosted apps.
  const GURL& site_url = site_instance->GetSiteURL();

  if (!site_url.SchemeIs(kExtensionScheme) &&
      !site_url.SchemeIs(content::kGuestScheme))
    return std::string();

  return site_url.host();
}

std::string GetExtensionID(content::RenderFrameHost* render_frame_host) {
  CHECK(render_frame_host);
  return GetExtensionIdForSiteInstance(render_frame_host->GetSiteInstance());
}

bool IsFrameInExtensionHost(ExtensionHost* extension_host,
                            content::RenderFrameHost* render_frame_host) {
  return content::WebContents::FromRenderFrameHost(render_frame_host) ==
      extension_host->host_contents();
}

// Incognito profiles use this process manager. It is mostly a shim that decides
// whether to fall back on the original profile's ProcessManager based
// on whether a given extension uses "split" or "spanning" incognito behavior.
// TODO(devlin): Given how little this does and the amount of cruft it adds to
// the .h file (in the form of protected members), we should consider just
// moving the incognito logic into the base class.
class IncognitoProcessManager : public ProcessManager {
 public:
  IncognitoProcessManager(BrowserContext* incognito_context,
                          BrowserContext* original_context,
                          ExtensionRegistry* extension_registry);
  ~IncognitoProcessManager() override {}
  bool CreateBackgroundHost(const Extension* extension,
                            const GURL& url) override;
  scoped_refptr<content::SiteInstance> GetSiteInstanceForURL(const GURL& url)
      override;

 private:
  DISALLOW_COPY_AND_ASSIGN(IncognitoProcessManager);
};

static void CreateBackgroundHostForExtensionLoad(
    ProcessManager* manager, const Extension* extension) {
  DVLOG(1) << "CreateBackgroundHostForExtensionLoad";
  if (BackgroundInfo::HasPersistentBackgroundPage(extension))
    manager->CreateBackgroundHost(extension,
                                  BackgroundInfo::GetBackgroundURL(extension));
}

void PropagateExtensionWakeResult(const base::Callback<void(bool)>& callback,
                                  extensions::ExtensionHost* host) {
  callback.Run(host != nullptr);
}

}  // namespace

struct ProcessManager::BackgroundPageData {
  // The count of things keeping the lazy background page alive.
  int lazy_keepalive_count;

  // Tracks if an impulse event has occured since the last polling check.
  bool keepalive_impulse;
  bool previous_keepalive_impulse;

  // True if the page responded to the ShouldSuspend message and is currently
  // dispatching the suspend event. During this time any events that arrive will
  // cancel the suspend process and an onSuspendCanceled event will be
  // dispatched to the page.
  bool is_closing;

  // Stores the value of the incremented
  // ProcessManager::last_background_close_sequence_id_ whenever the extension
  // is active. A copy of the ID is also passed in the callbacks and IPC
  // messages leading up to CloseLazyBackgroundPageNow. The process is aborted
  // if the IDs ever differ due to new activity.
  uint64_t close_sequence_id;

  // Keeps track of when this page was last suspended. Used for perf metrics.
  linked_ptr<base::ElapsedTimer> since_suspended;

  BackgroundPageData()
      : lazy_keepalive_count(0),
        keepalive_impulse(false),
        previous_keepalive_impulse(false),
        is_closing(false),
        close_sequence_id(0) {}
};

// Data of a RenderFrameHost associated with an extension.
struct ProcessManager::ExtensionRenderFrameData {
  // The type of the view.
  extensions::ViewType view_type;

  // Whether the view is keeping the lazy background page alive or not.
  bool has_keepalive;

  ExtensionRenderFrameData()
      : view_type(VIEW_TYPE_INVALID), has_keepalive(false) {}

  // Returns whether the view can keep the lazy background page alive or not.
  bool CanKeepalive() const {
    switch (view_type) {
      case VIEW_TYPE_APP_WINDOW:
      case VIEW_TYPE_BACKGROUND_CONTENTS:
      case VIEW_TYPE_COMPONENT:
      case VIEW_TYPE_EXTENSION_DIALOG:
      case VIEW_TYPE_EXTENSION_GUEST:
      case VIEW_TYPE_EXTENSION_POPUP:
      case VIEW_TYPE_LAUNCHER_PAGE:
      case VIEW_TYPE_PANEL:
      case VIEW_TYPE_TAB_CONTENTS:
        return true;

      case VIEW_TYPE_INVALID:
      case VIEW_TYPE_EXTENSION_BACKGROUND_PAGE:
        return false;
    }
    NOTREACHED();
    return false;
  }
};

//
// ProcessManager
//

// static
ProcessManager* ProcessManager::Get(BrowserContext* context) {
  return ProcessManagerFactory::GetForBrowserContext(context);
}

// static
ProcessManager* ProcessManager::Create(BrowserContext* context) {
  ExtensionRegistry* extension_registry = ExtensionRegistry::Get(context);
  ExtensionsBrowserClient* client = ExtensionsBrowserClient::Get();
  if (client->IsGuestSession(context)) {
    // In the guest session, there is a single off-the-record context.  Unlike
    // a regular incognito mode, background pages of extensions must be
    // created regardless of whether extensions use "spanning" or "split"
    // incognito behavior.
    BrowserContext* original_context = client->GetOriginalContext(context);
    return new ProcessManager(context, original_context, extension_registry);
  }

  if (context->IsOffTheRecord()) {
    BrowserContext* original_context = client->GetOriginalContext(context);
    return new IncognitoProcessManager(
        context, original_context, extension_registry);
  }

  return new ProcessManager(context, context, extension_registry);
}

// static
ProcessManager* ProcessManager::CreateForTesting(
    BrowserContext* context,
    ExtensionRegistry* extension_registry) {
  DCHECK(!context->IsOffTheRecord());
  return new ProcessManager(context, context, extension_registry);
}

// static
ProcessManager* ProcessManager::CreateIncognitoForTesting(
    BrowserContext* incognito_context,
    BrowserContext* original_context,
    ExtensionRegistry* extension_registry) {
  DCHECK(incognito_context->IsOffTheRecord());
  DCHECK(!original_context->IsOffTheRecord());
  return new IncognitoProcessManager(incognito_context,
                                     original_context,
                                     extension_registry);
}

ProcessManager::ProcessManager(BrowserContext* context,
                               BrowserContext* original_context,
                               ExtensionRegistry* extension_registry)
    : extension_registry_(extension_registry),
      site_instance_(content::SiteInstance::Create(context)),
      browser_context_(context),
      startup_background_hosts_created_(false),
      last_background_close_sequence_id_(0),
      weak_ptr_factory_(this) {
  // ExtensionRegistry is shared between incognito and regular contexts.
  DCHECK_EQ(original_context, extension_registry_->browser_context());
  extension_registry_->AddObserver(this);

  if (!context->IsOffTheRecord()) {
    // Only the original profile needs to listen for ready to create background
    // pages for all spanning extensions.
    registrar_.Add(this,
                 extensions::NOTIFICATION_EXTENSIONS_READY_DEPRECATED,
                 content::Source<BrowserContext>(original_context));
  }
  registrar_.Add(this,
                 extensions::NOTIFICATION_EXTENSION_HOST_DESTROYED,
                 content::Source<BrowserContext>(context));
  registrar_.Add(this,
                 extensions::NOTIFICATION_EXTENSION_HOST_VIEW_SHOULD_CLOSE,
                 content::Source<BrowserContext>(context));
  devtools_callback_ = base::Bind(&ProcessManager::OnDevToolsStateChanged,
                                  weak_ptr_factory_.GetWeakPtr());
  content::DevToolsAgentHost::AddAgentStateCallback(devtools_callback_);

  OnKeepaliveImpulseCheck();
}

ProcessManager::~ProcessManager() {
  extension_registry_->RemoveObserver(this);
  CloseBackgroundHosts();
  DCHECK(background_hosts_.empty());
  content::DevToolsAgentHost::RemoveAgentStateCallback(devtools_callback_);
}

void ProcessManager::RegisterRenderFrameHost(
    content::WebContents* web_contents,
    content::RenderFrameHost* render_frame_host,
    const Extension* extension) {
  ExtensionRenderFrameData* data = &all_extension_frames_[render_frame_host];
  data->view_type = GetViewType(web_contents);

  // Keep the lazy background page alive as long as any non-background-page
  // extension views are visible. Keepalive count balanced in
  // UnregisterRenderFrame.
  AcquireLazyKeepaliveCountForFrame(render_frame_host);

  FOR_EACH_OBSERVER(ProcessManagerObserver,
                    observer_list_,
                    OnExtensionFrameRegistered(extension->id(),
                                               render_frame_host));
}

void ProcessManager::UnregisterRenderFrameHost(
    content::RenderFrameHost* render_frame_host) {
  ExtensionRenderFrames::iterator frame =
      all_extension_frames_.find(render_frame_host);

  if (frame != all_extension_frames_.end()) {
    std::string extension_id = GetExtensionID(render_frame_host);
    // Keepalive count, balanced in RegisterRenderFrame.
    ReleaseLazyKeepaliveCountForFrame(render_frame_host);
    all_extension_frames_.erase(frame);

    FOR_EACH_OBSERVER(ProcessManagerObserver,
                      observer_list_,
                      OnExtensionFrameUnregistered(extension_id,
                                                   render_frame_host));
  }
}

void ProcessManager::DidNavigateRenderFrameHost(
    content::RenderFrameHost* render_frame_host) {
  ExtensionRenderFrames::iterator frame =
      all_extension_frames_.find(render_frame_host);

  if (frame != all_extension_frames_.end()) {
    std::string extension_id = GetExtensionID(render_frame_host);

    FOR_EACH_OBSERVER(ProcessManagerObserver,
                      observer_list_,
                      OnExtensionFrameNavigated(extension_id,
                                                render_frame_host));
  }
}

scoped_refptr<content::SiteInstance> ProcessManager::GetSiteInstanceForURL(
    const GURL& url) {
  return site_instance_->GetRelatedSiteInstance(url);
}

const ProcessManager::FrameSet ProcessManager::GetAllFrames() const {
  FrameSet result;
  for (const auto& key_value : all_extension_frames_)
    result.insert(key_value.first);
  return result;
}

ProcessManager::FrameSet ProcessManager::GetRenderFrameHostsForExtension(
    const std::string& extension_id) {
  FrameSet result;
  for (const auto& key_value : all_extension_frames_) {
    if (GetExtensionID(key_value.first) == extension_id)
      result.insert(key_value.first);
  }
  return result;
}

bool ProcessManager::IsRenderFrameHostRegistered(
    content::RenderFrameHost* render_frame_host) {
  return all_extension_frames_.find(render_frame_host) !=
         all_extension_frames_.end();
}

void ProcessManager::AddObserver(ProcessManagerObserver* observer) {
  observer_list_.AddObserver(observer);
}

void ProcessManager::RemoveObserver(ProcessManagerObserver* observer) {
  observer_list_.RemoveObserver(observer);
}

bool ProcessManager::CreateBackgroundHost(const Extension* extension,
                                          const GURL& url) {
  // Hosted apps are taken care of from BackgroundContentsService. Ignore them
  // here.
  if (extension->is_hosted_app())
    return false;

  // Don't create hosts if the embedder doesn't allow it.
  ProcessManagerDelegate* delegate =
      ExtensionsBrowserClient::Get()->GetProcessManagerDelegate();
  if (delegate && !delegate->IsBackgroundPageAllowed(browser_context_))
    return false;

  // Don't create multiple background hosts for an extension.
  if (GetBackgroundHostForExtension(extension->id()))
    return true;  // TODO(kalman): return false here? It might break things...

  ExtensionHost* host =
      new ExtensionHost(extension, GetSiteInstanceForURL(url).get(), url,
                        VIEW_TYPE_EXTENSION_BACKGROUND_PAGE);
  host->CreateRenderViewSoon();
  OnBackgroundHostCreated(host);
  return true;
}

void ProcessManager::MaybeCreateStartupBackgroundHosts() {
  if (startup_background_hosts_created_)
    return;

  // The embedder might disallow background pages entirely.
  ProcessManagerDelegate* delegate =
      ExtensionsBrowserClient::Get()->GetProcessManagerDelegate();
  if (delegate && !delegate->IsBackgroundPageAllowed(browser_context_))
    return;

  // The embedder might want to defer background page loading. For example,
  // Chrome defers background page loading when it is launched to show the app
  // list, then triggers a load later when a browser window opens.
  if (delegate &&
      delegate->DeferCreatingStartupBackgroundHosts(browser_context_))
    return;

  CreateStartupBackgroundHosts();
  startup_background_hosts_created_ = true;

  // Background pages should only be loaded once. To prevent any further loads
  // occurring, we remove the notification listeners.
  BrowserContext* original_context =
      ExtensionsBrowserClient::Get()->GetOriginalContext(browser_context_);
  if (registrar_.IsRegistered(
          this,
          extensions::NOTIFICATION_EXTENSIONS_READY_DEPRECATED,
          content::Source<BrowserContext>(original_context))) {
    registrar_.Remove(this,
                      extensions::NOTIFICATION_EXTENSIONS_READY_DEPRECATED,
                      content::Source<BrowserContext>(original_context));
  }
}

ExtensionHost* ProcessManager::GetBackgroundHostForExtension(
    const std::string& extension_id) {
  for (ExtensionHost* host : background_hosts_) {
    if (host->extension_id() == extension_id)
      return host;
  }
  return nullptr;
}

ExtensionHost* ProcessManager::GetExtensionHostForRenderFrameHost(
    content::RenderFrameHost* render_frame_host) {
  content::WebContents* web_contents =
      content::WebContents::FromRenderFrameHost(render_frame_host);
  for (ExtensionHost* extension_host : background_hosts_) {
    if (extension_host->host_contents() == web_contents)
      return extension_host;
  }
  return nullptr;
}

bool ProcessManager::IsEventPageSuspended(const std::string& extension_id) {
  return GetBackgroundHostForExtension(extension_id) == nullptr;
}

bool ProcessManager::WakeEventPage(const std::string& extension_id,
                                   const base::Callback<void(bool)>& callback) {
  if (GetBackgroundHostForExtension(extension_id)) {
    // Run the callback immediately if the extension is already awake.
    return false;
  }
  LazyBackgroundTaskQueue* queue =
      LazyBackgroundTaskQueue::Get(browser_context_);
  queue->AddPendingTask(browser_context_, extension_id,
                        base::Bind(&PropagateExtensionWakeResult, callback));
  return true;
}

bool ProcessManager::IsBackgroundHostClosing(const std::string& extension_id) {
  ExtensionHost* host = GetBackgroundHostForExtension(extension_id);
  return (host && background_page_data_[extension_id].is_closing);
}

const Extension* ProcessManager::GetExtensionForRenderFrameHost(
    content::RenderFrameHost* render_frame_host) {
  return extension_registry_->enabled_extensions().GetByID(
      GetExtensionID(render_frame_host));
}

const Extension* ProcessManager::GetExtensionForWebContents(
    const content::WebContents* web_contents) {
  if (!web_contents->GetSiteInstance())
    return nullptr;
  return extension_registry_->enabled_extensions().GetByID(
      GetExtensionIdForSiteInstance(web_contents->GetSiteInstance()));
}

int ProcessManager::GetLazyKeepaliveCount(const Extension* extension) {
  if (!BackgroundInfo::HasLazyBackgroundPage(extension))
    return 0;

  return background_page_data_[extension->id()].lazy_keepalive_count;
}

void ProcessManager::IncrementLazyKeepaliveCount(const Extension* extension) {
  if (BackgroundInfo::HasLazyBackgroundPage(extension)) {
    int& count = background_page_data_[extension->id()].lazy_keepalive_count;
    if (++count == 1)
      OnLazyBackgroundPageActive(extension->id());
  }
}

void ProcessManager::DecrementLazyKeepaliveCount(const Extension* extension) {
  if (BackgroundInfo::HasLazyBackgroundPage(extension))
    DecrementLazyKeepaliveCount(extension->id());
}

// This implementation layers on top of the keepalive count. An impulse sets
// a per extension flag. On a regular interval that flag is checked. Changes
// from the flag not being set to set cause an IncrementLazyKeepaliveCount.
void ProcessManager::KeepaliveImpulse(const Extension* extension) {
  if (!BackgroundInfo::HasLazyBackgroundPage(extension))
    return;

  BackgroundPageData& bd = background_page_data_[extension->id()];

  if (!bd.keepalive_impulse) {
    bd.keepalive_impulse = true;
    if (!bd.previous_keepalive_impulse) {
      IncrementLazyKeepaliveCount(extension);
    }
  }

  if (!keepalive_impulse_callback_for_testing_.is_null()) {
    ImpulseCallbackForTesting callback_may_clear_callbacks_reentrantly =
      keepalive_impulse_callback_for_testing_;
    callback_may_clear_callbacks_reentrantly.Run(extension->id());
  }
}

// static
void ProcessManager::OnKeepaliveFromPlugin(int render_process_id,
                                           int render_frame_id,
                                           const std::string& extension_id) {
  content::RenderFrameHost* render_frame_host =
      content::RenderFrameHost::FromID(render_process_id, render_frame_id);
  if (!render_frame_host)
    return;

  content::SiteInstance* site_instance = render_frame_host->GetSiteInstance();
  if (!site_instance)
    return;

  BrowserContext* browser_context = site_instance->GetBrowserContext();
  const Extension* extension =
      ExtensionRegistry::Get(browser_context)->enabled_extensions().GetByID(
          extension_id);
  if (!extension)
    return;

  ProcessManager::Get(browser_context)->KeepaliveImpulse(extension);
}

void ProcessManager::OnShouldSuspendAck(const std::string& extension_id,
                                        uint64_t sequence_id) {
  ExtensionHost* host = GetBackgroundHostForExtension(extension_id);
  if (host &&
      sequence_id == background_page_data_[extension_id].close_sequence_id) {
    host->render_process_host()->Send(new ExtensionMsg_Suspend(extension_id));
  }
}

void ProcessManager::OnSuspendAck(const std::string& extension_id) {
  background_page_data_[extension_id].is_closing = true;
  uint64_t sequence_id = background_page_data_[extension_id].close_sequence_id;
  base::ThreadTaskRunnerHandle::Get()->PostDelayedTask(
      FROM_HERE,
      base::Bind(&ProcessManager::CloseLazyBackgroundPageNow,
                 weak_ptr_factory_.GetWeakPtr(), extension_id, sequence_id),
      base::TimeDelta::FromMilliseconds(g_event_page_suspending_time_msec));
}

void ProcessManager::OnNetworkRequestStarted(
    content::RenderFrameHost* render_frame_host,
    uint64_t request_id) {
  ExtensionHost* host = GetBackgroundHostForExtension(
      GetExtensionID(render_frame_host));
  if (!host || !IsFrameInExtensionHost(host, render_frame_host))
    return;

  auto result =
      pending_network_requests_.insert(std::make_pair(request_id, host));
  DCHECK(result.second) << "Duplicate network request IDs.";

  IncrementLazyKeepaliveCount(host->extension());
  host->OnNetworkRequestStarted(request_id);
}

void ProcessManager::OnNetworkRequestDone(
    content::RenderFrameHost* render_frame_host,
    uint64_t request_id) {
  auto result = pending_network_requests_.find(request_id);
  if (result == pending_network_requests_.end())
    return;

  // The cached |host| can be invalid, if it was deleted between the time it
  // was inserted in the map and the look up. It is checked to ensure it is in
  // the list of existing background_hosts_.
  ExtensionHost* host = result->second;
  pending_network_requests_.erase(result);

  if (background_hosts_.find(host) == background_hosts_.end())
    return;

  DCHECK(IsFrameInExtensionHost(host, render_frame_host));

  host->OnNetworkRequestDone(request_id);
  DecrementLazyKeepaliveCount(host->extension());
}

void ProcessManager::CancelSuspend(const Extension* extension) {
  bool& is_closing = background_page_data_[extension->id()].is_closing;
  ExtensionHost* host = GetBackgroundHostForExtension(extension->id());
  if (host && is_closing) {
    is_closing = false;
    host->render_process_host()->Send(
        new ExtensionMsg_CancelSuspend(extension->id()));
    // This increment / decrement is to simulate an instantaneous event. This
    // has the effect of invalidating close_sequence_id, preventing any in
    // progress closes from completing and starting a new close process if
    // necessary.
    IncrementLazyKeepaliveCount(extension);
    DecrementLazyKeepaliveCount(extension);
  }
}

void ProcessManager::CloseBackgroundHosts() {
  STLDeleteElements(&background_hosts_);
}

void ProcessManager::SetKeepaliveImpulseCallbackForTesting(
    const ImpulseCallbackForTesting& callback) {
  keepalive_impulse_callback_for_testing_ = callback;
}

void ProcessManager::SetKeepaliveImpulseDecrementCallbackForTesting(
    const ImpulseCallbackForTesting& callback) {
  keepalive_impulse_decrement_callback_for_testing_ = callback;
}

// static
void ProcessManager::SetEventPageIdleTimeForTesting(unsigned idle_time_msec) {
  CHECK_GT(idle_time_msec, 0u);  // OnKeepaliveImpulseCheck requires non zero.
  g_event_page_idle_time_msec = idle_time_msec;
}

// static
void ProcessManager::SetEventPageSuspendingTimeForTesting(
    unsigned suspending_time_msec) {
  g_event_page_suspending_time_msec = suspending_time_msec;
}

////////////////////////////////////////////////////////////////////////////////
// Private

void ProcessManager::Observe(int type,
                             const content::NotificationSource& source,
                             const content::NotificationDetails& details) {
  TRACE_EVENT0("browser,startup", "ProcessManager::Observe");
  switch (type) {
    case extensions::NOTIFICATION_EXTENSIONS_READY_DEPRECATED: {
      // TODO(jamescook): Convert this to use ExtensionSystem::ready() instead
      // of a notification.
      SCOPED_UMA_HISTOGRAM_TIMER("Extensions.ProcessManagerStartupHostsTime");
      MaybeCreateStartupBackgroundHosts();
      break;
    }
    case extensions::NOTIFICATION_EXTENSION_HOST_DESTROYED: {
      ExtensionHost* host = content::Details<ExtensionHost>(details).ptr();
      if (background_hosts_.erase(host)) {
        ClearBackgroundPageData(host->extension()->id());
        background_page_data_[host->extension()->id()].since_suspended.reset(
            new base::ElapsedTimer());
      }
      break;
    }
    case extensions::NOTIFICATION_EXTENSION_HOST_VIEW_SHOULD_CLOSE: {
      ExtensionHost* host = content::Details<ExtensionHost>(details).ptr();
      if (host->extension_host_type() == VIEW_TYPE_EXTENSION_BACKGROUND_PAGE) {
        CloseBackgroundHost(host);
      }
      break;
    }
    default:
      NOTREACHED();
  }
}

void ProcessManager::OnExtensionLoaded(BrowserContext* browser_context,
                                       const Extension* extension) {
  if (ExtensionSystem::Get(browser_context)->ready().is_signaled()) {
    // The extension system is ready, so create the background host.
    CreateBackgroundHostForExtensionLoad(this, extension);
  }
}

void ProcessManager::OnExtensionUnloaded(
    BrowserContext* browser_context,
    const Extension* extension,
    UnloadedExtensionInfo::Reason reason) {
  ExtensionHost* host = GetBackgroundHostForExtension(extension->id());
  if (host != nullptr)
    CloseBackgroundHost(host);
  UnregisterExtension(extension->id());
}

void ProcessManager::CreateStartupBackgroundHosts() {
  DCHECK(!startup_background_hosts_created_);
  for (const scoped_refptr<const Extension>& extension :
           extension_registry_->enabled_extensions()) {
    CreateBackgroundHostForExtensionLoad(this, extension.get());
    FOR_EACH_OBSERVER(ProcessManagerObserver,
                      observer_list_,
                      OnBackgroundHostStartup(extension.get()));
  }
}

void ProcessManager::OnBackgroundHostCreated(ExtensionHost* host) {
  DCHECK_EQ(browser_context_, host->browser_context());
  background_hosts_.insert(host);

  if (BackgroundInfo::HasLazyBackgroundPage(host->extension())) {
    linked_ptr<base::ElapsedTimer> since_suspended(
        background_page_data_[host->extension()->id()].
            since_suspended.release());
    if (since_suspended.get()) {
      UMA_HISTOGRAM_LONG_TIMES("Extensions.EventPageIdleTime",
                               since_suspended->Elapsed());
    }
  }
  FOR_EACH_OBSERVER(ProcessManagerObserver, observer_list_,
                    OnBackgroundHostCreated(host));
}

void ProcessManager::CloseBackgroundHost(ExtensionHost* host) {
  ExtensionId extension_id = host->extension_id();
  CHECK(host->extension_host_type() == VIEW_TYPE_EXTENSION_BACKGROUND_PAGE);
  delete host;
  // |host| should deregister itself from our structures.
  CHECK(background_hosts_.find(host) == background_hosts_.end());

  FOR_EACH_OBSERVER(ProcessManagerObserver,
                    observer_list_,
                    OnBackgroundHostClose(extension_id));
}

void ProcessManager::AcquireLazyKeepaliveCountForFrame(
    content::RenderFrameHost* render_frame_host) {
  ExtensionRenderFrames::iterator it =
      all_extension_frames_.find(render_frame_host);
  if (it == all_extension_frames_.end())
    return;

  ExtensionRenderFrameData& data = it->second;
  if (data.CanKeepalive() && !data.has_keepalive) {
    const Extension* extension =
        GetExtensionForRenderFrameHost(render_frame_host);
    if (extension) {
      IncrementLazyKeepaliveCount(extension);
      data.has_keepalive = true;
    }
  }
}

void ProcessManager::ReleaseLazyKeepaliveCountForFrame(
    content::RenderFrameHost* render_frame_host) {
  ExtensionRenderFrames::iterator iter =
      all_extension_frames_.find(render_frame_host);
  if (iter == all_extension_frames_.end())
    return;

  ExtensionRenderFrameData& data = iter->second;
  if (data.CanKeepalive() && data.has_keepalive) {
    const Extension* extension =
        GetExtensionForRenderFrameHost(render_frame_host);
    if (extension) {
      DecrementLazyKeepaliveCount(extension);
      data.has_keepalive = false;
    }
  }
}

void ProcessManager::DecrementLazyKeepaliveCount(
    const std::string& extension_id) {
  int& count = background_page_data_[extension_id].lazy_keepalive_count;
  DCHECK(count > 0 ||
         !extension_registry_->enabled_extensions().Contains(extension_id));

  // If we reach a zero keepalive count when the lazy background page is about
  // to be closed, incrementing close_sequence_id will cancel the close
  // sequence and cause the background page to linger. So check is_closing
  // before initiating another close sequence.
  if (--count == 0 && !background_page_data_[extension_id].is_closing) {
    background_page_data_[extension_id].close_sequence_id =
        ++last_background_close_sequence_id_;
    base::ThreadTaskRunnerHandle::Get()->PostDelayedTask(
        FROM_HERE, base::Bind(&ProcessManager::OnLazyBackgroundPageIdle,
                              weak_ptr_factory_.GetWeakPtr(), extension_id,
                              last_background_close_sequence_id_),
        base::TimeDelta::FromMilliseconds(g_event_page_idle_time_msec));
  }
}

// DecrementLazyKeepaliveCount is called when no calls to KeepaliveImpulse
// have been made for at least g_event_page_idle_time_msec. In the best case an
// impulse was made just before being cleared, and the decrement will occur
// g_event_page_idle_time_msec later, causing a 2 * g_event_page_idle_time_msec
// total time for extension to be shut down based on impulses. Worst case is
// an impulse just after a clear, adding one check cycle and resulting in 3x
// total time.
void ProcessManager::OnKeepaliveImpulseCheck() {
  for (BackgroundPageDataMap::iterator i = background_page_data_.begin();
       i != background_page_data_.end();
       ++i) {
    if (i->second.previous_keepalive_impulse && !i->second.keepalive_impulse) {
      DecrementLazyKeepaliveCount(i->first);
      if (!keepalive_impulse_decrement_callback_for_testing_.is_null()) {
        ImpulseCallbackForTesting callback_may_clear_callbacks_reentrantly =
            keepalive_impulse_decrement_callback_for_testing_;
        callback_may_clear_callbacks_reentrantly.Run(i->first);
      }
    }

    i->second.previous_keepalive_impulse = i->second.keepalive_impulse;
    i->second.keepalive_impulse = false;
  }

  // OnKeepaliveImpulseCheck() is always called in constructor, but in unit
  // tests there will be no thread task runner handle. In that event don't
  // schedule tasks.
  if (base::ThreadTaskRunnerHandle::IsSet()) {
    base::ThreadTaskRunnerHandle::Get()->PostDelayedTask(
        FROM_HERE, base::Bind(&ProcessManager::OnKeepaliveImpulseCheck,
                              weak_ptr_factory_.GetWeakPtr()),
        base::TimeDelta::FromMilliseconds(g_event_page_idle_time_msec));
  }
}

void ProcessManager::OnLazyBackgroundPageIdle(const std::string& extension_id,
                                              uint64_t sequence_id) {
  ExtensionHost* host = GetBackgroundHostForExtension(extension_id);
  if (host && !background_page_data_[extension_id].is_closing &&
      sequence_id == background_page_data_[extension_id].close_sequence_id) {
    // Tell the renderer we are about to close. This is a simple ping that the
    // renderer will respond to. The purpose is to control sequencing: if the
    // extension remains idle until the renderer responds with an ACK, then we
    // know that the extension process is ready to shut down. If our
    // close_sequence_id has already changed, then we would ignore the
    // ShouldSuspendAck, so we don't send the ping.
    host->render_process_host()->Send(new ExtensionMsg_ShouldSuspend(
        extension_id, sequence_id));
  }
}

void ProcessManager::OnLazyBackgroundPageActive(
    const std::string& extension_id) {
  if (!background_page_data_[extension_id].is_closing) {
    // Cancel the current close sequence by changing the close_sequence_id,
    // which causes us to ignore the next ShouldSuspendAck.
    background_page_data_[extension_id].close_sequence_id =
        ++last_background_close_sequence_id_;
  }
}

void ProcessManager::CloseLazyBackgroundPageNow(const std::string& extension_id,
                                                uint64_t sequence_id) {
  ExtensionHost* host = GetBackgroundHostForExtension(extension_id);
  if (host &&
      sequence_id == background_page_data_[extension_id].close_sequence_id) {
    // Handle the case where the keepalive count was increased after the
    // OnSuspend event was sent.
    if (background_page_data_[extension_id].lazy_keepalive_count > 0) {
      CancelSuspend(host->extension());
      return;
    }

    // Close remaining views.
    std::vector<content::RenderFrameHost*> frames_to_close;
    for (const auto& key_value : all_extension_frames_) {
      if (key_value.second.CanKeepalive() &&
          GetExtensionID(key_value.first) == extension_id) {
        DCHECK(!key_value.second.has_keepalive);
        frames_to_close.push_back(key_value.first);
      }
    }
    for (content::RenderFrameHost* frame : frames_to_close) {
      content::WebContents::FromRenderFrameHost(frame)->ClosePage();
      // WebContents::ClosePage() may result in calling
      // UnregisterRenderViewHost() asynchronously and may cause race conditions
      // when the background page is reloaded.
      // To avoid this, unregister the view now.
      UnregisterRenderFrameHost(frame);
    }

    ExtensionHost* host = GetBackgroundHostForExtension(extension_id);
    if (host)
      CloseBackgroundHost(host);
  }
}

void ProcessManager::OnDevToolsStateChanged(
    content::DevToolsAgentHost* agent_host,
    bool attached) {
  content::WebContents* web_contents = agent_host->GetWebContents();
  // Ignore unrelated notifications.
  if (!web_contents || web_contents->GetBrowserContext() != browser_context_)
    return;
  if (GetViewType(web_contents) != VIEW_TYPE_EXTENSION_BACKGROUND_PAGE)
    return;
  const Extension* extension =
      extension_registry_->enabled_extensions().GetByID(
          GetExtensionIdForSiteInstance(web_contents->GetSiteInstance()));
  if (!extension)
    return;
  if (attached) {
    // Keep the lazy background page alive while it's being inspected.
    CancelSuspend(extension);
    IncrementLazyKeepaliveCount(extension);
  } else {
    DecrementLazyKeepaliveCount(extension);
  }
}

void ProcessManager::UnregisterExtension(const std::string& extension_id) {
  // The lazy_keepalive_count may be greater than zero at this point because
  // RenderFrameHosts are still alive. During extension reloading, they will
  // decrement the lazy_keepalive_count to negative for the new extension
  // instance when they are destroyed. Since we are erasing the background page
  // data for the unloaded extension, unregister the RenderFrameHosts too.
  for (ExtensionRenderFrames::iterator it = all_extension_frames_.begin();
       it != all_extension_frames_.end(); ) {
    content::RenderFrameHost* host = it->first;
    if (GetExtensionID(host) == extension_id) {
      all_extension_frames_.erase(it++);
      FOR_EACH_OBSERVER(ProcessManagerObserver,
                        observer_list_,
                        OnExtensionFrameUnregistered(extension_id, host));
    } else {
      ++it;
    }
  }

  background_page_data_.erase(extension_id);
}

void ProcessManager::ClearBackgroundPageData(const std::string& extension_id) {
  background_page_data_.erase(extension_id);

  // Re-register all RenderViews for this extension. We do this to restore
  // the lazy_keepalive_count (if any) to properly reflect the number of open
  // views.
  for (const auto& key_value : all_extension_frames_) {
    // Do not increment the count when |has_keepalive| is false
    // (i.e. ReleaseLazyKeepaliveCountForView() was called).
    if (GetExtensionID(key_value.first) == extension_id &&
        key_value.second.has_keepalive) {
      const Extension* extension =
          GetExtensionForRenderFrameHost(key_value.first);
      if (extension)
        IncrementLazyKeepaliveCount(extension);
    }
  }
}

//
// IncognitoProcessManager
//

IncognitoProcessManager::IncognitoProcessManager(
    BrowserContext* incognito_context,
    BrowserContext* original_context,
    ExtensionRegistry* extension_registry)
    : ProcessManager(incognito_context, original_context, extension_registry) {
  DCHECK(incognito_context->IsOffTheRecord());
}

bool IncognitoProcessManager::CreateBackgroundHost(const Extension* extension,
                                                   const GURL& url) {
  if (IncognitoInfo::IsSplitMode(extension)) {
    if (ExtensionsBrowserClient::Get()->IsExtensionIncognitoEnabled(
            extension->id(), browser_context()))
      return ProcessManager::CreateBackgroundHost(extension, url);
  } else {
    // Do nothing. If an extension is spanning, then its original-profile
    // background page is shared with incognito, so we don't create another.
  }
  return false;
}

scoped_refptr<content::SiteInstance>
IncognitoProcessManager::GetSiteInstanceForURL(const GURL& url) {
  const Extension* extension =
      extension_registry_->enabled_extensions().GetExtensionOrAppByURL(url);
  if (extension && !IncognitoInfo::IsSplitMode(extension)) {
    BrowserContext* original_context =
        ExtensionsBrowserClient::Get()->GetOriginalContext(browser_context());
    return ProcessManager::Get(original_context)->GetSiteInstanceForURL(url);
  }

  return ProcessManager::GetSiteInstanceForURL(url);
}

}  // namespace extensions
