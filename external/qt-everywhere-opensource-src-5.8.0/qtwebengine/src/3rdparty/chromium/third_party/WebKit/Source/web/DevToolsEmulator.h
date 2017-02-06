// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef DevToolsEmulator_h
#define DevToolsEmulator_h

#include "platform/heap/Handle.h"
#include "public/platform/PointerProperties.h"
#include "public/platform/WebViewportStyle.h"
#include "public/web/WebDeviceEmulationParams.h"
#include "wtf/Forward.h"
#include <memory>

namespace blink {

class InspectorEmulationAgent;
class IntPoint;
class WebInputEvent;
class WebViewImpl;

class DevToolsEmulator final : public GarbageCollectedFinalized<DevToolsEmulator> {
public:
    ~DevToolsEmulator();
    static DevToolsEmulator* create(WebViewImpl*);
    DECLARE_TRACE();

    // Settings overrides.
    void setTextAutosizingEnabled(bool);
    void setDeviceScaleAdjustment(float);
    void setPreferCompositingToLCDTextEnabled(bool);
    void setViewportStyle(WebViewportStyle);
    void setPluginsEnabled(bool);
    void setScriptEnabled(bool);
    void setDoubleTapToZoomEnabled(bool);
    bool doubleTapToZoomEnabled() const;
    void setAvailablePointerTypes(int);
    void setPrimaryPointerType(PointerType);
    void setAvailableHoverTypes(int);
    void setPrimaryHoverType(HoverType);
    void setMainFrameResizesAreOrientationChanges(bool);
    bool mainFrameResizesAreOrientationChanges() const;

    // Emulation.
    void enableDeviceEmulation(const WebDeviceEmulationParams&);
    void disableDeviceEmulation();
    bool resizeIsDeviceSizeChange();
    void setTouchEventEmulationEnabled(bool);
    bool handleInputEvent(const WebInputEvent&);
    void setScriptExecutionDisabled(bool);

private:
    explicit DevToolsEmulator(WebViewImpl*);

    void enableMobileEmulation();
    void disableMobileEmulation();

    WebViewImpl* m_webViewImpl;

    bool m_deviceMetricsEnabled;
    bool m_emulateMobileEnabled;
    WebDeviceEmulationParams m_emulationParams;

    bool m_isOverlayScrollbarsEnabled;
    bool m_isOrientationEventEnabled;
    bool m_isMobileLayoutThemeEnabled;
    float m_originalDefaultMinimumPageScaleFactor;
    float m_originalDefaultMaximumPageScaleFactor;
    bool m_embedderTextAutosizingEnabled;
    float m_embedderDeviceScaleAdjustment;
    bool m_embedderPreferCompositingToLCDTextEnabled;
    WebViewportStyle m_embedderViewportStyle;
    bool m_embedderPluginsEnabled;
    int m_embedderAvailablePointerTypes;
    PointerType m_embedderPrimaryPointerType;
    int m_embedderAvailableHoverTypes;
    HoverType m_embedderPrimaryHoverType;

    bool m_touchEventEmulationEnabled;
    bool m_doubleTapToZoomEnabled;
    bool m_mainFrameResizesAreOrientationChanges;
    bool m_originalTouchEnabled;
    bool m_originalDeviceSupportsMouse;
    bool m_originalDeviceSupportsTouch;
    int m_originalMaxTouchPoints;
    std::unique_ptr<IntPoint> m_lastPinchAnchorCss;
    std::unique_ptr<IntPoint> m_lastPinchAnchorDip;

    bool m_embedderScriptEnabled;
    bool m_scriptExecutionDisabled;
};

} // namespace blink

#endif
