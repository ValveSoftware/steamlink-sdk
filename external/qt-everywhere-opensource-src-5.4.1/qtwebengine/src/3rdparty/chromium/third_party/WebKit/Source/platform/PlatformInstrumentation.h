/*
 * Copyright (C) 2012 Google Inc. All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions are
 * met:
 *
 *     * Redistributions of source code must retain the above copyright
 * notice, this list of conditions and the following disclaimer.
 *     * Redistributions in binary form must reproduce the above
 * copyright notice, this list of conditions and the following disclaimer
 * in the documentation and/or other materials provided with the
 * distribution.
 *     * Neither the name of Google Inc. nor the names of its
 * contributors may be used to endorse or promote products derived from
 * this software without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS
 * "AS IS" AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT
 * LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR
 * A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT
 * OWNER OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL,
 * SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT
 * LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE,
 * DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY
 * THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
 * OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

#ifndef PlatformInstrumentation_h
#define PlatformInstrumentation_h

#include "platform/PlatformExport.h"
#include "platform/TraceEvent.h"
#include "wtf/MainThread.h"
#include "wtf/text/WTFString.h"

namespace WebCore {

class PLATFORM_EXPORT PlatformInstrumentationClient {
public:
    virtual ~PlatformInstrumentationClient();

    virtual void willDecodeImage(const String& imageType) = 0;
    virtual void didDecodeImage() = 0;
    virtual void willResizeImage(bool shouldCache) = 0;
    virtual void didResizeImage() = 0;
};

class PLATFORM_EXPORT PlatformInstrumentation {
public:
    class LazyPixelRefTracker: TraceEvent::TraceScopedTrackableObject<void*> {
    public:
        LazyPixelRefTracker(void* instance)
            : TraceEvent::TraceScopedTrackableObject<void*>(CategoryName, LazyPixelRef, instance)
        {
        }
    };

    static const char ImageDecodeEvent[];
    static const char ImageResizeEvent[];
    static const char DrawLazyPixelRefEvent[];
    static const char DecodeLazyPixelRefEvent[];

    static const char ImageTypeArgument[];
    static const char CachedArgument[];

    static const char LazyPixelRef[];

    static void setClient(PlatformInstrumentationClient*);
    static bool hasClient() { return m_client; }

    static void willDecodeImage(const String& imageType);
    static void didDecodeImage();
    static void willResizeImage(bool shouldCache);
    static void didResizeImage();
    static void didDrawLazyPixelRef(unsigned long long lazyPixelRefId);
    static void willDecodeLazyPixelRef(unsigned long long lazyPixelRefId);
    static void didDecodeLazyPixelRef();

private:
    static const char CategoryName[];

    static PlatformInstrumentationClient* m_client;
};

#define FAST_RETURN_IF_NO_CLIENT_OR_NOT_MAIN_THREAD() if (!m_client || !isMainThread()) return;

inline void PlatformInstrumentation::willDecodeImage(const String& imageType)
{
    TRACE_EVENT_BEGIN1(CategoryName, ImageDecodeEvent, ImageTypeArgument, imageType.ascii());
    FAST_RETURN_IF_NO_CLIENT_OR_NOT_MAIN_THREAD();
    m_client->willDecodeImage(imageType);
}

inline void PlatformInstrumentation::didDecodeImage()
{
    TRACE_EVENT_END0(CategoryName, ImageDecodeEvent);
    FAST_RETURN_IF_NO_CLIENT_OR_NOT_MAIN_THREAD();
    m_client->didDecodeImage();
}

inline void PlatformInstrumentation::willResizeImage(bool shouldCache)
{
    TRACE_EVENT_BEGIN1(CategoryName, ImageResizeEvent, CachedArgument, shouldCache);
    FAST_RETURN_IF_NO_CLIENT_OR_NOT_MAIN_THREAD();
    m_client->willResizeImage(shouldCache);
}

inline void PlatformInstrumentation::didResizeImage()
{
    TRACE_EVENT_END0(CategoryName, ImageResizeEvent);
    FAST_RETURN_IF_NO_CLIENT_OR_NOT_MAIN_THREAD();
    m_client->didResizeImage();
}

inline void PlatformInstrumentation::didDrawLazyPixelRef(unsigned long long lazyPixelRefId)
{
    TRACE_EVENT_INSTANT1(CategoryName, DrawLazyPixelRefEvent, LazyPixelRef, lazyPixelRefId);
}

inline void PlatformInstrumentation::willDecodeLazyPixelRef(unsigned long long lazyPixelRefId)
{
    TRACE_EVENT_BEGIN1(CategoryName, DecodeLazyPixelRefEvent, LazyPixelRef, lazyPixelRefId);
}

inline void PlatformInstrumentation::didDecodeLazyPixelRef()
{
    TRACE_EVENT_END0(CategoryName, DecodeLazyPixelRefEvent);
}

} // namespace WebCore

#endif // PlatformInstrumentation_h
