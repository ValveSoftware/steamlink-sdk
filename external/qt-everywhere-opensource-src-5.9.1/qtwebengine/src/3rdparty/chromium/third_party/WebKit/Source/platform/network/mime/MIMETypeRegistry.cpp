// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "platform/MIMETypeRegistry.h"

#include "base/files/file_path.h"
#include "base/strings/string_util.h"
#include "components/mime_util/mime_util.h"
#include "media/base/mime_util.h"
#include "media/filters/stream_parser_factory.h"
#include "net/base/mime_util.h"
#include "public/platform/FilePathConversion.h"
#include "public/platform/InterfaceProvider.h"
#include "public/platform/Platform.h"
#include "public/platform/mime_registry.mojom-blink.h"
#include "wtf/text/WTFString.h"

namespace blink {

namespace {

struct MimeRegistryPtrHolder {
 public:
  MimeRegistryPtrHolder() {
    Platform::current()->interfaceProvider()->getInterface(
        mojo::GetProxy(&mimeRegistry));
  }
  ~MimeRegistryPtrHolder() {}

  mojom::blink::MimeRegistryPtr mimeRegistry;
};

std::string ToASCIIOrEmpty(const WebString& string) {
  return string.containsOnlyASCII() ? string.ascii() : std::string();
}

template <typename CHARTYPE, typename SIZETYPE>
std::string ToLowerASCIIInternal(CHARTYPE* str, SIZETYPE length) {
  std::string lowerASCII;
  lowerASCII.reserve(length);
  for (CHARTYPE* p = str; p < str + length; p++)
    lowerASCII.push_back(base::ToLowerASCII(static_cast<char>(*p)));
  return lowerASCII;
}

// Does the same as ToASCIIOrEmpty, but also makes the chars lower.
std::string ToLowerASCIIOrEmpty(const String& str) {
  if (str.isEmpty() || !str.containsOnlyASCII())
    return std::string();
  if (str.is8Bit())
    return ToLowerASCIIInternal(str.characters8(), str.length());
  return ToLowerASCIIInternal(str.characters16(), str.length());
}

#define STATIC_ASSERT_ENUM(a, b)                            \
  static_assert(static_cast<int>(a) == static_cast<int>(b), \
                "mismatching enums: " #a)
STATIC_ASSERT_ENUM(MIMETypeRegistry::IsNotSupported, media::IsNotSupported);
STATIC_ASSERT_ENUM(MIMETypeRegistry::IsSupported, media::IsSupported);
STATIC_ASSERT_ENUM(MIMETypeRegistry::MayBeSupported, media::MayBeSupported);

}  // namespace

String MIMETypeRegistry::getMIMETypeForExtension(const String& ext) {
  // The sandbox restricts our access to the registry, so we need to proxy
  // these calls over to the browser process.
  DEFINE_STATIC_LOCAL(MimeRegistryPtrHolder, registryHolder, ());
  String mimeType;
  if (!registryHolder.mimeRegistry->GetMimeTypeFromExtension(ext, &mimeType))
    return String();
  return mimeType;
}

String MIMETypeRegistry::getWellKnownMIMETypeForExtension(const String& ext) {
  // This method must be thread safe and should not consult the OS/registry.
  std::string mimeType;
  net::GetWellKnownMimeTypeFromExtension(WebStringToFilePath(ext).value(),
                                         &mimeType);
  return String::fromUTF8(mimeType.data(), mimeType.length());
}

String MIMETypeRegistry::getMIMETypeForPath(const String& path) {
  int pos = path.reverseFind('.');
  if (pos < 0)
    return "application/octet-stream";
  String extension = path.substring(pos + 1);
  String mimeType = getMIMETypeForExtension(extension);
  return mimeType.isEmpty() ? "application/octet-stream" : mimeType;
}

bool MIMETypeRegistry::isSupportedMIMEType(const String& mimeType) {
  return mime_util::IsSupportedMimeType(ToLowerASCIIOrEmpty(mimeType));
}

bool MIMETypeRegistry::isSupportedImageMIMEType(const String& mimeType) {
  return mime_util::IsSupportedImageMimeType(ToLowerASCIIOrEmpty(mimeType));
}

bool MIMETypeRegistry::isSupportedImageResourceMIMEType(
    const String& mimeType) {
  return isSupportedImageMIMEType(mimeType);
}

bool MIMETypeRegistry::isSupportedImagePrefixedMIMEType(
    const String& mimeType) {
  std::string asciiMimeType = ToLowerASCIIOrEmpty(mimeType);
  return (mime_util::IsSupportedImageMimeType(asciiMimeType) ||
          (base::StartsWith(asciiMimeType, "image/",
                            base::CompareCase::SENSITIVE) &&
           mime_util::IsSupportedNonImageMimeType(asciiMimeType)));
}

bool MIMETypeRegistry::isSupportedImageMIMETypeForEncoding(
    const String& mimeType) {
  if (equalIgnoringCase(mimeType, "image/jpeg") ||
      equalIgnoringCase(mimeType, "image/png"))
    return true;
  if (equalIgnoringCase(mimeType, "image/webp"))
    return true;
  return false;
}

bool MIMETypeRegistry::isSupportedJavaScriptMIMEType(const String& mimeType) {
  return mime_util::IsSupportedJavascriptMimeType(
      ToLowerASCIIOrEmpty(mimeType));
}

bool MIMETypeRegistry::isSupportedNonImageMIMEType(const String& mimeType) {
  return mime_util::IsSupportedNonImageMimeType(ToLowerASCIIOrEmpty(mimeType));
}

bool MIMETypeRegistry::isSupportedMediaMIMEType(const String& mimeType,
                                                const String& codecs) {
  return supportsMediaMIMEType(mimeType, codecs) != IsNotSupported;
}

MIMETypeRegistry::SupportsType MIMETypeRegistry::supportsMediaMIMEType(
    const String& mimeType,
    const String& codecs) {
  const std::string asciiMimeType = ToLowerASCIIOrEmpty(mimeType);
  std::vector<std::string> codecVector;
  media::ParseCodecString(ToASCIIOrEmpty(codecs), &codecVector, false);
  return static_cast<SupportsType>(
      media::IsSupportedMediaFormat(asciiMimeType, codecVector));
}

bool MIMETypeRegistry::isSupportedMediaSourceMIMEType(const String& mimeType,
                                                      const String& codecs) {
  const std::string asciiMimeType = ToLowerASCIIOrEmpty(mimeType);
  if (asciiMimeType.empty())
    return false;
  std::vector<std::string> parsedCodecIds;
  media::ParseCodecString(ToASCIIOrEmpty(codecs), &parsedCodecIds, false);
  return static_cast<MIMETypeRegistry::SupportsType>(
      media::StreamParserFactory::IsTypeSupported(asciiMimeType,
                                                  parsedCodecIds));
}

bool MIMETypeRegistry::isJavaAppletMIMEType(const String& mimeType) {
  // Since this set is very limited and is likely to remain so we won't bother
  // with the overhead of using a hash set.  Any of the MIME types below may be
  // followed by any number of specific versions of the JVM, which is why we use
  // startsWith()
  return mimeType.startsWith("application/x-java-applet",
                             TextCaseInsensitive) ||
         mimeType.startsWith("application/x-java-bean", TextCaseInsensitive) ||
         mimeType.startsWith("application/x-java-vm", TextCaseInsensitive);
}

bool MIMETypeRegistry::isSupportedStyleSheetMIMEType(const String& mimeType) {
  return equalIgnoringCase(mimeType, "text/css");
}

bool MIMETypeRegistry::isSupportedFontMIMEType(const String& mimeType) {
  static const unsigned fontLen = 5;
  if (!mimeType.startsWith("font/", TextCaseInsensitive))
    return false;
  String subType = mimeType.substring(fontLen).lower();
  return subType == "woff" || subType == "woff2" || subType == "otf" ||
         subType == "ttf" || subType == "sfnt";
}

bool MIMETypeRegistry::isSupportedTextTrackMIMEType(const String& mimeType) {
  return equalIgnoringCase(mimeType, "text/vtt");
}

}  // namespace blink
