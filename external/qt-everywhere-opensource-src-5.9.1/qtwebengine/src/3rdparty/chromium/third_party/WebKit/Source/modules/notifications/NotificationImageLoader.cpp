// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "modules/notifications/NotificationImageLoader.h"

#include "core/dom/ExecutionContext.h"
#include "core/fetch/ResourceLoaderOptions.h"
#include "platform/Histogram.h"
#include "platform/image-decoders/ImageDecoder.h"
#include "platform/image-decoders/ImageFrame.h"
#include "platform/network/ResourceError.h"
#include "platform/network/ResourceLoadPriority.h"
#include "platform/network/ResourceRequest.h"
#include "platform/weborigin/KURL.h"
#include "public/platform/WebURLRequest.h"
#include "public/platform/modules/notifications/WebNotificationConstants.h"
#include "skia/ext/image_operations.h"
#include "wtf/CurrentTime.h"
#include "wtf/Threading.h"
#include <memory>

#define NOTIFICATION_PER_TYPE_HISTOGRAM_COUNTS(metric, type_name, value, max) \
  case NotificationImageLoader::Type::type_name: {                            \
    DEFINE_THREAD_SAFE_STATIC_LOCAL(                                          \
        CustomCountHistogram, metric##type_name##Histogram,                   \
        new CustomCountHistogram("Notifications." #metric "." #type_name,     \
                                 1 /* min */, max, 50 /* buckets */));        \
    metric##type_name##Histogram.count(value);                                \
    break;                                                                    \
  }

#define NOTIFICATION_HISTOGRAM_COUNTS(metric, type, value, max)            \
  switch (type) {                                                          \
    NOTIFICATION_PER_TYPE_HISTOGRAM_COUNTS(metric, Image, value, max)      \
    NOTIFICATION_PER_TYPE_HISTOGRAM_COUNTS(metric, Icon, value, max)       \
    NOTIFICATION_PER_TYPE_HISTOGRAM_COUNTS(metric, Badge, value, max)      \
    NOTIFICATION_PER_TYPE_HISTOGRAM_COUNTS(metric, ActionIcon, value, max) \
  }

namespace {

// 99.9% of all images were fetched successfully in 90 seconds.
const unsigned long kImageFetchTimeoutInMs = 90000;

}  // namespace

