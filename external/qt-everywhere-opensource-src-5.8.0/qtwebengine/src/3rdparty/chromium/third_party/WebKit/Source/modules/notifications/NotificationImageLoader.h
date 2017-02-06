// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef NotificationImageLoader_h
#define NotificationImageLoader_h

#include "core/loader/ThreadableLoader.h"
#include "core/loader/ThreadableLoaderClient.h"
#include "platform/SharedBuffer.h"
#include "platform/heap/Handle.h"
#include "wtf/Functional.h"
#include "wtf/RefPtr.h"
#include <memory>

class SkBitmap;

namespace blink {

class ExecutionContext;
class KURL;
class ResourceError;

// Asynchronously downloads an image when given a url, decodes the loaded data,
// and passes the bitmap to the given callback.
class NotificationImageLoader final : public GarbageCollectedFinalized<NotificationImageLoader>, public ThreadableLoaderClient {
public:
    // The bitmap may be empty if the request failed or the image data could not
    // be decoded.
    using ImageCallback = Function<void(const SkBitmap&)>;

    NotificationImageLoader();
    ~NotificationImageLoader() override;

    // Asynchronously downloads an image from the given url, decodes the loaded
    // data, and passes the bitmap to the callback.
    void start(ExecutionContext*, const KURL&, std::unique_ptr<ImageCallback>);

    // Cancels the pending load, if there is one. The |m_imageCallback| will not
    // be run.
    void stop();

    // ThreadableLoaderClient interface.
    void didReceiveData(const char* data, unsigned length) override;
    void didFinishLoading(unsigned long resourceIdentifier, double finishTime) override;
    void didFail(const ResourceError&) override;
    void didFailRedirectCheck() override;

    DEFINE_INLINE_TRACE() {}

private:
    void runCallbackWithEmptyBitmap();

    bool m_stopped;
    double m_startTime;
    RefPtr<SharedBuffer> m_data;
    std::unique_ptr<ImageCallback> m_imageCallback;
    std::unique_ptr<ThreadableLoader> m_threadableLoader;
};

} // namespace blink

#endif // NotificationImageLoader_h
