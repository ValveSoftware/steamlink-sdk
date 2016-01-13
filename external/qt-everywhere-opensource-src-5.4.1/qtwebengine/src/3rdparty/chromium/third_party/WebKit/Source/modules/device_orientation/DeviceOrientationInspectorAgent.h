// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef DeviceOrientationInspectorAgent_h
#define DeviceOrientationInspectorAgent_h

#include "core/inspector/InspectorBaseAgent.h"
#include "wtf/text/WTFString.h"

namespace WebCore {

class DeviceOrientationController;
class Page;

typedef String ErrorString;

class DeviceOrientationInspectorAgent FINAL : public InspectorBaseAgent<DeviceOrientationInspectorAgent>, public InspectorBackendDispatcher::DeviceOrientationCommandHandler {
    WTF_MAKE_NONCOPYABLE(DeviceOrientationInspectorAgent);
public:
    static void provideTo(Page&);

    virtual ~DeviceOrientationInspectorAgent();

    // Protocol methods.
    virtual void setDeviceOrientationOverride(ErrorString*, double, double, double) OVERRIDE;
    virtual void clearDeviceOrientationOverride(ErrorString*) OVERRIDE;

    // Inspector Controller API.
    virtual void clearFrontend() OVERRIDE;
    virtual void restore() OVERRIDE;
    virtual void didCommitLoadForMainFrame() OVERRIDE;

private:
    explicit DeviceOrientationInspectorAgent(Page&);
    DeviceOrientationController& controller();
    Page& m_page;
};

} // namespace WebCore


#endif // !defined(DeviceOrientationInspectorAgent_h)
