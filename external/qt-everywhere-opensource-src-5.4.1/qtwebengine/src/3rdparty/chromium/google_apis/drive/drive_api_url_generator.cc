// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "google_apis/drive/drive_api_url_generator.h"

#include "base/logging.h"
#include "base/strings/string_number_conversions.h"
#include "base/strings/stringprintf.h"
#include "google_apis/google_api_keys.h"
#include "net/base/escape.h"
#include "net/base/url_util.h"

namespace google_apis {

namespace {

// Hard coded URLs for communication with a google drive server.
const char kDriveV2AboutUrl[] = "/drive/v2/about";
const char kDriveV2AppsUrl[] = "/drive/v2/apps";
const char kDriveV2ChangelistUrl[] = "/drive/v2/changes";
const char kDriveV2FilesUrl[] = "/drive/v2/files";
const char kDriveV2FileUrlPrefix[] = "/drive/v2/files/";
const char kDriveV2ChildrenUrlFormat[] = "/drive/v2/files/%s/children";
const char kDriveV2ChildrenUrlForRemovalFormat[] =
    "/drive/v2/files/%s/children/%s";
const char kDriveV2FileCopyUrlFormat[] = "/drive/v2/files/%s/copy";
const char kDriveV2FileDeleteUrlFormat[] = "/drive/v2/files/%s";
const char kDriveV2FileTrashUrlFormat[] = "/drive/v2/files/%s/trash";
const char kDriveV2InitiateUploadNewFileUrl[] = "/upload/drive/v2/files";
const char kDriveV2InitiateUploadExistingFileUrlPrefix[] =
    "/upload/drive/v2/files/";
const char kDriveV2PermissionsUrlFormat[] = "/drive/v2/files/%s/permissions";

// apps.delete and file.authorize API is exposed through a special endpoint
// v2internal that is accessible only by the official API key for Chrome.
const char kDriveV2InternalAppsUrl[] = "/drive/v2internal/apps";
const char kDriveV2AppsDeleteUrlFormat[] = "/drive/v2internal/apps/%s";
const char kDriveV2FilesAuthorizeUrlFormat[] =
    "/drive/v2internal/files/%s/authorize?appId=%s";

GURL AddResumableUploadParam(const GURL& url) {
  return net::AppendOrReplaceQueryParameter(url, "uploadType", "resumable");
}

}  // namespace

DriveApiUrlGenerator::DriveApiUrlGenerator(const GURL& base_url,
                                           const GURL& base_download_url)
    : base_url_(base_url),
      base_download_url_(base_download_url) {
  // Do nothing.
}

DriveApiUrlGenerator::~DriveApiUrlGenerator() {
  // Do nothing.
}

const char DriveApiUrlGenerator::kBaseUrlForProduction[] =
    "https://www.googleapis.com";
const char DriveApiUrlGenerator::kBaseDownloadUrlForProduction[] =
    "https://www.googledrive.com/host/";

GURL DriveApiUrlGenerator::GetAboutGetUrl() const {
  return base_url_.Resolve(kDriveV2AboutUrl);
}

GURL DriveApiUrlGenerator::GetAppsListUrl(bool use_internal_endpoint) const {
  return base_url_.Resolve(use_internal_endpoint ?
      kDriveV2InternalAppsUrl : kDriveV2AppsUrl);
}

GURL DriveApiUrlGenerator::GetAppsDeleteUrl(const std::string& app_id) const {
  return base_url_.Resolve(base::StringPrintf(
      kDriveV2AppsDeleteUrlFormat, net::EscapePath(app_id).c_str()));
}

GURL DriveApiUrlGenerator::GetFilesGetUrl(const std::string& file_id) const {
  return base_url_.Resolve(kDriveV2FileUrlPrefix + net::EscapePath(file_id));
}

GURL DriveApiUrlGenerator::GetFilesAuthorizeUrl(
    const std::string& file_id,
    const std::string& app_id) const {
  return base_url_.Resolve(base::StringPrintf(kDriveV2FilesAuthorizeUrlFormat,
                                              net::EscapePath(file_id).c_str(),
                                              net::EscapePath(app_id).c_str()));
}

GURL DriveApiUrlGenerator::GetFilesInsertUrl() const {
  return base_url_.Resolve(kDriveV2FilesUrl);
}

GURL DriveApiUrlGenerator::GetFilesPatchUrl(const std::string& file_id,
                                            bool set_modified_date,
                                            bool update_viewed_date) const {
  GURL url =
      base_url_.Resolve(kDriveV2FileUrlPrefix + net::EscapePath(file_id));

  // setModifiedDate is "false" by default.
  if (set_modified_date)
    url = net::AppendOrReplaceQueryParameter(url, "setModifiedDate", "true");

  // updateViewedDate is "true" by default.
  if (!update_viewed_date)
    url = net::AppendOrReplaceQueryParameter(url, "updateViewedDate", "false");

  return url;
}

GURL DriveApiUrlGenerator::GetFilesCopyUrl(const std::string& file_id) const {
  return base_url_.Resolve(base::StringPrintf(
      kDriveV2FileCopyUrlFormat, net::EscapePath(file_id).c_str()));
}

GURL DriveApiUrlGenerator::GetFilesListUrl(int max_results,
                                           const std::string& page_token,
                                           const std::string& q) const {
  GURL url = base_url_.Resolve(kDriveV2FilesUrl);

  // maxResults is 100 by default.
  if (max_results != 100) {
    url = net::AppendOrReplaceQueryParameter(
        url, "maxResults", base::IntToString(max_results));
  }

  if (!page_token.empty())
    url = net::AppendOrReplaceQueryParameter(url, "pageToken", page_token);

  if (!q.empty())
    url = net::AppendOrReplaceQueryParameter(url, "q", q);

  return url;
}

GURL DriveApiUrlGenerator::GetFilesDeleteUrl(const std::string& file_id) const {
  return base_url_.Resolve(base::StringPrintf(
      kDriveV2FileDeleteUrlFormat, net::EscapePath(file_id).c_str()));
}

GURL DriveApiUrlGenerator::GetFilesTrashUrl(const std::string& file_id) const {
  return base_url_.Resolve(base::StringPrintf(
      kDriveV2FileTrashUrlFormat, net::EscapePath(file_id).c_str()));
}

GURL DriveApiUrlGenerator::GetChangesListUrl(bool include_deleted,
                                             int max_results,
                                             const std::string& page_token,
                                             int64 start_change_id) const {
  DCHECK_GE(start_change_id, 0);

  GURL url = base_url_.Resolve(kDriveV2ChangelistUrl);

  // includeDeleted is "true" by default.
  if (!include_deleted)
    url = net::AppendOrReplaceQueryParameter(url, "includeDeleted", "false");

  // maxResults is "100" by default.
  if (max_results != 100) {
    url = net::AppendOrReplaceQueryParameter(
        url, "maxResults", base::IntToString(max_results));
  }

  if (!page_token.empty())
    url = net::AppendOrReplaceQueryParameter(url, "pageToken", page_token);

  if (start_change_id > 0)
    url = net::AppendOrReplaceQueryParameter(
        url, "startChangeId", base::Int64ToString(start_change_id));

  return url;
}

GURL DriveApiUrlGenerator::GetChildrenInsertUrl(
    const std::string& file_id) const {
  return base_url_.Resolve(base::StringPrintf(
      kDriveV2ChildrenUrlFormat, net::EscapePath(file_id).c_str()));
}

GURL DriveApiUrlGenerator::GetChildrenDeleteUrl(
    const std::string& child_id, const std::string& folder_id) const {
  return base_url_.Resolve(
      base::StringPrintf(kDriveV2ChildrenUrlForRemovalFormat,
                         net::EscapePath(folder_id).c_str(),
                         net::EscapePath(child_id).c_str()));
}

GURL DriveApiUrlGenerator::GetInitiateUploadNewFileUrl(
    bool set_modified_date) const {
  GURL url = AddResumableUploadParam(
      base_url_.Resolve(kDriveV2InitiateUploadNewFileUrl));

  // setModifiedDate is "false" by default.
  if (set_modified_date)
    url = net::AppendOrReplaceQueryParameter(url, "setModifiedDate", "true");

  return url;
}

GURL DriveApiUrlGenerator::GetInitiateUploadExistingFileUrl(
    const std::string& resource_id,
    bool set_modified_date) const {
  GURL url = base_url_.Resolve(
      kDriveV2InitiateUploadExistingFileUrlPrefix +
      net::EscapePath(resource_id));
  url = AddResumableUploadParam(url);

  // setModifiedDate is "false" by default.
  if (set_modified_date)
    url = net::AppendOrReplaceQueryParameter(url, "setModifiedDate", "true");

  return url;
}

GURL DriveApiUrlGenerator::GenerateDownloadFileUrl(
    const std::string& resource_id) const {
  return base_download_url_.Resolve(net::EscapePath(resource_id));
}

GURL DriveApiUrlGenerator::GetPermissionsInsertUrl(
    const std::string& resource_id) const {
  return base_url_.Resolve(
      base::StringPrintf(kDriveV2PermissionsUrlFormat,
                         net::EscapePath(resource_id).c_str()));
}

}  // namespace google_apis
