// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "extensions/browser/extension_web_contents_observer.h"

#include "content/public/browser/child_process_security_policy.h"
#include "content/public/browser/navigation_details.h"
#include "content/public/browser/render_frame_host.h"
#include "content/public/browser/render_process_host.h"
#include "content/public/browser/render_view_host.h"
#include "content/public/browser/site_instance.h"
#include "content/public/browser/web_contents.h"
#include "content/public/common/url_constants.h"
#include "extensions/browser/extension_api_frame_id_map.h"
#include "extensions/browser/extension_prefs.h"
#include "extensions/browser/extension_registry.h"
#include "extensions/browser/extensions_browser_client.h"
#include "extensions/browser/mojo/service_registration.h"
#include "extensions/browser/process_manager.h"
#include "extensions/browser/renderer_startup_helper.h"
#include "extensions/browser/view_type_utils.h"
#include "extensions/common/constants.h"
#include "extensions/common/extension.h"
#include "extensions/common/extension_messages.h"
#include "extensions/common/view_type.h"

namespace extensions {

// static
ExtensionWebContentsObserver* ExtensionWebContentsObserver::GetForWebContents(
    content::WebContents* web_contents) {
  return ExtensionsBrowserClient::Get()->GetExtensionWebContentsObserver(
      web_contents);
}

ExtensionWebContentsObserver::ExtensionWebContentsObserver(
    content::WebContents* web_contents)
    : content::WebContentsObserver(web_contents),
      browser_context_(web_contents->GetBrowserContext()),
      dispatcher_(browser_context_) {
  web_contents->ForEachFrame(
      base::Bind(&ExtensionWebContentsObserver::InitializeFrameHelper,
                 base::Unretained(this)));
  dispatcher_.set_delegate(this);
}

ExtensionWebContentsObserver::~ExtensionWebContentsObserver() {
}

void ExtensionWebContentsObserver::InitializeRenderFrame(
    content::RenderFrameHost* render_frame_host) {
  DCHECK(render_frame_host);
  DCHECK(render_frame_host->IsRenderFrameLive());

  // At the initialization of the render frame, the last committed URL is not
  // reliable, so do not take it into account in determining whether it is an
  // extension frame.
  const Extension* frame_extension =
      GetExtensionFromFrame(render_frame_host, false);
  // This observer is attached to every WebContents, so we are also notified of
  // frames that are not in an extension process.
  if (!frame_extension)
    return;

  // |render_frame_host->GetProcess()| is an extension process. Grant permission
  // to commit pages from chrome-extension:// origins.
  content::ChildProcessSecurityPolicy* security_policy =
      content::ChildProcessSecurityPolicy::GetInstance();
  int process_id = render_frame_host->GetProcess()->GetID();
  security_policy->GrantScheme(process_id, extensions::kExtensionScheme);
  security_policy->GrantScheme(process_id,
                               extensions::kExtensionResourceScheme);

  // Notify the render frame of the view type.
  render_frame_host->Send(new ExtensionMsg_NotifyRenderViewType(
      render_frame_host->GetRoutingID(), GetViewType(web_contents())));

  ExtensionsBrowserClient::Get()->RegisterMojoServices(render_frame_host,
                                                       frame_extension);
  ProcessManager::Get(browser_context_)
      ->RegisterRenderFrameHost(web_contents(), render_frame_host,
                                frame_extension);
}

content::WebContents* ExtensionWebContentsObserver::GetAssociatedWebContents()
    const {
  return web_contents();
}

void ExtensionWebContentsObserver::RenderViewCreated(
    content::RenderViewHost* render_view_host) {
  // TODO(devlin): Most/all of this should move to RenderFrameCreated.
  const Extension* extension = GetExtension(render_view_host);
  if (!extension)
    return;

  Manifest::Type type = extension->GetType();

  // Some extensions use file:// URLs.
  if (type == Manifest::TYPE_EXTENSION ||
      type == Manifest::TYPE_LEGACY_PACKAGED_APP) {
    ExtensionPrefs* prefs = ExtensionPrefs::Get(browser_context_);
    if (prefs->AllowFileAccess(extension->id())) {
      content::ChildProcessSecurityPolicy::GetInstance()->GrantScheme(
          render_view_host->GetProcess()->GetID(), url::kFileScheme);
    }
  }

  // Tells the new view that it's hosted in an extension process.
  //
  // This will often be a rendant IPC, because activating extensions happens at
  // the process level, not at the view level. However, without some mild
  // refactoring this isn't trivial to do, and this way is simpler.
  //
  // Plus, we can delete the concept of activating an extension once site
  // isolation is turned on.
  RendererStartupHelperFactory::GetForBrowserContext(browser_context_)
      ->ActivateExtensionInProcess(*extension, render_view_host->GetProcess());
}

void ExtensionWebContentsObserver::RenderFrameCreated(
    content::RenderFrameHost* render_frame_host) {
  InitializeRenderFrame(render_frame_host);

  // Optimization: Look up the extension API frame ID to force the mapping to be
  // cached. This minimizes the number of IO->UI->IO thread hops when the ID is
  // looked up again on the IO thread for the webRequest API.
  ExtensionApiFrameIdMap::Get()->CacheFrameData(render_frame_host);
}

void ExtensionWebContentsObserver::RenderFrameDeleted(
    content::RenderFrameHost* render_frame_host) {
  ProcessManager::Get(browser_context_)
      ->UnregisterRenderFrameHost(render_frame_host);
  ExtensionApiFrameIdMap::Get()->RemoveFrameData(render_frame_host);
}

void ExtensionWebContentsObserver::DidCommitProvisionalLoadForFrame(
    content::RenderFrameHost* render_frame_host,
    const GURL& url,
    ui::PageTransition transition_type) {
  ProcessManager* pm = ProcessManager::Get(browser_context_);

  if (pm->IsRenderFrameHostRegistered(render_frame_host)) {
    const Extension* frame_extension =
        GetExtensionFromFrame(render_frame_host, true);

    if (!frame_extension)
      pm->UnregisterRenderFrameHost(render_frame_host);
  }
}

void ExtensionWebContentsObserver::DidNavigateAnyFrame(
    content::RenderFrameHost* render_frame_host,
    const content::LoadCommittedDetails& details,
    const content::FrameNavigateParams& params) {
  if (details.is_in_page)
    return;

  const Extension* frame_extension =
      GetExtensionFromFrame(render_frame_host, true);
  ProcessManager* pm = ProcessManager::Get(browser_context_);

  if (!frame_extension) {
    // Should have been unregistered by DidCommitProvisionalLoadForFrame.
    DCHECK(!pm->IsRenderFrameHostRegistered(render_frame_host));
    return;
  }

  if (pm->IsRenderFrameHostRegistered(render_frame_host)) {
    // Notify ProcessManager, because some clients do not just want to know
    // whether the frame is in an extension process, but also whether the frame
    // was navigated.
    pm->DidNavigateRenderFrameHost(render_frame_host);
  } else {
    pm->RegisterRenderFrameHost(web_contents(), render_frame_host,
                                frame_extension);
  }
}

bool ExtensionWebContentsObserver::OnMessageReceived(
    const IPC::Message& message,
    content::RenderFrameHost* render_frame_host) {
  bool handled = true;
  IPC_BEGIN_MESSAGE_MAP_WITH_PARAM(
      ExtensionWebContentsObserver, message, render_frame_host)
    IPC_MESSAGE_HANDLER(ExtensionHostMsg_Request, OnRequest)
    IPC_MESSAGE_UNHANDLED(handled = false)
  IPC_END_MESSAGE_MAP()
  return handled;
}

void ExtensionWebContentsObserver::PepperInstanceCreated() {
  if (GetViewType(web_contents()) == VIEW_TYPE_EXTENSION_BACKGROUND_PAGE) {
    ProcessManager* const process_manager =
        ProcessManager::Get(browser_context_);
    const Extension* const extension =
        process_manager->GetExtensionForWebContents(web_contents());
    if (extension)
      process_manager->IncrementLazyKeepaliveCount(extension);
  }
}

void ExtensionWebContentsObserver::PepperInstanceDeleted() {
  if (GetViewType(web_contents()) == VIEW_TYPE_EXTENSION_BACKGROUND_PAGE) {
    ProcessManager* const process_manager =
        ProcessManager::Get(browser_context_);
    const Extension* const extension =
        process_manager->GetExtensionForWebContents(web_contents());
    if (extension)
      process_manager->DecrementLazyKeepaliveCount(extension);
  }
}

std::string ExtensionWebContentsObserver::GetExtensionIdFromFrame(
    content::RenderFrameHost* render_frame_host) const {
  // The second argument is false because |render_frame_host| need not be an
  // active RenderFrameHost (crbug.com/567277).
  // TODO(robwu): If there is a method to check whether |render_frame_host| is
  // an active host, use it.
  const Extension* extension = GetExtensionFromFrame(render_frame_host, false);
  return extension ? extension->id() : std::string();
}

const Extension* ExtensionWebContentsObserver::GetExtensionFromFrame(
    content::RenderFrameHost* render_frame_host,
    bool verify_url) const {
  const GURL site_url(render_frame_host->GetSiteInstance()->GetSiteURL());
  if (!site_url.SchemeIs(kExtensionScheme))
    return nullptr;

  const std::string& extension_id = site_url.host();
  content::BrowserContext* browser_context =
      render_frame_host->GetProcess()->GetBrowserContext();
  const Extension* extension = ExtensionRegistry::Get(browser_context)
                                   ->enabled_extensions()
                                   .GetByID(extension_id);
  if (!extension)
    return nullptr;

  if (verify_url) {
    const url::Origin& origin(render_frame_host->GetLastCommittedOrigin());
    // Without site isolation, this check is needed to eliminate non-extension
    // schemes. With site isolation, this is still needed to exclude sandboxed
    // extension frames with a unique origin.
    if (origin.unique() ||
        site_url != content::SiteInstance::GetSiteForURL(
                        browser_context, GURL(origin.Serialize())))
      return nullptr;
  }

  return extension;
}

const Extension* ExtensionWebContentsObserver::GetExtension(
    content::RenderViewHost* render_view_host) {
  std::string extension_id = GetExtensionId(render_view_host);
  if (extension_id.empty())
    return NULL;

  // May be null if the extension doesn't exist, for example if somebody typos
  // a chrome-extension:// URL.
  return ExtensionRegistry::Get(browser_context_)
      ->GetExtensionById(extension_id, ExtensionRegistry::ENABLED);
}

// static
std::string ExtensionWebContentsObserver::GetExtensionId(
    content::RenderViewHost* render_view_host) {
  // Note that due to ChromeContentBrowserClient::GetEffectiveURL(), hosted apps
  // (excluding bookmark apps) will have a chrome-extension:// URL for their
  // site, so we can ignore that wrinkle here.
  const GURL& site = render_view_host->GetSiteInstance()->GetSiteURL();

  if (!site.SchemeIs(kExtensionScheme))
    return std::string();

  return site.host();
}

void ExtensionWebContentsObserver::OnRequest(
    content::RenderFrameHost* render_frame_host,
    const ExtensionHostMsg_Request_Params& params) {
  dispatcher_.Dispatch(params, render_frame_host,
                       render_frame_host->GetProcess()->GetID());
}

void ExtensionWebContentsObserver::InitializeFrameHelper(
    content::RenderFrameHost* render_frame_host) {
  // Since this is called for all existing RenderFrameHosts during the
  // ExtensionWebContentsObserver's creation, it's possible that not all hosts
  // are ready.
  // We only initialize the frame if the renderer counterpart is live; otherwise
  // we wait for the RenderFrameCreated notification.
  if (render_frame_host->IsRenderFrameLive())
    InitializeRenderFrame(render_frame_host);
}

}  // namespace extensions
