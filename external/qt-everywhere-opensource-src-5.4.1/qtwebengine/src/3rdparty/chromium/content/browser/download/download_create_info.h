// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_BROWSER_DOWNLOAD_DOWNLOAD_CREATE_INFO_H_
#define CONTENT_BROWSER_DOWNLOAD_DOWNLOAD_CREATE_INFO_H_

#include <string>
#include <vector>

#include "base/basictypes.h"
#include "base/files/file_path.h"
#include "base/time/time.h"
#include "content/browser/download/download_file.h"
#include "content/browser/download/download_request_handle.h"
#include "content/common/content_export.h"
#include "content/public/browser/download_save_info.h"
#include "content/public/common/page_transition_types.h"
#include "net/base/net_log.h"
#include "url/gurl.h"

namespace content {

// Used for informing the download manager of a new download, since we don't
// want to pass |DownloadItem|s between threads.
struct CONTENT_EXPORT DownloadCreateInfo {
  DownloadCreateInfo(const base::Time& start_time,
                     int64 total_bytes,
                     const net::BoundNetLog& bound_net_log,
                     bool has_user_gesture,
                     PageTransition transition_type,
                     scoped_ptr<DownloadSaveInfo> save_info);
  DownloadCreateInfo();
  ~DownloadCreateInfo();

  std::string DebugString() const;

  // The URL from which we are downloading. This is the final URL after any
  // redirection by the server for |url_chain|.
  const GURL& url() const;

  // The chain of redirects that leading up to and including the final URL.
  std::vector<GURL> url_chain;

  // The URL that referred us.
  GURL referrer_url;

  // The URL of the tab that started us.
  GURL tab_url;

  // The referrer URL of the tab that started us.
  GURL tab_referrer_url;

  // The time when the download started.
  base::Time start_time;

  // The total download size.
  int64 total_bytes;

  // The ID of the download.
  uint32 download_id;

  // True if the download was initiated by user action.
  bool has_user_gesture;

  PageTransition transition_type;

  // The content-disposition string from the response header.
  std::string content_disposition;

  // The mime type string from the response header (may be overridden).
  std::string mime_type;

  // The value of the content type header sent with the downloaded item.  It
  // may be different from |mime_type|, which may be set based on heuristics
  // which may look at the file extension and first few bytes of the file.
  std::string original_mime_type;

  // For continuing a download, the modification time of the file.
  // Storing as a string for exact match to server format on
  // "If-Unmodified-Since" comparison.
  std::string last_modified;

  // For continuing a download, the ETAG of the file.
  std::string etag;

  // The download file save info.
  scoped_ptr<DownloadSaveInfo> save_info;

  // The remote IP address where the download was fetched from.  Copied from
  // UrlRequest::GetSocketAddress().
  std::string remote_address;

  // The handle to the URLRequest sourcing this download.
  DownloadRequestHandle request_handle;

  // The request's |BoundNetLog|, for "source_dependency" linking with the
  // download item's.
  const net::BoundNetLog request_bound_net_log;

 private:
  DISALLOW_COPY_AND_ASSIGN(DownloadCreateInfo);
};

}  // namespace content

#endif  // CONTENT_BROWSER_DOWNLOAD_DOWNLOAD_CREATE_INFO_H_
