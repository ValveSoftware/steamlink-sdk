// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef MockMimeRegistry_h
#define MockMimeRegistry_h

#include "net/base/mime_util.h"
#include "public/platform/FilePathConversion.h"
#include "public/platform/mime_registry.mojom-blink.h"

namespace blink {

// Used for unit tests.
class MockMimeRegistry : public mojom::blink::MimeRegistry {
 public:
  MockMimeRegistry() = default;
  ~MockMimeRegistry() = default;

  void GetMimeTypeFromExtension(
      const String& ext,
      const GetMimeTypeFromExtensionCallback& callback) override {
    std::string mimeType;
    net::GetMimeTypeFromExtension(WebStringToFilePath(ext).value(), &mimeType);
    callback.Run(String::fromUTF8(mimeType.data(), mimeType.length()));
  }
};

}  // namespace blink

#endif  // MockMimeRegistry_h
