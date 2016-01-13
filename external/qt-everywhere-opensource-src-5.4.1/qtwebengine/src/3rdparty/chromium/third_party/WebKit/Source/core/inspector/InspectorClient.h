/*
 * Copyright (C) 2007 Apple Inc.  All rights reserved.
 * Copyright (C) 2011 Google Inc.  All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 *
 * THIS SOFTWARE IS PROVIDED BY APPLE COMPUTER, INC. ``AS IS'' AND ANY
 * EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR
 * PURPOSE ARE DISCLAIMED.  IN NO EVENT SHALL APPLE COMPUTER, INC. OR
 * CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL,
 * EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO,
 * PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR
 * PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY
 * OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
 * OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

#ifndef InspectorClient_h
#define InspectorClient_h

#include "core/inspector/InspectorStateClient.h"
#include "wtf/Forward.h"
#include "wtf/HashMap.h"
#include "wtf/HashSet.h"

namespace WebCore {

class IntPoint;
class PlatformKeyboardEvent;
class PlatformMouseEvent;

class InspectorClient : public InspectorStateClient {
public:
    virtual void highlight() = 0;
    virtual void hideHighlight() = 0;

    virtual void clearBrowserCache() { }
    virtual void clearBrowserCookies() { }

    typedef void (*TraceEventCallback)(char phase, const unsigned char*, const char* name, unsigned long long id,
        int numArgs, const char* const* argNames, const unsigned char* argTypes, const unsigned long long* argValues,
        unsigned char flags, double timestamp);
    virtual void setTraceEventCallback(const String& categoryFilter, TraceEventCallback) { }
    virtual void resetTraceEventCallback() { }
    virtual void enableTracing(const String& categoryFilter) { }
    virtual void disableTracing() { }

    virtual void startGPUEventsRecording() { }
    virtual void stopGPUEventsRecording() { }

    virtual void setDeviceMetricsOverride(int /*width*/, int /*height*/, float /*deviceScaleFactor*/, bool /*emulateViewport*/, bool /*fitWindow*/) { }
    virtual void clearDeviceMetricsOverride() { }
    virtual void setTouchEventEmulationEnabled(bool) { }

    virtual bool overridesShowPaintRects() { return false; }
    virtual void setShowPaintRects(bool) { }
    virtual void setShowDebugBorders(bool) { }
    virtual void setShowFPSCounter(bool) { }
    virtual void setContinuousPaintingEnabled(bool) { }
    virtual void setShowScrollBottleneckRects(bool) { }

    virtual void requestPageScaleFactor(float scale, const IntPoint& origin) { }
    virtual void getAllocatedObjects(HashSet<const void*>&) { }
    virtual void dumpUncountedAllocatedObjects(const HashMap<const void*, size_t>&) { }

    virtual void dispatchKeyEvent(const PlatformKeyboardEvent&) { }
    virtual void dispatchMouseEvent(const PlatformMouseEvent&) { }

protected:
    virtual ~InspectorClient() { }
};

} // namespace WebCore

#endif // !defined(InspectorClient_h)