namespace blink {

NotificationImageLoader::NotificationImageLoader(Type type)
    : m_type(type), m_stopped(false), m_startTime(0.0) {}

NotificationImageLoader::~NotificationImageLoader() {}

// static
SkBitmap NotificationImageLoader::scaleDownIfNeeded(const SkBitmap& image,
                                                    Type type) {
  int maxWidthPx = 0, maxHeightPx = 0;
  switch (type) {
    case Type::Image:
      maxWidthPx = kWebNotificationMaxImageWidthPx;
      maxHeightPx = kWebNotificationMaxImageHeightPx;
      break;
    case Type::Icon:
      maxWidthPx = kWebNotificationMaxIconSizePx;
      maxHeightPx = kWebNotificationMaxIconSizePx;
      break;
    case Type::Badge:
      maxWidthPx = kWebNotificationMaxBadgeSizePx;
      maxHeightPx = kWebNotificationMaxBadgeSizePx;
      break;
    case Type::ActionIcon:
      maxWidthPx = kWebNotificationMaxActionIconSizePx;
      maxHeightPx = kWebNotificationMaxActionIconSizePx;
      break;
  }
  DCHECK_GT(maxWidthPx, 0);
  DCHECK_GT(maxHeightPx, 0);
  // TODO(peter): Explore doing the scaling on a background thread.
  if (image.width() > maxWidthPx || image.height() > maxHeightPx) {
    double scale = std::min(static_cast<double>(maxWidthPx) / image.width(),
                            static_cast<double>(maxHeightPx) / image.height());
    double startTime = monotonicallyIncreasingTimeMS();
    // TODO(peter): Try using RESIZE_BETTER for large images.
    SkBitmap scaledImage =
        skia::ImageOperations::Resize(image, skia::ImageOperations::RESIZE_BEST,
                                      std::lround(scale * image.width()),
                                      std::lround(scale * image.height()));
    NOTIFICATION_HISTOGRAM_COUNTS(LoadScaleDownTime, type,
                                  monotonicallyIncreasingTimeMS() - startTime,
                                  1000 * 10 /* 10 seconds max */);
    return scaledImage;
  }
  return image;
}

void NotificationImageLoader::start(
    ExecutionContext* executionContext,
    const KURL& url,
    std::unique_ptr<ImageCallback> imageCallback) {
  DCHECK(!m_stopped);

  m_startTime = monotonicallyIncreasingTimeMS();
  m_imageCallback = std::move(imageCallback);

  ThreadableLoaderOptions threadableLoaderOptions;
  threadableLoaderOptions.preflightPolicy = PreventPreflight;
  threadableLoaderOptions.crossOriginRequestPolicy = AllowCrossOriginRequests;
  threadableLoaderOptions.timeoutMilliseconds = kImageFetchTimeoutInMs;

  // TODO(mvanouwerkerk): Add an entry for notifications to
  // FetchInitiatorTypeNames and use it.
  ResourceLoaderOptions resourceLoaderOptions;
  resourceLoaderOptions.allowCredentials = AllowStoredCredentials;
  if (executionContext->isWorkerGlobalScope())
    resourceLoaderOptions.requestInitiatorContext = WorkerContext;

  ResourceRequest resourceRequest(url);
  resourceRequest.setRequestContext(WebURLRequest::RequestContextImage);
  resourceRequest.setPriority(ResourceLoadPriorityMedium);
  resourceRequest.setRequestorOrigin(executionContext->getSecurityOrigin());

  m_threadableLoader = ThreadableLoader::create(
      *executionContext, this, threadableLoaderOptions, resourceLoaderOptions);
  m_threadableLoader->start(resourceRequest);
}

void NotificationImageLoader::stop() {
  if (m_stopped)
    return;

  m_stopped = true;
  if (m_threadableLoader) {
    m_threadableLoader->cancel();
    m_threadableLoader = nullptr;
  }
}

void NotificationImageLoader::didReceiveData(const char* data,
                                             unsigned length) {
  if (!m_data)
    m_data = SharedBuffer::create();
  m_data->append(data, length);
}

void NotificationImageLoader::didFinishLoading(unsigned long resourceIdentifier,
                                               double finishTime) {
  // If this has been stopped it is not desirable to trigger further work,
  // there is a shutdown of some sort in progress.
  if (m_stopped)
    return;

  NOTIFICATION_HISTOGRAM_COUNTS(LoadFinishTime, m_type,
                                monotonicallyIncreasingTimeMS() - m_startTime,
                                1000 * 60 * 60 /* 1 hour max */);

  if (m_data) {
    NOTIFICATION_HISTOGRAM_COUNTS(LoadFileSize, m_type, m_data->size(),
                                  10000000 /* ~10mb max */);

    std::unique_ptr<ImageDecoder> decoder = ImageDecoder::create(
        m_data, true /* dataComplete */, ImageDecoder::AlphaPremultiplied,
        ImageDecoder::ColorSpaceApplied);
    if (decoder) {
      // The |ImageFrame*| is owned by the decoder.
      ImageFrame* imageFrame = decoder->frameBufferAtIndex(0);
      if (imageFrame) {
        (*m_imageCallback)(imageFrame->bitmap());
        return;
      }
    }
  }
  runCallbackWithEmptyBitmap();
}

void NotificationImageLoader::didFail(const ResourceError& error) {
  NOTIFICATION_HISTOGRAM_COUNTS(LoadFailTime, m_type,
                                monotonicallyIncreasingTimeMS() - m_startTime,
                                1000 * 60 * 60 /* 1 hour max */);

  runCallbackWithEmptyBitmap();
}

void NotificationImageLoader::didFailRedirectCheck() {
  runCallbackWithEmptyBitmap();
}

void NotificationImageLoader::runCallbackWithEmptyBitmap() {
  // If this has been stopped it is not desirable to trigger further work,
  // there is a shutdown of some sort in progress.
  if (m_stopped)
    return;

  (*m_imageCallback)(SkBitmap());
}

}  // namespace blink
