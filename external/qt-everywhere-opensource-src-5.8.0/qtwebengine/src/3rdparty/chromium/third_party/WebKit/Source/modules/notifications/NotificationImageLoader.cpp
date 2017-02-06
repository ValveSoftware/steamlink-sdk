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
#include "third_party/skia/include/core/SkBitmap.h"
#include "wtf/CurrentTime.h"
#include "wtf/Threading.h"
#include <memory>

namespace blink {

NotificationImageLoader::NotificationImageLoader()
    : m_stopped(false), m_startTime(0.0)
{
}

NotificationImageLoader::~NotificationImageLoader()
{
}

void NotificationImageLoader::start(ExecutionContext* executionContext, const KURL& url, std::unique_ptr<ImageCallback> imageCallback)
{
    DCHECK(!m_stopped);

    m_startTime = monotonicallyIncreasingTimeMS();
    m_imageCallback = std::move(imageCallback);

    // TODO(mvanouwerkerk): Add a timeout mechanism: crbug.com/579137.
    ThreadableLoaderOptions threadableLoaderOptions;
    threadableLoaderOptions.preflightPolicy = PreventPreflight;
    threadableLoaderOptions.crossOriginRequestPolicy = AllowCrossOriginRequests;

    // TODO(mvanouwerkerk): Add an entry for notifications to FetchInitiatorTypeNames and use it.
    ResourceLoaderOptions resourceLoaderOptions;
    resourceLoaderOptions.allowCredentials = AllowStoredCredentials;
    if (executionContext->isWorkerGlobalScope())
        resourceLoaderOptions.requestInitiatorContext = WorkerContext;

    ResourceRequest resourceRequest(url);
    resourceRequest.setRequestContext(WebURLRequest::RequestContextImage);
    resourceRequest.setPriority(ResourceLoadPriorityMedium);
    resourceRequest.setRequestorOrigin(executionContext->getSecurityOrigin());

    m_threadableLoader = ThreadableLoader::create(*executionContext, this, threadableLoaderOptions, resourceLoaderOptions);
    m_threadableLoader->start(resourceRequest);
}

void NotificationImageLoader::stop()
{
    if (m_stopped)
        return;

    m_stopped = true;
    if (m_threadableLoader) {
        m_threadableLoader->cancel();
        // WorkerThreadableLoader keeps a Persistent<WorkerGlobalScope> to the
        // ExecutionContext it received in |create|. Kill it to prevent
        // reference cycles involving a mix of GC and non-GC types that fail to
        // clear in ThreadState::cleanup.
        m_threadableLoader.reset();
    }
}

void NotificationImageLoader::didReceiveData(const char* data, unsigned length)
{
    if (!m_data)
        m_data = SharedBuffer::create();
    m_data->append(data, length);
}

void NotificationImageLoader::didFinishLoading(unsigned long resourceIdentifier, double finishTime)
{
    // If this has been stopped it is not desirable to trigger further work,
    // there is a shutdown of some sort in progress.
    if (m_stopped)
        return;

    DEFINE_THREAD_SAFE_STATIC_LOCAL(CustomCountHistogram, finishedTimeHistogram, new CustomCountHistogram("Notifications.Icon.LoadFinishTime", 1, 1000 * 60 * 60 /* 1 hour max */, 50 /* buckets */));
    finishedTimeHistogram.count(monotonicallyIncreasingTimeMS() - m_startTime);

    if (m_data) {
        DEFINE_THREAD_SAFE_STATIC_LOCAL(CustomCountHistogram, fileSizeHistogram, new CustomCountHistogram("Notifications.Icon.FileSize", 1, 10000000 /* ~10mb max */, 50 /* buckets */));
        fileSizeHistogram.count(m_data->size());

        std::unique_ptr<ImageDecoder> decoder = ImageDecoder::create(*m_data.get(), ImageDecoder::AlphaPremultiplied, ImageDecoder::GammaAndColorProfileApplied);
        if (decoder) {
            decoder->setData(m_data.get(), true /* allDataReceived */);
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

void NotificationImageLoader::didFail(const ResourceError& error)
{
    DEFINE_THREAD_SAFE_STATIC_LOCAL(CustomCountHistogram, failedTimeHistogram, new CustomCountHistogram("Notifications.Icon.LoadFailTime", 1, 1000 * 60 * 60 /* 1 hour max */, 50 /* buckets */));
    failedTimeHistogram.count(monotonicallyIncreasingTimeMS() - m_startTime);

    runCallbackWithEmptyBitmap();
}

void NotificationImageLoader::didFailRedirectCheck()
{
    runCallbackWithEmptyBitmap();
}

void NotificationImageLoader::runCallbackWithEmptyBitmap()
{
    // If this has been stopped it is not desirable to trigger further work,
    // there is a shutdown of some sort in progress.
    if (m_stopped)
        return;

    (*m_imageCallback)(SkBitmap());
}

} // namespace blink
