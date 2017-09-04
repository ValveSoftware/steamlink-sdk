// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/public/common/drop_data.h"

namespace content {

DropData::DropData()
    : did_originate_from_renderer(false),
      referrer_policy(blink::WebReferrerPolicyDefault),
      key_modifiers(0) {}

DropData::DropData(const DropData& other) = default;

DropData::~DropData() {
}

DropData::Metadata::Metadata() {}

// static
DropData::Metadata DropData::Metadata::CreateForMimeType(
    const Kind& kind,
    const base::string16& mime_type) {
  Metadata metadata;
  metadata.kind = kind;
  metadata.mime_type = mime_type;
  return metadata;
}

// static
DropData::Metadata DropData::Metadata::CreateForFilePath(
    const base::FilePath& filename) {
  Metadata metadata;
  metadata.kind = Kind::FILENAME;
  metadata.filename = filename;
  return metadata;
}

// static
DropData::Metadata DropData::Metadata::CreateForFileSystemUrl(
    const GURL& file_system_url) {
  Metadata metadata;
  metadata.kind = Kind::FILESYSTEMFILE;
  metadata.file_system_url = file_system_url;
  return metadata;
}

DropData::Metadata::Metadata(const DropData::Metadata& other) = default;

DropData::Metadata::~Metadata() {}

}  // namespace content
