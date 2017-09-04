// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "modules/mediasession/MediaImage.h"

#include "core/dom/ExecutionContext.h"
#include "core/inspector/ConsoleMessage.h"
#include "modules/mediasession/MediaImageInit.h"
#include "platform/weborigin/KURL.h"
#include "wtf/text/StringOperators.h"

namespace blink {

// static
MediaImage* MediaImage::create(ExecutionContext* context,
                               const MediaImageInit& image) {
  return new MediaImage(context, image);
}

MediaImage::MediaImage(ExecutionContext* context, const MediaImageInit& image) {
  m_src = context->completeURL(image.src());
  if (!KURL(ParsedURLString, m_src).isValid()) {
    context->addConsoleMessage(
        ConsoleMessage::create(JSMessageSource, WarningMessageLevel,
                               "MediaImage src is invalid: " + image.src()));
  }
  m_sizes = image.sizes();
  m_type = image.type();
}

String MediaImage::src() const {
  return m_src;
}

String MediaImage::sizes() const {
  return m_sizes;
}

String MediaImage::type() const {
  return m_type;
}

}  // namespace blink
