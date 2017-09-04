// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef NotificationImageLoader_h
#define NotificationImageLoader_h

#include "core/loader/ThreadableLoader.h"
#include "core/loader/ThreadableLoaderClient.h"
#include "modules/ModulesExport.h"
#include "platform/SharedBuffer.h"
#include "platform/heap/Handle.h"
#include "third_party/skia/include/core/SkBitmap.h"
#include "wtf/Functional.h"
#include "wtf/RefPtr.h"
#include <memory>

namespace blink {

class ExecutionContext;
class KURL;
class ResourceError;

// Asynchronously downloads an image when given a url, decodes the loaded data,
// and passes the bitmap to the given callback.
class MODULES_EXPORT NotificationImageLoader final
    : public GarbageCollectedFinalized<NotificationImageLoader>,
      public ThreadableLoaderClient {
 public:
  // Type names are used in UMAs, so do not rename.
  enum class Type { Image, Icon, Badge, ActionIcon };

  // The bitmap may be empty if the request failed or the image data could not
  // be decoded.
  using ImageCallback = Function<void(const SkBitmap&)>;

  explicit NotificationImageLoader(Type);
  ~NotificationImageLoader() override;

  // Scales down |image| according to its type and returns result. If it is
  // already small enough, |image| is returned unchanged.
  static SkBitmap scaleDownIfNeeded(const SkBitmap& image, Type);

  // Asynchronously downloads an image from the given url, decodes the loaded
  // data, and passes the bitmap to the callback. Times out if the load takes
  // too long and ImageCallback is invoked with an empty bitmap.
  void start(ExecutionContext*, const KURL&, std::unique_ptr<ImageCallback>);

  // Cancels the pending load, if there is one. The |m_imageCallback| will not
  // be run.
  void stop();

  // ThreadableLoaderClient interface.
  void didReceiveData(const char* data, unsigned length) override;
  void didFinishLoading(unsigned long resourceIdentifier,
                        double finishTime) override;
  void didFail(const ResourceError&) override;
  void didFailRedirectCheck() override;

  DEFINE_INLINE_TRACE() { visitor->trace(m_threadableLoader); }

 private:
  void runCallbackWithEmptyBitmap();

  Type m_type;
  bool m_stopped;
  double m_startTime;
  RefPtr<SharedBuffer> m_data;
  std::unique_ptr<ImageCallback> m_imageCallback;
  Member<ThreadableLoader> m_threadableLoader;
};

}  // namespace blink

#endif  // NotificationImageLoader_h
