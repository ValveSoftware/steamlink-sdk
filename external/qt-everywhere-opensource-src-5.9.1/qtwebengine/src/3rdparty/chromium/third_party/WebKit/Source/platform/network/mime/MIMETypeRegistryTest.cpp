// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "platform/MIMETypeRegistry.h"

#include "testing/gtest/include/gtest/gtest.h"

namespace blink {

TEST(MIMETypeRegistryTest, MimeTypeTest) {
  EXPECT_TRUE(MIMETypeRegistry::isSupportedImagePrefixedMIMEType("image/gif"));
  EXPECT_TRUE(MIMETypeRegistry::isSupportedImageResourceMIMEType("image/gif"));
  EXPECT_TRUE(MIMETypeRegistry::isSupportedImagePrefixedMIMEType("Image/Gif"));
  EXPECT_TRUE(MIMETypeRegistry::isSupportedImageResourceMIMEType("Image/Gif"));
  static const UChar upper16[] = {0x0049, 0x006d, 0x0061, 0x0067,
                                  0x0065, 0x002f, 0x0067, 0x0069,
                                  0x0066, 0};  // Image/gif in UTF16
  EXPECT_TRUE(
      MIMETypeRegistry::isSupportedImagePrefixedMIMEType(String(upper16)));
  EXPECT_TRUE(
      MIMETypeRegistry::isSupportedImagePrefixedMIMEType("image/svg+xml"));
  EXPECT_FALSE(
      MIMETypeRegistry::isSupportedImageResourceMIMEType("image/svg+xml"));
}

TEST(MIMETypeRegistryTest, PluginMimeTypes) {
  // Since we've removed MIME type guessing based on plugin-declared file
  // extensions, ensure that the MIMETypeRegistry already contains
  // the extensions used by common PPAPI plugins.
  EXPECT_EQ("application/pdf",
            MIMETypeRegistry::getWellKnownMIMETypeForExtension("pdf").utf8());
  EXPECT_EQ("application/x-shockwave-flash",
            MIMETypeRegistry::getWellKnownMIMETypeForExtension("swf").utf8());
}

}  // namespace blink
