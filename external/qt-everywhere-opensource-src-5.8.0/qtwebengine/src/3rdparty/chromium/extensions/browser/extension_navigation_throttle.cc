// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "extensions/browser/extension_navigation_throttle.h"

#include "content/public/browser/browser_thread.h"
#include "content/public/browser/navigation_handle.h"
#include "content/public/browser/render_frame_host.h"
#include "content/public/browser/web_contents.h"
#include "content/public/common/url_constants.h"
#include "extensions/browser/extension_registry.h"
#include "extensions/common/constants.h"
#include "extensions/common/extension.h"
#include "extensions/common/extension_set.h"
#include "extensions/common/manifest_handlers/web_accessible_resources_info.h"

namespace extensions {

ExtensionNavigationThrottle::ExtensionNavigationThrottle(
    content::NavigationHandle* navigation_handle)
    : content::NavigationThrottle(navigation_handle) {}

ExtensionNavigationThrottle::~ExtensionNavigationThrottle() {}

content::NavigationThrottle::ThrottleCheckResult
ExtensionNavigationThrottle::WillStartRequest() {
  DCHECK_CURRENTLY_ON(content::BrowserThread::UI);

  // This method for now enforces only web_accessible_resources for navigations.
  // Top-level navigations should always be allowed.
  DCHECK(!navigation_handle()->IsInMainFrame());

  // If the navigation is not to a chrome-extension:// URL, no need to perform
  // any more checks.
  if (!navigation_handle()->GetURL().SchemeIs(extensions::kExtensionScheme))
    return content::NavigationThrottle::PROCEED;

  // The subframe which is navigated needs to have all of its ancestors be
  // at the same origin, otherwise the resource needs to be explicitly listed
  // in web_accessible_resources.
  // Since the RenderFrameHost is not known until navigation has committed,
  // we can't get it from NavigationHandle. However, this code only cares about
  // the ancestor chain, so find the current RenderFrameHost and use it to
  // traverse up to the main frame.
  content::RenderFrameHost* navigating_frame = nullptr;
  for (auto* frame : navigation_handle()->GetWebContents()->GetAllFrames()) {
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
    if (ancestor->GetLastCommittedURL().GetOrigin() !=
        navigation_handle()->GetURL().GetOrigin()) {
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
  std::string resource_path = navigation_handle()->GetURL().path();
  ExtensionRegistry* registry = ExtensionRegistry::Get(
      navigation_handle()->GetWebContents()->GetBrowserContext());
  if (!registry)
    return content::NavigationThrottle::BLOCK_REQUEST;

  const extensions::Extension* extension =
      registry->enabled_extensions().GetByID(
          navigation_handle()->GetURL().host());
  if (!extension)
    return content::NavigationThrottle::BLOCK_REQUEST;

  if (WebAccessibleResourcesInfo::IsResourceWebAccessible(extension,
                                                          resource_path)) {
    return content::NavigationThrottle::PROCEED;
  }

  return content::NavigationThrottle::BLOCK_REQUEST;
}

}  // namespace extensions
