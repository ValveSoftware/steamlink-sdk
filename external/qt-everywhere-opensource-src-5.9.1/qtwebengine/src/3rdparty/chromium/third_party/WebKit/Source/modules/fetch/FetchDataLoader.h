// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef FetchDataLoader_h
#define FetchDataLoader_h

#include "core/dom/DOMArrayBuffer.h"
#include "core/streams/Stream.h"
#include "modules/ModulesExport.h"
#include "platform/blob/BlobData.h"
#include "platform/heap/Handle.h"
#include "wtf/Forward.h"

namespace blink {

class BytesConsumer;

// FetchDataLoader subclasses
// 1. take a BytesConsumer,
// 2. read all data, and
// 3. call either didFetchDataLoaded...() on success or
//    difFetchDataLoadFailed() otherwise
//    on the thread where FetchDataLoader is created.
//
// - Client's methods can be called synchronously in start().
// - If FetchDataLoader::cancel() is called, Client's methods will not be
//   called anymore.
class MODULES_EXPORT FetchDataLoader
    : public GarbageCollectedFinalized<FetchDataLoader> {
 public:
  class MODULES_EXPORT Client : public GarbageCollectedMixin {
   public:
    virtual ~Client() {}

    // The method corresponding to createLoaderAs... is called on success.
    virtual void didFetchDataLoadedBlobHandle(PassRefPtr<BlobDataHandle>) {
      ASSERT_NOT_REACHED();
    }
    virtual void didFetchDataLoadedArrayBuffer(DOMArrayBuffer*) {
      ASSERT_NOT_REACHED();
    }
    virtual void didFetchDataLoadedString(const String&) {
      ASSERT_NOT_REACHED();
    }
    // This is called after all data are read from |handle| and written
    // to |outStream|, and |outStream| is closed or aborted.
    virtual void didFetchDataLoadedStream() { ASSERT_NOT_REACHED(); }

    virtual void didFetchDataLoadFailed() = 0;

    DEFINE_INLINE_VIRTUAL_TRACE() {}
  };

  static FetchDataLoader* createLoaderAsBlobHandle(const String& mimeType);
  static FetchDataLoader* createLoaderAsArrayBuffer();
  static FetchDataLoader* createLoaderAsString();
  static FetchDataLoader* createLoaderAsStream(Stream* outStream);

  virtual ~FetchDataLoader() {}

  // |consumer| must not have a client when called.
  virtual void start(BytesConsumer* /* consumer */, Client*) = 0;

  virtual void cancel() = 0;

  DEFINE_INLINE_VIRTUAL_TRACE() {}
};

}  // namespace blink

#endif  // FetchDataLoader_h
