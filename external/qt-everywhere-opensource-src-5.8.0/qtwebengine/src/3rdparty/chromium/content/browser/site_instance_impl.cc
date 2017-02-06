// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/browser/site_instance_impl.h"

#include "content/browser/browsing_instance.h"
#include "content/browser/child_process_security_policy_impl.h"
#include "content/browser/frame_host/debug_urls.h"
#include "content/browser/frame_host/frame_tree_node.h"
#include "content/browser/renderer_host/render_process_host_impl.h"
#include "content/browser/storage_partition_impl.h"
#include "content/common/site_isolation_policy.h"
#include "content/public/browser/content_browser_client.h"
#include "content/public/browser/render_process_host_factory.h"
#include "content/public/browser/web_ui_controller_factory.h"
#include "content/public/common/url_constants.h"
#include "net/base/registry_controlled_domains/registry_controlled_domain.h"

namespace content {

const RenderProcessHostFactory*
    SiteInstanceImpl::g_render_process_host_factory_ = NULL;
int32_t SiteInstanceImpl::next_site_instance_id_ = 1;

SiteInstanceImpl::SiteInstanceImpl(BrowsingInstance* browsing_instance)
    : id_(next_site_instance_id_++),
      active_frame_count_(0),
      browsing_instance_(browsing_instance),
      process_(nullptr),
      has_site_(false),
      is_default_subframe_site_instance_(false) {
  DCHECK(browsing_instance);
}

SiteInstanceImpl::~SiteInstanceImpl() {
  GetContentClient()->browser()->SiteInstanceDeleting(this);

  if (process_)
    process_->RemoveObserver(this);

  // Now that no one is referencing us, we can safely remove ourselves from
  // the BrowsingInstance.  Any future visits to a page from this site
  // (within the same BrowsingInstance) can safely create a new SiteInstance.
  if (has_site_)
    browsing_instance_->UnregisterSiteInstance(this);
}

scoped_refptr<SiteInstanceImpl> SiteInstanceImpl::Create(
    BrowserContext* browser_context) {
  return make_scoped_refptr(
      new SiteInstanceImpl(new BrowsingInstance(browser_context)));
}

scoped_refptr<SiteInstanceImpl> SiteInstanceImpl::CreateForURL(
    BrowserContext* browser_context,
    const GURL& url) {
  // This will create a new SiteInstance and BrowsingInstance.
  scoped_refptr<BrowsingInstance> instance(
      new BrowsingInstance(browser_context));
  return instance->GetSiteInstanceForURL(url);
}

int32_t SiteInstanceImpl::GetId() {
  return id_;
}

bool SiteInstanceImpl::HasProcess() const {
  if (process_ != NULL)
    return true;

  // If we would use process-per-site for this site, also check if there is an
  // existing process that we would use if GetProcess() were called.
  BrowserContext* browser_context =
      browsing_instance_->browser_context();
  if (has_site_ &&
      RenderProcessHost::ShouldUseProcessPerSite(browser_context, site_) &&
      RenderProcessHostImpl::GetProcessHostForSite(browser_context, site_)) {
    return true;
  }

  return false;
}

RenderProcessHost* SiteInstanceImpl::GetProcess() {
  // TODO(erikkay) It would be nice to ensure that the renderer type had been
  // properly set before we get here.  The default tab creation case winds up
  // with no site set at this point, so it will default to TYPE_NORMAL.  This
  // may not be correct, so we'll wind up potentially creating a process that
  // we then throw away, or worse sharing a process with the wrong process type.
  // See crbug.com/43448.

  // Create a new process if ours went away or was reused.
  if (!process_) {
    BrowserContext* browser_context = browsing_instance_->browser_context();

    // If we should use process-per-site mode (either in general or for the
    // given site), then look for an existing RenderProcessHost for the site.
    bool use_process_per_site = has_site_ &&
        RenderProcessHost::ShouldUseProcessPerSite(browser_context, site_);
    if (use_process_per_site) {
      process_ = RenderProcessHostImpl::GetProcessHostForSite(browser_context,
                                                              site_);
    }

    // If not (or if none found), see if we should reuse an existing process.
    if (!process_ && RenderProcessHostImpl::ShouldTryToUseExistingProcessHost(
            browser_context, site_)) {
      process_ = RenderProcessHostImpl::GetExistingProcessHost(browser_context,
                                                               site_);
    }

    // Otherwise (or if that fails), create a new one.
    if (!process_) {
      if (g_render_process_host_factory_) {
        process_ = g_render_process_host_factory_->CreateRenderProcessHost(
            browser_context, this);
      } else {
        StoragePartitionImpl* partition =
            static_cast<StoragePartitionImpl*>(
                BrowserContext::GetStoragePartition(browser_context, this));
        process_ = new RenderProcessHostImpl(browser_context,
                                             partition,
                                             site_.SchemeIs(kGuestScheme));
      }
    }
    CHECK(process_);
    process_->AddObserver(this);

    // If we are using process-per-site, we need to register this process
    // for the current site so that we can find it again.  (If no site is set
    // at this time, we will register it in SetSite().)
    if (use_process_per_site) {
      RenderProcessHostImpl::RegisterProcessHostForSite(browser_context,
                                                        process_, site_);
    }

    TRACE_EVENT2("navigation", "SiteInstanceImpl::GetProcess",
                 "site id", id_, "process id", process_->GetID());
    GetContentClient()->browser()->SiteInstanceGotProcess(this);

    if (has_site_)
      LockToOrigin();
  }
  DCHECK(process_);

  return process_;
}

void SiteInstanceImpl::SetSite(const GURL& url) {
  TRACE_EVENT2("navigation", "SiteInstanceImpl::SetSite",
               "site id", id_, "url", url.possibly_invalid_spec());
  // A SiteInstance's site should not change.
  // TODO(creis): When following links or script navigations, we can currently
  // render pages from other sites in this SiteInstance.  This will eventually
  // be fixed, but until then, we should still not set the site of a
  // SiteInstance more than once.
  DCHECK(!has_site_);

  // Remember that this SiteInstance has been used to load a URL, even if the
  // URL is invalid.
  has_site_ = true;
  BrowserContext* browser_context = browsing_instance_->browser_context();
  site_ = GetSiteForURL(browser_context, url);

  // Now that we have a site, register it with the BrowsingInstance.  This
  // ensures that we won't create another SiteInstance for this site within
  // the same BrowsingInstance, because all same-site pages within a
  // BrowsingInstance can script each other.
  browsing_instance_->RegisterSiteInstance(this);

  if (process_) {
    LockToOrigin();

    // Ensure the process is registered for this site if necessary.
    if (RenderProcessHost::ShouldUseProcessPerSite(browser_context, site_)) {
      RenderProcessHostImpl::RegisterProcessHostForSite(
          browser_context, process_, site_);
    }
  }
}

const GURL& SiteInstanceImpl::GetSiteURL() const {
  return site_;
}

bool SiteInstanceImpl::HasSite() const {
  return has_site_;
}

bool SiteInstanceImpl::HasRelatedSiteInstance(const GURL& url) {
  return browsing_instance_->HasSiteInstance(url);
}

scoped_refptr<SiteInstance> SiteInstanceImpl::GetRelatedSiteInstance(
    const GURL& url) {
  return browsing_instance_->GetSiteInstanceForURL(url);
}

bool SiteInstanceImpl::IsRelatedSiteInstance(const SiteInstance* instance) {
  return browsing_instance_.get() == static_cast<const SiteInstanceImpl*>(
                                         instance)->browsing_instance_.get();
}

size_t SiteInstanceImpl::GetRelatedActiveContentsCount() {
  return browsing_instance_->active_contents_count();
}

bool SiteInstanceImpl::HasWrongProcessForURL(const GURL& url) {
  // Having no process isn't a problem, since we'll assign it correctly.
  // Note that HasProcess() may return true if process_ is null, in
  // process-per-site cases where there's an existing process available.
  // We want to use such a process in the IsSuitableHost check, so we
  // may end up assigning process_ in the GetProcess() call below.
  if (!HasProcess())
    return false;

  // If the URL to navigate to can be associated with any site instance,
  // we want to keep it in the same process.
  if (IsRendererDebugURL(url))
    return false;

  // If the site URL is an extension (e.g., for hosted apps or WebUI) but the
  // process is not (or vice versa), make sure we notice and fix it.
  GURL site_url = GetSiteForURL(browsing_instance_->browser_context(), url);
  return !RenderProcessHostImpl::IsSuitableHost(
      GetProcess(), browsing_instance_->browser_context(), site_url);
}

scoped_refptr<SiteInstanceImpl>
SiteInstanceImpl::GetDefaultSubframeSiteInstance() {
  return browsing_instance_->GetDefaultSubframeSiteInstance();
}

bool SiteInstanceImpl::RequiresDedicatedProcess() {
  if (!has_site_)
    return false;

  return DoesSiteRequireDedicatedProcess(GetBrowserContext(), site_);
}

void SiteInstanceImpl::IncrementActiveFrameCount() {
  active_frame_count_++;
}

void SiteInstanceImpl::DecrementActiveFrameCount() {
  if (--active_frame_count_ == 0)
    FOR_EACH_OBSERVER(Observer, observers_, ActiveFrameCountIsZero(this));
}

void SiteInstanceImpl::IncrementRelatedActiveContentsCount() {
  browsing_instance_->increment_active_contents_count();
}

void SiteInstanceImpl::DecrementRelatedActiveContentsCount() {
  browsing_instance_->decrement_active_contents_count();
}

void SiteInstanceImpl::AddObserver(Observer* observer) {
  observers_.AddObserver(observer);
}

void SiteInstanceImpl::RemoveObserver(Observer* observer) {
  observers_.RemoveObserver(observer);
}

void SiteInstanceImpl::set_render_process_host_factory(
    const RenderProcessHostFactory* rph_factory) {
  g_render_process_host_factory_ = rph_factory;
}

BrowserContext* SiteInstanceImpl::GetBrowserContext() const {
  return browsing_instance_->browser_context();
}

// static
scoped_refptr<SiteInstance> SiteInstance::Create(
    BrowserContext* browser_context) {
  return SiteInstanceImpl::Create(browser_context);
}

// static
scoped_refptr<SiteInstance> SiteInstance::CreateForURL(
    BrowserContext* browser_context,
    const GURL& url) {
  return SiteInstanceImpl::CreateForURL(browser_context, url);
}

// static
bool SiteInstance::IsSameWebSite(BrowserContext* browser_context,
                                 const GURL& real_src_url,
                                 const GURL& real_dest_url) {
  GURL src_url = SiteInstanceImpl::GetEffectiveURL(browser_context,
                                                   real_src_url);
  GURL dest_url = SiteInstanceImpl::GetEffectiveURL(browser_context,
                                                    real_dest_url);

  // We infer web site boundaries based on the registered domain name of the
  // top-level page and the scheme.  We do not pay attention to the port if
  // one is present, because pages served from different ports can still
  // access each other if they change their document.domain variable.

  // Some special URLs will match the site instance of any other URL. This is
  // done before checking both of them for validity, since we want these URLs
  // to have the same site instance as even an invalid one.
  if (IsRendererDebugURL(src_url) || IsRendererDebugURL(dest_url))
    return true;

  // If either URL is invalid, they aren't part of the same site.
  if (!src_url.is_valid() || !dest_url.is_valid())
    return false;

  // If the destination url is just a blank page, we treat them as part of the
  // same site.
  GURL blank_page(url::kAboutBlankURL);
  if (dest_url == blank_page)
    return true;

  // If the schemes differ, they aren't part of the same site.
  if (src_url.scheme() != dest_url.scheme())
    return false;

  return net::registry_controlled_domains::SameDomainOrHost(
      src_url,
      dest_url,
      net::registry_controlled_domains::INCLUDE_PRIVATE_REGISTRIES);
}

// static
GURL SiteInstance::GetSiteForURL(BrowserContext* browser_context,
                                 const GURL& real_url) {
  // TODO(fsamuel, creis): For some reason appID is not recognized as a host.
  if (real_url.SchemeIs(kGuestScheme))
    return real_url;

  GURL url = SiteInstanceImpl::GetEffectiveURL(browser_context, real_url);
  url::Origin origin(url);

  // If the url has a host, then determine the site.
  if (!origin.host().empty()) {
    // Only keep the scheme and registered domain of |origin|.
    std::string domain = net::registry_controlled_domains::GetDomainAndRegistry(
        origin.host(),
        net::registry_controlled_domains::INCLUDE_PRIVATE_REGISTRIES);
    std::string site = origin.scheme();
    site += url::kStandardSchemeSeparator;
    site += domain.empty() ? origin.host() : domain;
    return GURL(site);
  }

  // If there is no host but there is a scheme, return the scheme.
  // This is useful for cases like file URLs.
  if (url.has_scheme())
    return GURL(url.scheme() + ":");

  // Otherwise the URL should be invalid; return an empty site.
  DCHECK(!url.is_valid());
  return GURL();
}

// static
GURL SiteInstanceImpl::GetEffectiveURL(BrowserContext* browser_context,
                                       const GURL& url) {
  return GetContentClient()->browser()->
      GetEffectiveURL(browser_context, url);
}

// static
bool SiteInstanceImpl::DoesSiteRequireDedicatedProcess(
    BrowserContext* browser_context,
    const GURL& url) {
  // If --site-per-process is enabled, site isolation is enabled everywhere.
  if (SiteIsolationPolicy::UseDedicatedProcessesForAllSites())
    return true;

  // Let the content embedder enable site isolation for specific URLs. Use the
  // canonical site url for this check, so that schemes with nested origins
  // (blob and filesystem) work properly.
  GURL site_url = GetSiteForURL(browser_context, url);
  if (GetContentClient()->IsSupplementarySiteIsolationModeEnabled() &&
      GetContentClient()->browser()->DoesSiteRequireDedicatedProcess(
          browser_context, site_url)) {
    return true;
  }

  return false;
}

void SiteInstanceImpl::RenderProcessHostDestroyed(RenderProcessHost* host) {
  DCHECK_EQ(process_, host);
  process_->RemoveObserver(this);
  process_ = nullptr;
}

void SiteInstanceImpl::RenderProcessWillExit(RenderProcessHost* host) {
  // TODO(nick): http://crbug.com/575400 - RenderProcessWillExit might not serve
  // any purpose here.
  FOR_EACH_OBSERVER(Observer, observers_, RenderProcessGone(this));
}

void SiteInstanceImpl::RenderProcessExited(RenderProcessHost* host,
                                           base::TerminationStatus status,
                                           int exit_code) {
  FOR_EACH_OBSERVER(Observer, observers_, RenderProcessGone(this));
}

void SiteInstanceImpl::LockToOrigin() {
  // TODO(nick): When all sites are isolated, this operation provides strong
  // protection. If only some sites are isolated, we need additional logic to
  // prevent the non-isolated sites from requesting resources for isolated
  // sites. https://crbug.com/509125
  if (RequiresDedicatedProcess()) {
    // Guest processes cannot be locked to its site because guests always have
    // a fixed SiteInstance. The site of GURLs a guest loads doesn't match that
    // SiteInstance. So we skip locking the guest process to the site.
    // TODO(ncarter): Remove this exclusion once we can make origin lock per
    // RenderFrame routing id.
    if (site_.SchemeIs(content::kGuestScheme))
      return;

    // TODO(creis, nick) https://crbug.com/510588 Chrome UI pages use the same
    // site (chrome://chrome), so they can't be locked because the site being
    // loaded doesn't match the SiteInstance.
    if (site_.SchemeIs(content::kChromeUIScheme))
      return;

    // TODO(creis, nick): Until we can handle sites with effective URLs at the
    // call sites of ChildProcessSecurityPolicy::CanAccessDataForOrigin, we
    // must give the embedder a chance to exempt some sites to avoid process
    // kills.
    if (!GetContentClient()->browser()->ShouldLockToOrigin(
            browsing_instance_->browser_context(), site_))
      return;

    ChildProcessSecurityPolicyImpl* policy =
        ChildProcessSecurityPolicyImpl::GetInstance();
    policy->LockToOrigin(process_->GetID(), site_);
  }
}

}  // namespace content
