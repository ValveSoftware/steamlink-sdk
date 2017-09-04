// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "modules/cachestorage/CacheStorageError.h"

#include "core/dom/DOMException.h"
#include "core/dom/ExceptionCode.h"
#include "modules/cachestorage/Cache.h"
#include "public/platform/modules/serviceworker/WebServiceWorkerCacheError.h"

namespace blink {

DOMException* CacheStorageError::createException(
    WebServiceWorkerCacheError webError) {
  switch (webError) {
    case WebServiceWorkerCacheErrorNotImplemented:
      return DOMException::create(NotSupportedError,
                                  "Method is not implemented.");
    case WebServiceWorkerCacheErrorNotFound:
      return DOMException::create(NotFoundError, "Entry was not found.");
    case WebServiceWorkerCacheErrorExists:
      return DOMException::create(InvalidAccessError, "Entry already exists.");
    case WebServiceWorkerCacheErrorQuotaExceeded:
      return DOMException::create(QuotaExceededError, "Quota exceeded.");
    case WebServiceWorkerCacheErrorCacheNameNotFound:
      return DOMException::create(NotFoundError, "Cache was not found.");
    case WebServiceWorkerCacheErrorTooLarge:
      return DOMException::create(AbortError, "Operation too large.");
  }
  NOTREACHED();
  return nullptr;
}

}  // namespace blink
