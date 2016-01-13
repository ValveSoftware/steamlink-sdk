// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
//
// URL utility functions for Google Documents List API (aka WAPI).

#ifndef GOOGLE_APIS_DRIVE_GDATA_WAPI_URL_GENERATOR_H_
#define GOOGLE_APIS_DRIVE_GDATA_WAPI_URL_GENERATOR_H_

#include <string>

#include "url/gurl.h"

namespace google_apis {

// The class is used to generate URLs for communicating with the WAPI server.
// for production, and the local server for testing.
class GDataWapiUrlGenerator {
 public:
  GDataWapiUrlGenerator(const GURL& base_url);
  ~GDataWapiUrlGenerator();

  // The base URL for communicating with the WAPI server for production.
  static const char kBaseUrlForProduction[];

  // The base URL for the file download server for production.
  static const char kBaseDownloadUrlForProduction[];

  // Adds additional parameters for API version, output content type and to
  // show folders in the feed are added to document feed URLs.
  static GURL AddStandardUrlParams(const GURL& url);

  // Generates a URL for getting or editing the resource entry of
  // the given resource ID.
  GURL GenerateEditUrl(const std::string& resource_id) const;

  // Generates a URL for getting or editing the resource entry of the
  // given resource ID without query params.
  // Note that, in order to access to the WAPI server, it is necessary to
  // append some query parameters to the URL. GenerateEditUrl declared above
  // should be used in such cases. This method is designed for constructing
  // the data, such as xml element/attributes in request body containing
  // edit urls.
  GURL GenerateEditUrlWithoutParams(const std::string& resource_id) const;

  // Generates a URL for getting or editing the resource entry of the given
  // resource ID with additionally passed embed origin. This is used to fetch
  // share urls for the sharing dialog to be embedded with the |embed_origin|
  // origin.
  GURL GenerateEditUrlWithEmbedOrigin(const std::string& resource_id,
                                      const GURL& embed_origin) const;

 private:
  const GURL base_url_;
};

}  // namespace google_apis

#endif  // GOOGLE_APIS_DRIVE_GDATA_WAPI_URL_GENERATOR_H_
