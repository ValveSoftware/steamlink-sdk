// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/history/core/browser/download_row.h"

#include "components/history/core/browser/download_constants.h"

namespace history {

DownloadRow::DownloadRow()
    : received_bytes(0),
      total_bytes(0),
      state(DownloadState::IN_PROGRESS),
      danger_type(DownloadDangerType::NOT_DANGEROUS),
      interrupt_reason(0),
      id(kInvalidDownloadId),
      opened(false) {
  // |interrupt_reason| is left undefined by this constructor as the value
  // has no meaning unless |state| is equal to kStateInterrupted.
}

DownloadRow::DownloadRow(const base::FilePath& current_path,
                         const base::FilePath& target_path,
                         const std::vector<GURL>& url_chain,
                         const GURL& referrer_url,
                         const GURL& site_url,
                         const GURL& tab_url,
                         const GURL& tab_referrer_url,
                         const std::string& http_method,
                         const std::string& mime_type,
                         const std::string& original_mime_type,
                         const base::Time& start,
                         const base::Time& end,
                         const std::string& etag,
                         const std::string& last_modified,
                         int64_t received,
                         int64_t total,
                         DownloadState download_state,
                         DownloadDangerType danger_type,
                         DownloadInterruptReason interrupt_reason,
                         const std::string& hash,
                         DownloadId id,
                         const std::string& guid,
                         bool download_opened,
                         const std::string& ext_id,
                         const std::string& ext_name)
    : current_path(current_path),
      target_path(target_path),
      url_chain(url_chain),
      referrer_url(referrer_url),
      site_url(site_url),
      tab_url(tab_url),
      tab_referrer_url(tab_referrer_url),
      http_method(http_method),
      mime_type(mime_type),
      original_mime_type(original_mime_type),
      start_time(start),
      end_time(end),
      etag(etag),
      last_modified(last_modified),
      received_bytes(received),
      total_bytes(total),
      state(download_state),
      danger_type(danger_type),
      interrupt_reason(interrupt_reason),
      hash(hash),
      id(id),
      guid(guid),
      opened(download_opened),
      by_ext_id(ext_id),
      by_ext_name(ext_name) {}

DownloadRow::DownloadRow(const DownloadRow& other) = default;

DownloadRow::~DownloadRow() {}

bool DownloadRow::operator==(const DownloadRow& rhs) const {
  return current_path == rhs.current_path && target_path == rhs.target_path &&
         url_chain == rhs.url_chain && referrer_url == rhs.referrer_url &&
         site_url == rhs.site_url && tab_url == rhs.tab_url &&
         tab_referrer_url == rhs.tab_referrer_url &&
         http_method == rhs.http_method && mime_type == rhs.mime_type &&
         original_mime_type == rhs.original_mime_type &&
         start_time == rhs.start_time && end_time == rhs.end_time &&
         etag == rhs.etag && last_modified == rhs.last_modified &&
         received_bytes == rhs.received_bytes &&
         total_bytes == rhs.total_bytes && state == rhs.state &&
         danger_type == rhs.danger_type &&
         interrupt_reason == rhs.interrupt_reason && hash == rhs.hash &&
         id == rhs.id && guid == rhs.guid && opened == rhs.opened &&
         by_ext_id == rhs.by_ext_id && by_ext_name == rhs.by_ext_name;
}

}  // namespace history
