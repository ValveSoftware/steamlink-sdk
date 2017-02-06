// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/common/resource_request_body_impl.h"

#include "base/strings/utf_string_conversions.h"
#include "content/common/page_state_serialization.h"

using blink::WebHTTPBody;
using blink::WebString;

namespace content {

ResourceRequestBodyImpl::ResourceRequestBodyImpl() : identifier_(0) {}

void ResourceRequestBodyImpl::AppendBytes(const char* bytes, int bytes_len) {
  if (bytes_len > 0) {
    elements_.push_back(Element());
    elements_.back().SetToBytes(bytes, bytes_len);
  }
}

void ResourceRequestBodyImpl::AppendFileRange(
    const base::FilePath& file_path,
    uint64_t offset,
    uint64_t length,
    const base::Time& expected_modification_time) {
  elements_.push_back(Element());
  elements_.back().SetToFilePathRange(file_path, offset, length,
                                      expected_modification_time);
}

void ResourceRequestBodyImpl::AppendBlob(const std::string& uuid) {
  elements_.push_back(Element());
  elements_.back().SetToBlob(uuid);
}

void ResourceRequestBodyImpl::AppendFileSystemFileRange(
    const GURL& url,
    uint64_t offset,
    uint64_t length,
    const base::Time& expected_modification_time) {
  elements_.push_back(Element());
  elements_.back().SetToFileSystemUrlRange(url, offset, length,
                                           expected_modification_time);
}

std::vector<base::FilePath> ResourceRequestBodyImpl::GetReferencedFiles()
    const {
  std::vector<base::FilePath> result;
  for (const auto& element : *elements()) {
    if (element.type() == Element::TYPE_FILE)
      result.push_back(element.path());
  }
  return result;
}

ResourceRequestBodyImpl::~ResourceRequestBodyImpl() {}

}  // namespace content
