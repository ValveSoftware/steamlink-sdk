// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CacheStorageError_h
#define CacheStorageError_h

#include "public/platform/modules/serviceworker/WebServiceWorkerCacheError.h"
#include "wtf/Allocator.h"
#include "wtf/Noncopyable.h"

namespace blink {

class DOMException;
class ScriptPromiseResolver;

class CacheStorageError {
  STATIC_ONLY(CacheStorageError);

 public:
  // For CallbackPromiseAdapter. Ownership of a given error is not
  // transferred.
  using WebType = WebServiceWorkerCacheError;
  static DOMException* take(ScriptPromiseResolver*,
                            WebServiceWorkerCacheError webError) {
    return createException(webError);
  }

  static DOMException* createException(WebServiceWorkerCacheError webError);
};

}  // namespace blink

#endif  // CacheStorageError_h
