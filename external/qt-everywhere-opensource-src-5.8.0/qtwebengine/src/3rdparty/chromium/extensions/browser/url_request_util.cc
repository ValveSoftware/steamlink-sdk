// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "extensions/browser/url_request_util.h"

#include <string>

#include "content/public/browser/resource_request_info.h"
#include "extensions/browser/guest_view/web_view/web_view_renderer_state.h"
#include "extensions/browser/info_map.h"
#include "extensions/common/extension.h"
#include "extensions/common/manifest_handlers/icons_handler.h"
#include "extensions/common/manifest_handlers/web_accessible_resources_info.h"
#include "extensions/common/manifest_handlers/webview_info.h"
#include "net/url_request/url_request.h"

namespace extensions {
namespace url_request_util {

bool AllowCrossRendererResourceLoad(net::URLRequest* request,
                                    bool is_incognito,
                                    const Extension* extension,
                                    InfoMap* extension_info_map,
                                    bool* allowed) {
  const content::ResourceRequestInfo* info =
      content::ResourceRequestInfo::ForRequest(request);

  // Extensions with webview: allow loading certain resources by guest renderers
  // with privileged partition IDs as specified in owner's extension the
  // manifest file.
  std::string owner_extension_id;
  int owner_process_id;
  WebViewRendererState::GetInstance()->GetOwnerInfo(
      info->GetChildID(), &owner_process_id, &owner_extension_id);
  const Extension* owner_extension =
      extension_info_map->extensions().GetByID(owner_extension_id);
  std::string partition_id;
  bool is_guest = WebViewRendererState::GetInstance()->GetPartitionID(
      info->GetChildID(), &partition_id);
  std::string resource_path = request->url().path();

  // |owner_extension == extension| needs to be checked because extension
  // resources should only be accessible to WebViews owned by that extension.
  if (is_guest && owner_extension == extension &&
      WebviewInfo::IsResourceWebviewAccessible(extension, partition_id,
                                               resource_path)) {
    *allowed = true;
    return true;
  }

  if (is_guest &&
      !ui::PageTransitionIsWebTriggerable(info->GetPageTransition())) {
    *allowed = false;
    return true;
  }

  // The following checks require that we have an actual extension object. If we
  // don't have it, allow the request handling to continue with the rest of the
  // checks.
  if (!extension) {
    *allowed = true;
    return true;
  }

  // Disallow loading of packaged resources for hosted apps. We don't allow
  // hybrid hosted/packaged apps. The one exception is access to icons, since
  // some extensions want to be able to do things like create their own
  // launchers.
  std::string resource_root_relative_path =
      request->url().path().empty() ? std::string()
                                    : request->url().path().substr(1);
  if (extension->is_hosted_app() &&
      !IconsInfo::GetIcons(extension)
           .ContainsPath(resource_root_relative_path)) {
    LOG(ERROR) << "Denying load of " << request->url().spec() << " from "
               << "hosted app.";
    *allowed = false;
    return true;
  }

  DCHECK_EQ(extension->url(), request->url().GetWithEmptyPath());

  // Extensions with manifest before v2 did not have web_accessible_resource
  // section, therefore the request needs to be allowed.
  if (extension->manifest_version() < 2) {
    *allowed = true;
    return true;
  }

  // Navigating the main frame to an extension URL is allowed, even if not
  // explicitly listed as web_accessible_resource.
  if (info->GetResourceType() == content::RESOURCE_TYPE_MAIN_FRAME) {
    *allowed = true;
    return true;
  } else if (info->GetResourceType() == content::RESOURCE_TYPE_SUB_FRAME) {
    // When navigating in subframe, allow if it is the same origin
    // as the top-level frame. This can only be the case if the subframe
    // request is coming from the extension process.
    if (extension_info_map->process_map().Contains(info->GetChildID())) {
      *allowed = true;
      return true;
    }

    // Also allow if the file is explicitly listed as a web_accessible_resource.
    if (WebAccessibleResourcesInfo::IsResourceWebAccessible(extension,
                                                            resource_path)) {
      *allowed = true;
      return true;
    }
  }

  // Since not all subresources are required to be listed in a v2
  // manifest, we must allow all subresource loads if there are any web
  // accessible resources. See http://crbug.com/179127.
  if (!content::IsResourceTypeFrame(info->GetResourceType()) &&
      WebAccessibleResourcesInfo::HasWebAccessibleResources(extension)) {
    *allowed = true;
    return true;
  }

  if (!ui::PageTransitionIsWebTriggerable(info->GetPageTransition())) {
    *allowed = false;
    return true;
  }

  // Couldn't determine if the resource is allowed or not.
  return false;
}

bool IsWebViewRequest(const net::URLRequest* request) {
  const content::ResourceRequestInfo* info =
      content::ResourceRequestInfo::ForRequest(request);
  // |info| can be NULL sometimes: http://crbug.com/370070.
  if (!info)
    return false;
  return WebViewRendererState::GetInstance()->IsGuest(info->GetChildID());
}

}  // namespace url_request_util
}  // namespace extensions
