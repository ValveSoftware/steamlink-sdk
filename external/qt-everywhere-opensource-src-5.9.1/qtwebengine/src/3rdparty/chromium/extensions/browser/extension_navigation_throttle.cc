// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "extensions/browser/extension_navigation_throttle.h"

#include "components/guest_view/browser/guest_view_base.h"
#include "content/public/browser/browser_thread.h"
#include "content/public/browser/navigation_handle.h"
#include "content/public/browser/render_frame_host.h"
#include "content/public/browser/web_contents.h"
#include "content/public/common/browser_side_navigation_policy.h"
#include "content/public/common/url_constants.h"
#include "extensions/browser/extension_registry.h"
#include "extensions/browser/guest_view/web_view/web_view_guest.h"
#include "extensions/browser/url_request_util.h"
#include "extensions/common/constants.h"
#include "extensions/common/extension.h"
#include "extensions/common/extension_set.h"
#include "extensions/common/manifest_handlers/web_accessible_resources_info.h"
#include "extensions/common/manifest_handlers/webview_info.h"
#include "extensions/common/permissions/api_permission.h"
#include "extensions/common/permissions/permissions_data.h"
#include "ui/base/page_transition_types.h"

namespace extensions {

ExtensionNavigationThrottle::ExtensionNavigationThrottle(
    content::NavigationHandle* navigation_handle)
    : content::NavigationThrottle(navigation_handle) {}

ExtensionNavigationThrottle::~ExtensionNavigationThrottle() {}

content::NavigationThrottle::ThrottleCheckResult
ExtensionNavigationThrottle::WillStartRequest() {
  DCHECK_CURRENTLY_ON(content::BrowserThread::UI);
  GURL url(navigation_handle()->GetURL());
  content::WebContents* web_contents = navigation_handle()->GetWebContents();
  ExtensionRegistry* registry =
      ExtensionRegistry::Get(web_contents->GetBrowserContext());

  if (navigation_handle()->IsInMainFrame()) {
    // Block top-level navigations to blob: or filesystem: URLs with extension
    // origin from non-extension processes.  See https://crbug.com/645028.
    bool is_nested_url = url.SchemeIsFileSystem() || url.SchemeIsBlob();
    bool is_extension = false;
    if (registry) {
      is_extension = !!registry->enabled_extensions().GetExtensionOrAppByURL(
          navigation_handle()->GetStartingSiteInstance()->GetSiteURL());
    }

    url::Origin origin(url);
    if (is_nested_url && origin.scheme() == extensions::kExtensionScheme &&
        !is_extension) {
      // Relax this restriction for apps that use <webview>.  See
      // https://crbug.com/652077.
      const extensions::Extension* extension =
          registry->enabled_extensions().GetByID(origin.host());
      bool has_webview_permission =
          extension &&
          extension->permissions_data()->HasAPIPermission(
              extensions::APIPermission::kWebView);
      if (!has_webview_permission)
        return content::NavigationThrottle::CANCEL;
    }

    if (content::IsBrowserSideNavigationEnabled() &&
        url.scheme() == extensions::kExtensionScheme) {
      // This logic is performed for PlzNavigate sub-resources and for
      // non-PlzNavigate in
      // extensions::url_request_util::AllowCrossRendererResourceLoad.
      const Extension* extension =
          registry->enabled_extensions().GetExtensionOrAppByURL(url);
      guest_view::GuestViewBase* guest =
          guest_view::GuestViewBase::FromWebContents(web_contents);
      if (guest) {
        std::string owner_extension_id = guest->owner_host();
        const Extension* owner_extension =
            registry->enabled_extensions().GetByID(owner_extension_id);

        std::string partition_domain, partition_id;
        bool in_memory;
        std::string resource_path = url.path();
        bool is_guest = WebViewGuest::GetGuestPartitionConfigForSite(
            navigation_handle()->GetStartingSiteInstance()->GetSiteURL(),
            &partition_domain, &partition_id, &in_memory);

        bool allowed = true;
        url_request_util::AllowCrossRendererResourceLoadHelper(
            is_guest, extension, owner_extension, partition_id, resource_path,
            navigation_handle()->GetPageTransition(), &allowed);
        if (!allowed)
          return content::NavigationThrottle::CANCEL;
      }
    }

    return content::NavigationThrottle::PROCEED;
  }

  // Now enforce web_accessible_resources for navigations. Top-level navigations
  // should always be allowed.

  // If the navigation is not to a chrome-extension:// URL, no need to perform
  // any more checks.
  if (!url.SchemeIs(extensions::kExtensionScheme))
    return content::NavigationThrottle::PROCEED;

  // The subframe which is navigated needs to have all of its ancestors be
  // at the same origin, otherwise the resource needs to be explicitly listed
  // in web_accessible_resources.
  // Since the RenderFrameHost is not known until navigation has committed,
  // we can't get it from NavigationHandle. However, this code only cares about
  // the ancestor chain, so find the current RenderFrameHost and use it to
  // traverse up to the main frame.
  content::RenderFrameHost* navigating_frame = nullptr;
  for (auto* frame : web_contents->GetAllFrames()) {
    if (frame->GetFrameTreeNodeId() ==
        navigation_handle()->GetFrameTreeNodeId()) {
      navigating_frame = frame;
      break;
    }
  }
  DCHECK(navigating_frame);

  // Traverse the chain of parent frames, checking if they are the same origin
  // as the URL of this navigation.
  content::RenderFrameHost* ancestor = navigating_frame->GetParent();
  bool external_ancestor = false;
  while (ancestor) {
    if (ancestor->GetLastCommittedURL().GetOrigin() != url.GetOrigin()) {
      // Ignore DevTools, as it is allowed to embed extension pages.
      if (!ancestor->GetLastCommittedURL().SchemeIs(
              content::kChromeDevToolsScheme)) {
        external_ancestor = true;
        break;
      }
    }
    ancestor = ancestor->GetParent();
  }

  if (!external_ancestor)
    return content::NavigationThrottle::PROCEED;

  // Since there was at least one origin different than the navigation URL,
  // explicitly check for the resource in web_accessible_resources.
  std::string resource_path = url.path();
  if (!registry)
    return content::NavigationThrottle::BLOCK_REQUEST;

  const extensions::Extension* extension =
      registry->enabled_extensions().GetByID(url.host());
  if (!extension)
    return content::NavigationThrottle::BLOCK_REQUEST;

  if (WebAccessibleResourcesInfo::IsResourceWebAccessible(extension,
                                                          resource_path)) {
    return content::NavigationThrottle::PROCEED;
  }

  return content::NavigationThrottle::BLOCK_REQUEST;
}

}  // namespace extensions
