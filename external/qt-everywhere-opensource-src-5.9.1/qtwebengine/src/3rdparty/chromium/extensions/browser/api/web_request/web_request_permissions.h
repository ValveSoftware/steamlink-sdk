// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef EXTENSIONS_BROWSER_API_WEB_REQUEST_WEB_REQUEST_PERMISSIONS_H_
#define EXTENSIONS_BROWSER_API_WEB_REQUEST_WEB_REQUEST_PERMISSIONS_H_

#include <map>
#include <string>

#include "base/macros.h"
#include "extensions/common/permissions/permissions_data.h"

class GURL;

namespace extensions {
class ExtensionNavigationUIData;
class InfoMap;
}

namespace net {
class URLRequest;
}

// Exposed for unit testing.
bool IsSensitiveURL(const GURL& url);

// This class is used to test whether extensions may modify web requests.
class WebRequestPermissions {
 public:
  // Different host permission checking modes for CanExtensionAccessURL.
  enum HostPermissionsCheck {
    DO_NOT_CHECK_HOST = 0,    // No check.
    REQUIRE_HOST_PERMISSION,  // Permission needed for given URL.
    REQUIRE_ALL_URLS          // Permission needed for <all_urls>.
  };

  // Returns true if the request shall not be reported to extensions.
  static bool HideRequest(
      const extensions::InfoMap* extension_info_map,
      const net::URLRequest* request,
      extensions::ExtensionNavigationUIData* navigation_ui_data);

  // Helper function used only in tests, sets a variable which enables or
  // disables a CHECK.
  static void AllowAllExtensionLocationsInPublicSessionForTesting(bool value);

  // |host_permission_check| controls how permissions are checked with regard to
  // |url|.
  static extensions::PermissionsData::AccessType CanExtensionAccessURL(
      const extensions::InfoMap* extension_info_map,
      const std::string& extension_id,
      const GURL& url,
      int tab_id,
      bool crosses_incognito,
      HostPermissionsCheck host_permissions_check);

 private:
  DISALLOW_IMPLICIT_CONSTRUCTORS(WebRequestPermissions);
};

#endif  // EXTENSIONS_BROWSER_API_WEB_REQUEST_WEB_REQUEST_PERMISSIONS_H_
