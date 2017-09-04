// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "modules/mediasession/MediaMetadataSanitizer.h"

#include "core/dom/ExecutionContext.h"
#include "core/inspector/ConsoleMessage.h"
#include "modules/mediasession/MediaImage.h"
#include "modules/mediasession/MediaMetadata.h"
#include "public/platform/WebIconSizesParser.h"
#include "public/platform/WebSize.h"
#include "url/url_constants.h"
#include "wtf/text/StringOperators.h"

namespace blink {

namespace {

// Constants used by the sanitizer, must be consistent with
// content::MediaMetdataSanitizer.

// Maximum length of all strings inside MediaMetadata when it is sent over mojo.
const size_t kMaxStringLength = 4 * 1024;

// Maximum type length of MediaImage, which conforms to RFC 4288
// (https://tools.ietf.org/html/rfc4288).
const size_t kMaxImageTypeLength = 2 * 127 + 1;

// Maximum number of MediaImages inside the MediaMetadata.
const size_t kMaxNumberOfMediaImages = 10;

// Maximum of sizes in a MediaImage.
const size_t kMaxNumberOfImageSizes = 10;

bool checkMediaImageSrcSanity(const KURL& src, ExecutionContext* context) {
  // Console warning for invalid src is printed upon MediaImage creation.
  if (!src.isValid())
    return false;

  if (!src.protocolIs(url::kHttpScheme) && !src.protocolIs(url::kHttpsScheme) &&
      !src.protocolIs(url::kDataScheme)) {
    context->addConsoleMessage(ConsoleMessage::create(
        JSMessageSource, WarningMessageLevel,
        "MediaImage src can only be of http/https/data scheme: " +
            src.getString()));
    return false;
  }
  DCHECK(src.getString().is8Bit());
  if (src.getString().length() > url::kMaxURLChars) {
    context->addConsoleMessage(ConsoleMessage::create(
        JSMessageSource, WarningMessageLevel,
        "MediaImage src exceeds maximum URL length: " + src.getString()));
    return false;
  }
  return true;
}

// Sanitize MediaImage and do mojo serialization. Returns null when
// |image.src()| is bad.
blink::mojom::blink::MediaImagePtr sanitizeMediaImageAndConvertToMojo(
    const MediaImage* image,
    ExecutionContext* context) {
  DCHECK(image);

  blink::mojom::blink::MediaImagePtr mojoImage;

  KURL url = KURL(ParsedURLString, image->src());
  if (!checkMediaImageSrcSanity(url, context))
    return mojoImage;

  mojoImage = blink::mojom::blink::MediaImage::New();
  mojoImage->src = url;
  mojoImage->type = image->type().left(kMaxImageTypeLength);
  for (const auto& webSize :
       WebIconSizesParser::parseIconSizes(image->sizes())) {
    mojoImage->sizes.append(webSize);
    if (mojoImage->sizes.size() == kMaxNumberOfImageSizes) {
      context->addConsoleMessage(ConsoleMessage::create(
          JSMessageSource, WarningMessageLevel,
          "The number of MediaImage sizes exceeds the upper limit. "
          "All remaining MediaImage will be ignored"));
      break;
    }
  }
  return mojoImage;
}

}  // anonymous namespace

blink::mojom::blink::MediaMetadataPtr
MediaMetadataSanitizer::sanitizeAndConvertToMojo(const MediaMetadata* metadata,
                                                 ExecutionContext* context) {
  blink::mojom::blink::MediaMetadataPtr mojoMetadata;
  if (!metadata)
    return mojoMetadata;

  mojoMetadata = blink::mojom::blink::MediaMetadata::New();

  mojoMetadata->title = metadata->title().left(kMaxStringLength);
  mojoMetadata->artist = metadata->artist().left(kMaxStringLength);
  mojoMetadata->album = metadata->album().left(kMaxStringLength);

  for (const auto image : metadata->artwork()) {
    blink::mojom::blink::MediaImagePtr mojoImage =
        sanitizeMediaImageAndConvertToMojo(image.get(), context);
    if (!mojoImage.is_null())
      mojoMetadata->artwork.append(std::move(mojoImage));
    if (mojoMetadata->artwork.size() == kMaxNumberOfMediaImages) {
      context->addConsoleMessage(ConsoleMessage::create(
          JSMessageSource, WarningMessageLevel,
          "The number of MediaImage sizes exceeds the upper limit. "
          "All remaining MediaImage will be ignored"));
      break;
    }
  }
  return mojoMetadata;
}

}  // namespace blink
