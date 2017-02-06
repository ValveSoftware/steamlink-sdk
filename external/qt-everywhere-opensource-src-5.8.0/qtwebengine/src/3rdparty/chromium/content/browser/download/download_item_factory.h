// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
//
// The DownloadItemFactory is used to produce different DownloadItems.
// It is separate from the DownloadManager to allow download manager
// unit tests to control the items produced.

#ifndef CONTENT_BROWSER_DOWNLOAD_DOWNLOAD_ITEM_FACTORY_H_
#define CONTENT_BROWSER_DOWNLOAD_DOWNLOAD_ITEM_FACTORY_H_

#include <stdint.h>

#include <memory>
#include <string>
#include <vector>

#include "content/public/browser/download_item.h"

class GURL;

namespace base {
class FilePath;
}

namespace net {
class BoundNetLog;
}

namespace content {

class DownloadItem;
class DownloadItemImpl;
class DownloadItemImplDelegate;
class DownloadRequestHandleInterface;
struct DownloadCreateInfo;

class DownloadItemFactory {
public:
  virtual ~DownloadItemFactory() {}

  virtual DownloadItemImpl* CreatePersistedItem(
      DownloadItemImplDelegate* delegate,
      const std::string& guid,
      uint32_t download_id,
      const base::FilePath& current_path,
      const base::FilePath& target_path,
      const std::vector<GURL>& url_chain,
      const GURL& referrer_url,
      const GURL& site_url,
      const GURL& tab_url,
      const GURL& tab_refererr_url,
      const std::string& mime_type,
      const std::string& original_mime_type,
      const base::Time& start_time,
      const base::Time& end_time,
      const std::string& etag,
      const std::string& last_modified,
      int64_t received_bytes,
      int64_t total_bytes,
      const std::string& hash,
      DownloadItem::DownloadState state,
      DownloadDangerType danger_type,
      DownloadInterruptReason interrupt_reason,
      bool opened,
      const net::BoundNetLog& bound_net_log) = 0;

  virtual DownloadItemImpl* CreateActiveItem(
      DownloadItemImplDelegate* delegate,
      uint32_t download_id,
      const DownloadCreateInfo& info,
      const net::BoundNetLog& bound_net_log) = 0;

  virtual DownloadItemImpl* CreateSavePageItem(
      DownloadItemImplDelegate* delegate,
      uint32_t download_id,
      const base::FilePath& path,
      const GURL& url,
      const std::string& mime_type,
      std::unique_ptr<DownloadRequestHandleInterface> request_handle,
      const net::BoundNetLog& bound_net_log) = 0;
};

}  // namespace content

#endif  // CONTENT_BROWSER_DOWNLOAD_DOWNLOAD_ITEM_FACTORY_H_
