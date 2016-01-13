// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "webkit/common/resource_type.h"

#include "base/logging.h"

using blink::WebURLRequest;

// static
ResourceType::Type ResourceType::FromTargetType(
    WebURLRequest::TargetType type) {
  switch (type) {
    case WebURLRequest::TargetIsMainFrame:
      return ResourceType::MAIN_FRAME;
    case WebURLRequest::TargetIsSubframe:
      return ResourceType::SUB_FRAME;
    case WebURLRequest::TargetIsSubresource:
      return ResourceType::SUB_RESOURCE;
    case WebURLRequest::TargetIsStyleSheet:
      return ResourceType::STYLESHEET;
    case WebURLRequest::TargetIsScript:
      return ResourceType::SCRIPT;
    case WebURLRequest::TargetIsFontResource:
      return ResourceType::FONT_RESOURCE;
    case WebURLRequest::TargetIsImage:
      return ResourceType::IMAGE;
    case WebURLRequest::TargetIsObject:
      return ResourceType::OBJECT;
    case WebURLRequest::TargetIsMedia:
      return ResourceType::MEDIA;
    case WebURLRequest::TargetIsWorker:
      return ResourceType::WORKER;
    case WebURLRequest::TargetIsSharedWorker:
      return ResourceType::SHARED_WORKER;
    case WebURLRequest::TargetIsPrefetch:
      return ResourceType::PREFETCH;
    case WebURLRequest::TargetIsFavicon:
      return ResourceType::FAVICON;
    case WebURLRequest::TargetIsXHR:
      return ResourceType::XHR;
    case WebURLRequest::TargetIsPing:
      return ResourceType::PING;
    case WebURLRequest::TargetIsServiceWorker:
      return ResourceType::SERVICE_WORKER;
    default:
      NOTREACHED();
      return ResourceType::SUB_RESOURCE;
  }
}
