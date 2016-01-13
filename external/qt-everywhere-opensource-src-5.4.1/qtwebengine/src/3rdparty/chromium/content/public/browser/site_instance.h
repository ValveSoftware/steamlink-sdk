// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_PUBLIC_BROWSER_SITE_INSTANCE_H_
#define CONTENT_PUBLIC_BROWSER_SITE_INSTANCE_H_

#include "base/basictypes.h"
#include "base/memory/ref_counted.h"
#include "content/common/content_export.h"
#include "url/gurl.h"

namespace content {
class BrowserContext;
class RenderProcessHost;

///////////////////////////////////////////////////////////////////////////////
// SiteInstance interface.
//
// A SiteInstance represents a group of web pages that may be able to
// synchronously script each other, and thus must live in the same renderer
// process.
//
// We identify this group using a combination of where the page comes from
// (the site) and which tabs have references to each other (the instance).
// Here, a "site" is similar to the page's origin, but it only includes the
// registered domain name and scheme, not the port or subdomains.  This accounts
// for the fact that changes to document.domain allow similar origin pages with
// different ports or subdomains to script each other.  An "instance" includes
// all tabs that might be able to script each other because of how they were
// created (e.g., window.open or targeted links).  We represent instances using
// the BrowsingInstance class.
//
// Process models:
//
// In process-per-site-instance (the current default process model),
// SiteInstances are created (1) when the user manually creates a new tab
// (which also creates a new BrowsingInstance), and (2) when the user navigates
// across site boundaries (which uses the same BrowsingInstance).  If the user
// navigates within a site, the same SiteInstance is used.
// (Caveat: we currently allow renderer-initiated cross-site navigations to
// stay in the same SiteInstance, to preserve compatibility in cases like
// cross-site iframes that open popups.)
//
// In --process-per-tab, SiteInstances are created when the user manually
// creates a new tab, but not when navigating across site boundaries (unless
// a process swap is required for security reasons, such as navigating from
// a privileged WebUI page to a normal web page).  This corresponds to one
// process per BrowsingInstance.
//
// In --process-per-site, we consolidate all SiteInstances for a given site into
// the same process, throughout the entire browser context.  This ensures that
// only one process will be used for each site.
//
// Each NavigationEntry for a WebContents points to the SiteInstance that
// rendered it.  Each RenderViewHost also points to the SiteInstance that it is
// associated with.  A SiteInstance keeps track of the number of these
// references and deletes itself when the count goes to zero.  This means that
// a SiteInstance is only live as long as it is accessible, either from new
// tabs with no NavigationEntries or in NavigationEntries in the history.
//
///////////////////////////////////////////////////////////////////////////////
class CONTENT_EXPORT SiteInstance : public base::RefCounted<SiteInstance> {
 public:
  // Returns a unique ID for this SiteInstance.
  virtual int32 GetId() = 0;

  // Whether this SiteInstance has a running process associated with it.
  // This may return true before the first call to GetProcess(), in cases where
  // we use process-per-site and there is an existing process available.
  virtual bool HasProcess() const = 0;

  // Returns the current RenderProcessHost being used to render pages for this
  // SiteInstance.  If there is no RenderProcessHost (because either none has
  // yet been created or there was one but it was cleanly destroyed (e.g. when
  // it is not actively being used)), then this method will create a new
  // RenderProcessHost (and a new ID).  Note that renderer process crashes leave
  // the current RenderProcessHost (and ID) in place.
  //
  // For sites that require process-per-site mode (e.g., WebUI), this will
  // ensure only one RenderProcessHost for the site exists/ within the
  // BrowserContext.
  virtual content::RenderProcessHost* GetProcess() = 0;

  // Browser context to which this SiteInstance (and all related
  // SiteInstances) belongs.
  virtual content::BrowserContext* GetBrowserContext() const = 0;

  // Get the web site that this SiteInstance is rendering pages for.
  // This includes the scheme and registered domain, but not the port.
  virtual const GURL& GetSiteURL() const = 0;

  // Gets a SiteInstance for the given URL that shares the current
  // BrowsingInstance, creating a new SiteInstance if necessary.  This ensures
  // that a BrowsingInstance only has one SiteInstance per site, so that pages
  // in a BrowsingInstance have the ability to script each other.  Callers
  // should ensure that this SiteInstance becomes ref counted, by storing it in
  // a scoped_refptr.  (By having this method, we can hide the BrowsingInstance
  // class from the rest of the codebase.)
  // TODO(creis): This may be an argument to build a pass_refptr<T> class, as
  // Darin suggests.
  virtual SiteInstance* GetRelatedSiteInstance(const GURL& url) = 0;

  // Returns whether the given SiteInstance is in the same BrowsingInstance as
  // this one.  If so, JavaScript interactions that are permitted across
  // origins (e.g., postMessage) should be supported.
  virtual bool IsRelatedSiteInstance(const SiteInstance* instance) = 0;

  // Returns the total active WebContents count for this SiteInstance and all
  // related SiteInstances in the same BrowsingInstance.
  virtual size_t GetRelatedActiveContentsCount() = 0;

  // Factory method to create a new SiteInstance.  This will create a new
  // new BrowsingInstance, so it should only be used when creating a new tab
  // from scratch (or similar circumstances).  Callers should ensure that
  // this SiteInstance becomes ref counted, by storing it in a scoped_refptr.
  //
  // The render process host factory may be NULL. See SiteInstance constructor.
  //
  // TODO(creis): This may be an argument to build a pass_refptr<T> class, as
  // Darin suggests.
  static SiteInstance* Create(content::BrowserContext* browser_context);

  // Factory method to get the appropriate SiteInstance for the given URL, in
  // a new BrowsingInstance.  Use this instead of Create when you know the URL,
  // since it allows special site grouping rules to be applied (for example,
  // to group chrome-ui pages into the same instance).
  static SiteInstance* CreateForURL(
      content::BrowserContext* browser_context, const GURL& url);

  // Return whether both URLs are part of the same web site, for the purpose of
  // assigning them to processes accordingly.  The decision is currently based
  // on the registered domain of the URLs (google.com, bbc.co.uk), as well as
  // the scheme (https, http).  This ensures that two pages will be in
  // the same process if they can communicate with other via JavaScript.
  // (e.g., docs.google.com and mail.google.com have DOM access to each other
  // if they both set their document.domain properties to google.com.)
  static bool IsSameWebSite(content::BrowserContext* browser_context,
                            const GURL& url1, const GURL& url2);

  // Returns the site for the given URL, which includes only the scheme and
  // registered domain.  Returns an empty GURL if the URL has no host.
  static GURL GetSiteForURL(BrowserContext* context, const GURL& url);

 protected:
  friend class base::RefCounted<SiteInstance>;

  SiteInstance() {}
  virtual ~SiteInstance() {}
};

}  // namespace content.

#endif  // CONTENT_PUBLIC_BROWSER_SITE_INSTANCE_H_
