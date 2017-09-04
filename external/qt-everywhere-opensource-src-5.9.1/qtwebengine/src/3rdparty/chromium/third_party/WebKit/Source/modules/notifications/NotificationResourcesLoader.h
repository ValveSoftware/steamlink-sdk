// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef NotificationResourcesLoader_h
#define NotificationResourcesLoader_h

#include "modules/ModulesExport.h"
#include "modules/notifications/NotificationImageLoader.h"
#include "platform/heap/GarbageCollected.h"
#include "platform/heap/Handle.h"
#include "platform/heap/HeapAllocator.h"
#include "platform/heap/ThreadState.h"
#include "third_party/skia/include/core/SkBitmap.h"
#include "wtf/Functional.h"
#include "wtf/Vector.h"
#include <memory>

namespace blink {

class ExecutionContext;
struct WebNotificationData;
struct WebNotificationResources;

// Fetches the resources specified in a given WebNotificationData. Uses a
// callback to notify the caller when all fetches have finished.
class MODULES_EXPORT NotificationResourcesLoader final
    : public GarbageCollectedFinalized<NotificationResourcesLoader> {
  USING_PRE_FINALIZER(NotificationResourcesLoader, stop);

 public:
  // Called when all fetches have finished. Passes a pointer to the
  // NotificationResourcesLoader so callers that use multiple loaders can use
  // the same function to handle the callbacks.
  using CompletionCallback = Function<void(NotificationResourcesLoader*)>;

  explicit NotificationResourcesLoader(std::unique_ptr<CompletionCallback>);
  ~NotificationResourcesLoader();

  // Starts fetching the resources specified in the given WebNotificationData.
  // If all the urls for the resources are empty or invalid,
  // |m_completionCallback| will be run synchronously, otherwise it will be
  // run asynchronously when all fetches have finished. Should not be called
  // more than once.
  void start(ExecutionContext*, const WebNotificationData&);

  // Returns a new WebNotificationResources populated with the resources that
  // have been fetched.
  std::unique_ptr<WebNotificationResources> getResources() const;

  // Stops every loader in |m_imageLoaders|. This is also used as the
  // pre-finalizer.
  void stop();

  DECLARE_VIRTUAL_TRACE();

 private:
  void loadImage(ExecutionContext*,
                 NotificationImageLoader::Type,
                 const KURL&,
                 std::unique_ptr<NotificationImageLoader::ImageCallback>);
  void didLoadImage(const SkBitmap& image);
  void didLoadIcon(const SkBitmap& image);
  void didLoadBadge(const SkBitmap& image);
  void didLoadActionIcon(size_t actionIndex, const SkBitmap& image);

  // Decrements |m_pendingRequestCount| and runs |m_completionCallback| if
  // there are no more pending requests.
  void didFinishRequest();

  bool m_started;
  std::unique_ptr<CompletionCallback> m_completionCallback;
  int m_pendingRequestCount;
  HeapVector<Member<NotificationImageLoader>> m_imageLoaders;
  SkBitmap m_image;
  SkBitmap m_icon;
  SkBitmap m_badge;
  Vector<SkBitmap> m_actionIcons;
};

}  // namespace blink

#endif  // NotificationResourcesLoader_h
