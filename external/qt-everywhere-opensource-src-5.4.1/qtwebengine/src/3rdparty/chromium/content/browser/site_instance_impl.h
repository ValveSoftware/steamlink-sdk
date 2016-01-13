// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_BROWSER_SITE_INSTANCE_IMPL_H_
#define CONTENT_BROWSER_SITE_INSTANCE_IMPL_H_

#include "content/browser/renderer_host/render_process_host_impl.h"
#include "content/common/content_export.h"
#include "content/public/browser/render_process_host_observer.h"
#include "content/public/browser/site_instance.h"
#include "url/gurl.h"

namespace content {
class BrowsingInstance;
class RenderProcessHostFactory;

class CONTENT_EXPORT SiteInstanceImpl : public SiteInstance,
                                        public RenderProcessHostObserver {
 public:
  // SiteInstance interface overrides.
  virtual int32 GetId() OVERRIDE;
  virtual bool HasProcess() const OVERRIDE;
  virtual RenderProcessHost* GetProcess() OVERRIDE;
  virtual BrowserContext* GetBrowserContext() const OVERRIDE;
  virtual const GURL& GetSiteURL() const OVERRIDE;
  virtual SiteInstance* GetRelatedSiteInstance(const GURL& url) OVERRIDE;
  virtual bool IsRelatedSiteInstance(const SiteInstance* instance) OVERRIDE;
  virtual size_t GetRelatedActiveContentsCount() OVERRIDE;

  // Set the web site that this SiteInstance is rendering pages for.
  // This includes the scheme and registered domain, but not the port.  If the
  // URL does not have a valid registered domain, then the full hostname is
  // stored.
  void SetSite(const GURL& url);
  bool HasSite() const;

  // Returns whether there is currently a related SiteInstance (registered with
  // BrowsingInstance) for the site of the given url.  If so, we should try to
  // avoid dedicating an unused SiteInstance to it (e.g., in a new tab).
  bool HasRelatedSiteInstance(const GURL& url);

  // Returns whether this SiteInstance has a process that is the wrong type for
  // the given URL.  If so, the browser should force a process swap when
  // navigating to the URL.
  bool HasWrongProcessForURL(const GURL& url);

  // Increase the number of active views in this SiteInstance. This is
  // increased when a view is created, or a currently swapped out view
  // is swapped in.
  void increment_active_view_count() { active_view_count_++; }

  // Decrease the number of active views in this SiteInstance. This is
  // decreased when a view is destroyed, or a currently active view is
  // swapped out.
  void decrement_active_view_count() { active_view_count_--; }

  // Get the number of active views which belong to this
  // SiteInstance. If there is no active view left in this
  // SiteInstance, all view in this SiteInstance can be safely
  // discarded to save memory.
  size_t active_view_count() { return active_view_count_; }

  // Increase the number of active WebContentses using this SiteInstance. Note
  // that, unlike active_view_count, this does not count pending RVHs.
  void IncrementRelatedActiveContentsCount();

  // Decrease the number of active WebContentses using this SiteInstance. Note
  // that, unlike active_view_count, this does not count pending RVHs.
  void DecrementRelatedActiveContentsCount();

  // Sets the global factory used to create new RenderProcessHosts.  It may be
  // NULL, in which case the default BrowserRenderProcessHost will be created
  // (this is the behavior if you don't call this function).  The factory must
  // be set back to NULL before it's destroyed; ownership is not transferred.
  static void set_render_process_host_factory(
      const RenderProcessHostFactory* rph_factory);

  // Get the effective URL for the given actual URL.  This allows the
  // ContentBrowserClient to override the SiteInstance's site for certain URLs.
  // For example, Chrome uses this to replace hosted app URLs with extension
  // hosts.
  // Only public so that we can make a consistent process swap decision in
  // RenderFrameHostManager.
  static GURL GetEffectiveURL(BrowserContext* browser_context,
                              const GURL& url);

 protected:
  friend class BrowsingInstance;
  friend class SiteInstance;

  // Virtual to allow tests to extend it.
  virtual ~SiteInstanceImpl();

  // Create a new SiteInstance.  Protected to give access to BrowsingInstance
  // and tests; most callers should use Create or GetRelatedSiteInstance
  // instead.
  explicit SiteInstanceImpl(BrowsingInstance* browsing_instance);

 private:
  // RenderProcessHostObserver implementation.
  virtual void RenderProcessHostDestroyed(RenderProcessHost* host) OVERRIDE;

  // Used to restrict a process' origin access rights.
  void LockToOrigin();

  // An object used to construct RenderProcessHosts.
  static const RenderProcessHostFactory* g_render_process_host_factory_;

  // The next available SiteInstance ID.
  static int32 next_site_instance_id_;

  // A unique ID for this SiteInstance.
  int32 id_;

  // The number of active views under this SiteInstance.
  size_t active_view_count_;

  // BrowsingInstance to which this SiteInstance belongs.
  scoped_refptr<BrowsingInstance> browsing_instance_;

  // Current RenderProcessHost that is rendering pages for this SiteInstance.
  // This pointer will only change once the RenderProcessHost is destructed.  It
  // will still remain the same even if the process crashes, since in that
  // scenario the RenderProcessHost remains the same.
  RenderProcessHost* process_;

  // The web site that this SiteInstance is rendering pages for.
  GURL site_;

  // Whether SetSite has been called.
  bool has_site_;

  DISALLOW_COPY_AND_ASSIGN(SiteInstanceImpl);
};

}  // namespace content

#endif  // CONTENT_BROWSER_SITE_INSTANCE_IMPL_H_
