// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "google_apis/drive/gdata_wapi_url_generator.h"

#include "base/logging.h"
#include "base/strings/string_number_conversions.h"
#include "base/strings/stringprintf.h"
#include "net/base/escape.h"
#include "net/base/url_util.h"
#include "url/gurl.h"

namespace google_apis {
namespace {

// URL requesting single resource entry whose resource id is followed by this
// prefix.
const char kGetEditURLPrefix[] = "/feeds/default/private/full/";

}  // namespace

const char GDataWapiUrlGenerator::kBaseUrlForProduction[] =
    "https://docs.google.com/";

// static
GURL GDataWapiUrlGenerator::AddStandardUrlParams(const GURL& url) {
  GURL result = net::AppendOrReplaceQueryParameter(url, "v", "3");
  result = net::AppendOrReplaceQueryParameter(result, "alt", "json");
  result = net::AppendOrReplaceQueryParameter(result, "showroot", "true");
  return result;
}

GDataWapiUrlGenerator::GDataWapiUrlGenerator(const GURL& base_url)
    : base_url_(base_url) {
}

GDataWapiUrlGenerator::~GDataWapiUrlGenerator() {
}

GURL GDataWapiUrlGenerator::GenerateEditUrl(
    const std::string& resource_id) const {
  return AddStandardUrlParams(GenerateEditUrlWithoutParams(resource_id));
}

GURL GDataWapiUrlGenerator::GenerateEditUrlWithoutParams(
    const std::string& resource_id) const {
  return base_url_.Resolve(kGetEditURLPrefix + net::EscapePath(resource_id));
}

GURL GDataWapiUrlGenerator::GenerateEditUrlWithEmbedOrigin(
    const std::string& resource_id, const GURL& embed_origin) const {
  GURL url = GenerateEditUrl(resource_id);
  if (!embed_origin.is_empty()) {
    // Construct a valid serialized embed origin from an url, according to
    // WD-html5-20110525. Such string has to be built manually, since
    // GURL::spec() always adds the trailing slash. Moreover, ports are
    // currently not supported.
    DCHECK(!embed_origin.has_port());
    DCHECK(!embed_origin.has_path() || embed_origin.path() == "/");
    const std::string serialized_embed_origin =
        embed_origin.scheme() + "://" + embed_origin.host();
    url = net::AppendOrReplaceQueryParameter(
        url, "embedOrigin", serialized_embed_origin);
  }
  return url;
}

}  // namespace google_apis
