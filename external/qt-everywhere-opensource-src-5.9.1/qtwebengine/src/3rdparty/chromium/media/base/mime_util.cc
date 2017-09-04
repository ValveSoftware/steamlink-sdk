// Copyright 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "media/base/mime_util.h"

#include "base/lazy_instance.h"
#include "media/base/mime_util_internal.h"

namespace media {

// This variable is Leaky because it is accessed from WorkerPool threads.
static base::LazyInstance<internal::MimeUtil>::Leaky g_media_mime_util =
    LAZY_INSTANCE_INITIALIZER;

bool IsSupportedMediaMimeType(const std::string& mime_type) {
  return g_media_mime_util.Pointer()->IsSupportedMediaMimeType(mime_type);
}

SupportsType IsSupportedMediaFormat(const std::string& mime_type,
                                    const std::vector<std::string>& codecs) {
  return g_media_mime_util.Pointer()->IsSupportedMediaFormat(mime_type, codecs,
                                                             false);
}

SupportsType IsSupportedEncryptedMediaFormat(
    const std::string& mime_type,
    const std::vector<std::string>& codecs) {
  return g_media_mime_util.Pointer()->IsSupportedMediaFormat(mime_type, codecs,
                                                             true);
}

void ParseCodecString(const std::string& codecs,
                      std::vector<std::string>* codecs_out,
                      bool strip) {
  g_media_mime_util.Pointer()->ParseCodecString(codecs, codecs_out, strip);
}

void RemoveProprietaryMediaTypesAndCodecsForTests() {
  g_media_mime_util.Pointer()->RemoveProprietaryMediaTypesAndCodecs();
}

}  // namespace media
