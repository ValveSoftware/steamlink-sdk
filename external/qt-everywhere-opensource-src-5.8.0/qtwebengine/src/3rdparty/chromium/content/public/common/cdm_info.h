// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_PUBLIC_COMMON_CDM_INFO_H_
#define CONTENT_PUBLIC_COMMON_CDM_INFO_H_

#include <string>
#include <vector>

#include "base/files/file_path.h"
#include "base/version.h"
#include "content/common/content_export.h"

namespace content {

// Represents a Content Decryption Module implementation and its capabilities.
struct CONTENT_EXPORT CdmInfo {
  CdmInfo(const std::string& type,
          const base::Version& version,
          const base::FilePath& path,
          const std::vector<std::string>& supported_codecs);
  CdmInfo(const CdmInfo& other);
  ~CdmInfo();

  // Type of the CDM (e.g. Widevine).
  std::string type;

  // Version of the CDM. May be empty if the version is not known.
  base::Version version;

  // Path to the library implementing the CDM. May be empty if the
  // CDM is not a separate library (e.g. Widevine on Android).
  base::FilePath path;

  // List of codecs supported by the CDM (e.g. vp8).
  // TODO(jrummell): use the enums from media::AudioCodec and media::VideoCodec
  // instead of strings.
  std::vector<std::string> supported_codecs;
};

}  // namespace content

#endif  // CONTENT_PUBLIC_COMMON_CDM_INFO_H_
