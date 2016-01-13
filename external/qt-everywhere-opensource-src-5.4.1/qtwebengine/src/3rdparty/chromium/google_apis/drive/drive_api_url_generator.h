// Copyright (c) 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef GOOGLE_APIS_DRIVE_DRIVE_API_URL_GENERATOR_H_
#define GOOGLE_APIS_DRIVE_DRIVE_API_URL_GENERATOR_H_

#include <string>

#include "url/gurl.h"

namespace google_apis {

// This class is used to generate URLs for communicating with drive api
// servers for production, and a local server for testing.
class DriveApiUrlGenerator {
 public:
  // |base_url| is the path to the target drive api server.
  // Note that this is an injecting point for a testing server.
  DriveApiUrlGenerator(const GURL& base_url, const GURL& base_download_url);
  ~DriveApiUrlGenerator();

  // The base URL for communicating with the production drive api server.
  static const char kBaseUrlForProduction[];

  // The base URL for the file download server for production.
  static const char kBaseDownloadUrlForProduction[];

  // Returns a URL to invoke "About: get" method.
  GURL GetAboutGetUrl() const;

  // Returns a URL to invoke "Apps: list" method.
  // Set |use_internal_endpoint| to true if official Chrome's API key is used
  // and retrieving more information (related to App uninstall) is necessary.
  GURL GetAppsListUrl(bool use_internal_endpoint) const;

  // Returns a URL to uninstall an app with the give |app_id|.
  GURL GetAppsDeleteUrl(const std::string& app_id) const;

  // Returns a URL to fetch a file metadata.
  GURL GetFilesGetUrl(const std::string& file_id) const;

  // Returns a URL to authorize an app to access a file.
  GURL GetFilesAuthorizeUrl(const std::string& file_id,
                            const std::string& app_id) const;

  // Returns a URL to create a resource.
  GURL GetFilesInsertUrl() const;

  // Returns a URL to patch file metadata.
  GURL GetFilesPatchUrl(const std::string& file_id,
                        bool set_modified_date,
                        bool update_viewed_date) const;

  // Returns a URL to copy a resource specified by |file_id|.
  GURL GetFilesCopyUrl(const std::string& file_id) const;

  // Returns a URL to fetch file list.
  GURL GetFilesListUrl(int max_results,
                       const std::string& page_token,
                       const std::string& q) const;

  // Returns a URL to delete a resource with the given |file_id|.
  GURL GetFilesDeleteUrl(const std::string& file_id) const;

  // Returns a URL to trash a resource with the given |file_id|.
  GURL GetFilesTrashUrl(const std::string& file_id) const;

  // Returns a URL to fetch a list of changes.
  GURL GetChangesListUrl(bool include_deleted,
                         int max_results,
                         const std::string& page_token,
                         int64 start_change_id) const;

  // Returns a URL to add a resource to a directory with |folder_id|.
  GURL GetChildrenInsertUrl(const std::string& folder_id) const;

  // Returns a URL to remove a resource with |child_id| from a directory
  // with |folder_id|.
  GURL GetChildrenDeleteUrl(const std::string& child_id,
                            const std::string& folder_id) const;

  // Returns a URL to initiate uploading a new file.
  GURL GetInitiateUploadNewFileUrl(bool set_modified_date) const;

  // Returns a URL to initiate uploading an existing file specified by
  // |resource_id|.
  GURL GetInitiateUploadExistingFileUrl(const std::string& resource_id,
                                        bool set_modified_date) const;

  // Generates a URL for downloading a file.
  GURL GenerateDownloadFileUrl(const std::string& resource_id) const;

  // Generates a URL for adding permissions.
  GURL GetPermissionsInsertUrl(const std::string& resource_id) const;

 private:
  const GURL base_url_;
  const GURL base_download_url_;

  // This class is copyable hence no DISALLOW_COPY_AND_ASSIGN here.
};

}  // namespace google_apis

#endif  // GOOGLE_APIS_DRIVE_DRIVE_API_URL_GENERATOR_H_
