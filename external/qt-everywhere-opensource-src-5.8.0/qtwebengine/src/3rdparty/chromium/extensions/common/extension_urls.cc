// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "extensions/common/extension_urls.h"

#include "base/strings/string_util.h"
#include "base/strings/stringprintf.h"
#include "base/strings/utf_string_conversions.h"
#include "extensions/common/constants.h"
#include "extensions/common/extensions_client.h"
#include "net/base/escape.h"
#include "net/base/url_util.h"
#include "url/gurl.h"

namespace extensions {

const char kEventBindings[] = "event_bindings";

const char kSchemaUtils[] = "schemaUtils";

bool IsSourceFromAnExtension(const base::string16& source) {
  return GURL(source).SchemeIs(kExtensionScheme) ||
         base::StartsWith(source, base::ASCIIToUTF16("extensions::"),
                          base::CompareCase::SENSITIVE);
}

}  // namespace extensions

namespace extension_urls {

const char kChromeWebstoreBaseURL[] = "https://chrome.google.com/webstore";
const char kChromeWebstoreUpdateURL[] =
    "https://clients2.google.com/service/update2/crx";

std::string GetWebstoreLaunchURL() {
  extensions::ExtensionsClient* client = extensions::ExtensionsClient::Get();
  if (client)
    return client->GetWebstoreBaseURL();
  return kChromeWebstoreBaseURL;
}

std::string GetWebstoreExtensionsCategoryURL() {
  return GetWebstoreLaunchURL() + "/category/extensions";
}

std::string GetWebstoreItemDetailURLPrefix() {
  return GetWebstoreLaunchURL() + "/detail/";
}

GURL GetWebstoreItemJsonDataURL(const std::string& extension_id) {
  return GURL(GetWebstoreLaunchURL() + "/inlineinstall/detail/" + extension_id);
}

GURL GetWebstoreJsonSearchUrl(const std::string& query,
                              const std::string& host_language_code) {
  GURL url(GetWebstoreLaunchURL() + "/jsonsearch");
  url = net::AppendQueryParameter(url, "q", query);
  url = net::AppendQueryParameter(url, "hl", host_language_code);
  return url;
}

GURL GetWebstoreSearchPageUrl(const std::string& query) {
  return GURL(GetWebstoreLaunchURL() + "/search/" +
              net::EscapeQueryParamValue(query, false));
}

GURL GetWebstoreUpdateUrl() {
  extensions::ExtensionsClient* client = extensions::ExtensionsClient::Get();
  if (client)
    return GURL(client->GetWebstoreUpdateURL());
  return GURL(kChromeWebstoreUpdateURL);
}

GURL GetWebstoreReportAbuseUrl(const std::string& extension_id,
                               const std::string& referrer_id) {
  return GURL(base::StringPrintf("%s/report/%s?utm_source=%s",
                                 GetWebstoreLaunchURL().c_str(),
                                 extension_id.c_str(), referrer_id.c_str()));
}

bool IsWebstoreUpdateUrl(const GURL& update_url) {
  GURL store_url = GetWebstoreUpdateUrl();
  if (update_url == store_url) {
    return true;
  } else {
    return (update_url.host() == store_url.host() &&
            update_url.path() == store_url.path());
  }
}

bool IsBlacklistUpdateUrl(const GURL& url) {
  extensions::ExtensionsClient* client = extensions::ExtensionsClient::Get();
  if (client)
    return client->IsBlacklistUpdateURL(url);
  return false;
}

}  // namespace extension_urls
