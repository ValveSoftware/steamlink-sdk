// Copyright (c) 2010 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/browser/download/save_types.h"

#include "base/files/file_path.h"

namespace content {

SaveFileCreateInfo::SaveFileCreateInfo(const base::FilePath& path,
                                       const GURL& url,
                                       SaveItemId save_item_id,
                                       SavePackageId save_package_id,
                                       int render_process_id,
                                       int render_frame_routing_id,
                                       SaveFileSource save_source)
    : path(path),
      url(url),
      save_item_id(save_item_id),
      save_package_id(save_package_id),
      render_process_id(render_process_id),
      render_frame_routing_id(render_frame_routing_id),
      request_id(-1),
      total_bytes(0),
      save_source(save_source) {}

SaveFileCreateInfo::SaveFileCreateInfo(const GURL& url,
                                       const GURL& final_url,
                                       SaveItemId save_item_id,
                                       SavePackageId save_package_id,
                                       int render_process_id,
                                       int render_frame_routing_id,
                                       int request_id,
                                       const std::string& content_disposition,
                                       int64_t total_bytes)
    : url(url),
      final_url(final_url),
      save_item_id(save_item_id),
      save_package_id(save_package_id),
      render_process_id(render_process_id),
      render_frame_routing_id(render_frame_routing_id),
      request_id(request_id),
      content_disposition(content_disposition),
      total_bytes(total_bytes),
      save_source(SaveFileCreateInfo::SAVE_FILE_FROM_NET) {}

SaveFileCreateInfo::SaveFileCreateInfo(const SaveFileCreateInfo& other) =
    default;

SaveFileCreateInfo::~SaveFileCreateInfo() {}

}  // namespace content
