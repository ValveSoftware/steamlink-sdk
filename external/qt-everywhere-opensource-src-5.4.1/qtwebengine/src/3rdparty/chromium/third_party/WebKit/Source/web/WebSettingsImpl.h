/*
 * Copyright (C) 2009 Google Inc. All rights reserved.
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

#ifndef WebSettingsImpl_h
#define WebSettingsImpl_h

#include "public/web/WebSettings.h"

namespace WebCore {
class InspectorController;
class Settings;
}

namespace blink {

class WebSettingsImpl FINAL : public WebSettings {
public:
    explicit WebSettingsImpl(WebCore::Settings*, WebCore::InspectorController*);
    virtual ~WebSettingsImpl() { }

    virtual bool mainFrameResizesAreOrientationChanges() const OVERRIDE;
    virtual bool shrinksViewportContentToFit() const OVERRIDE;
    virtual bool scrollAnimatorEnabled() const OVERRIDE;
    virtual bool touchEditingEnabled() const OVERRIDE;
    virtual bool viewportEnabled() const OVERRIDE;
    virtual bool viewportMetaEnabled() const OVERRIDE;
    virtual void setAccelerated2dCanvasEnabled(bool) OVERRIDE;
    virtual void setAccelerated2dCanvasMSAASampleCount(int) OVERRIDE;
    virtual void setAcceleratedCompositingEnabled(bool) OVERRIDE;
    virtual void setAcceleratedCompositingForCanvasEnabled(bool) OVERRIDE;
    virtual void setAcceleratedCompositingForFiltersEnabled(bool) OVERRIDE;
    virtual void setAcceleratedCompositingForFixedPositionEnabled(bool) OVERRIDE;
    virtual void setAcceleratedCompositingForOverflowScrollEnabled(bool) OVERRIDE;
    virtual void setCompositorDrivenAcceleratedScrollingEnabled(bool) OVERRIDE;
    virtual void setAcceleratedCompositingForFixedRootBackgroundEnabled(bool) OVERRIDE;
    virtual void setAcceleratedCompositingForVideoEnabled(bool) OVERRIDE;
    virtual void setAllowDisplayOfInsecureContent(bool) OVERRIDE;
    virtual void setAllowFileAccessFromFileURLs(bool) OVERRIDE;
    virtual void setAllowCustomScrollbarInMainFrame(bool) OVERRIDE;
    virtual void setAllowRunningOfInsecureContent(bool) OVERRIDE;
    virtual void setAllowConnectingInsecureWebSocket(bool) OVERRIDE;
    virtual void setAllowScriptsToCloseWindows(bool) OVERRIDE;
    virtual void setAllowUniversalAccessFromFileURLs(bool) OVERRIDE;
    virtual void setAntialiased2dCanvasEnabled(bool) OVERRIDE;
    virtual void setAsynchronousSpellCheckingEnabled(bool) OVERRIDE;
    virtual void setAutoZoomFocusedNodeToLegibleScale(bool) OVERRIDE;
    virtual void setCaretBrowsingEnabled(bool) OVERRIDE;
    virtual void setClobberUserAgentInitialScaleQuirk(bool) OVERRIDE;
    virtual void setCompositedScrollingForFramesEnabled(bool) OVERRIDE;
    virtual void setCompositorTouchHitTesting(bool) OVERRIDE;
    virtual void setContainerCullingEnabled(bool) OVERRIDE;
    virtual void setCookieEnabled(bool) OVERRIDE;
    virtual void setNavigateOnDragDrop(bool) OVERRIDE;
    virtual void setCursiveFontFamily(const WebString&, UScriptCode = USCRIPT_COMMON) OVERRIDE;
    virtual void setDNSPrefetchingEnabled(bool) OVERRIDE;
    virtual void setDOMPasteAllowed(bool) OVERRIDE;
    virtual void setDefaultFixedFontSize(int) OVERRIDE;
    virtual void setDefaultFontSize(int) OVERRIDE;
    virtual void setDefaultTextEncodingName(const WebString&) OVERRIDE;
    virtual void setDefaultVideoPosterURL(const WebString&) OVERRIDE;
    virtual void setDeferredFiltersEnabled(bool) OVERRIDE;
    virtual void setDeferredImageDecodingEnabled(bool) OVERRIDE;
    virtual void setDeviceScaleAdjustment(float) OVERRIDE;
    virtual void setDeviceSupportsMouse(bool) OVERRIDE;
    virtual void setDeviceSupportsTouch(bool) OVERRIDE;
    virtual void setDoubleTapToZoomEnabled(bool) OVERRIDE;
    virtual void setDownloadableBinaryFontsEnabled(bool) OVERRIDE;
    virtual void setEditingBehavior(EditingBehavior) OVERRIDE;
    virtual void setEnableScrollAnimator(bool) OVERRIDE;
    virtual void setEnableTouchAdjustment(bool) OVERRIDE;
    virtual void setRegionBasedColumnsEnabled(bool) OVERRIDE;
    virtual void setExperimentalWebGLEnabled(bool) OVERRIDE;
    virtual void setFantasyFontFamily(const WebString&, UScriptCode = USCRIPT_COMMON) OVERRIDE;
    virtual void setFixedFontFamily(const WebString&, UScriptCode = USCRIPT_COMMON) OVERRIDE;
    virtual void setGestureTapHighlightEnabled(bool) OVERRIDE;
    virtual void setForceZeroLayoutHeight(bool) OVERRIDE;
    virtual void setHyperlinkAuditingEnabled(bool) OVERRIDE;
    virtual void setIgnoreMainFrameOverflowHiddenQuirk(bool) OVERRIDE;
    virtual void setImagesEnabled(bool) OVERRIDE;
    virtual void setJavaEnabled(bool) OVERRIDE;
    virtual void setJavaScriptCanAccessClipboard(bool) OVERRIDE;
    virtual void setJavaScriptCanOpenWindowsAutomatically(bool) OVERRIDE;
    virtual void setJavaScriptEnabled(bool) OVERRIDE;
    virtual void setLayerSquashingEnabled(bool) OVERRIDE;
    virtual void setLoadsImagesAutomatically(bool) OVERRIDE;
    virtual void setLoadWithOverviewMode(bool) OVERRIDE;
    virtual void setLocalStorageEnabled(bool) OVERRIDE;
    virtual void setMainFrameClipsContent(bool) OVERRIDE;
    virtual void setMainFrameResizesAreOrientationChanges(bool) OVERRIDE;
    virtual void setMaxTouchPoints(int) OVERRIDE;
    virtual void setMediaControlsOverlayPlayButtonEnabled(bool) OVERRIDE;
    virtual void setMediaPlaybackRequiresUserGesture(bool) OVERRIDE;
    virtual void setMinimumAccelerated2dCanvasSize(int) OVERRIDE;
    virtual void setMinimumFontSize(int) OVERRIDE;
    virtual void setMinimumLogicalFontSize(int) OVERRIDE;
    virtual void setMockScrollbarsEnabled(bool) OVERRIDE;
    virtual void setNeedsSiteSpecificQuirks(bool) OVERRIDE;
    virtual void setOfflineWebApplicationCacheEnabled(bool) OVERRIDE;
    virtual void setOpenGLMultisamplingEnabled(bool) OVERRIDE;
    virtual void setPasswordEchoDurationInSeconds(double) OVERRIDE;
    virtual void setPasswordEchoEnabled(bool) OVERRIDE;
    virtual void setPerTilePaintingEnabled(bool) OVERRIDE;
    virtual void setPictographFontFamily(const WebString&, UScriptCode = USCRIPT_COMMON) OVERRIDE;
    virtual void setPinchOverlayScrollbarThickness(int) OVERRIDE;
    virtual void setPinchVirtualViewportEnabled(bool) OVERRIDE;
    virtual void setPluginsEnabled(bool) OVERRIDE;
    virtual void setRenderVSyncNotificationEnabled(bool) OVERRIDE;
    virtual void setReportScreenSizeInPhysicalPixelsQuirk(bool) OVERRIDE;
    virtual void setSansSerifFontFamily(const WebString&, UScriptCode = USCRIPT_COMMON) OVERRIDE;
    virtual void setSelectTrailingWhitespaceEnabled(bool OVERRIDE);
    virtual void setSelectionIncludesAltImageText(bool) OVERRIDE;
    virtual void setSerifFontFamily(const WebString&, UScriptCode = USCRIPT_COMMON) OVERRIDE;
    virtual void setShouldPrintBackgrounds(bool) OVERRIDE;
    virtual void setShouldClearDocumentBackground(bool) OVERRIDE;
    virtual void setShouldRespectImageOrientation(bool) OVERRIDE;
    virtual void setShowFPSCounter(bool) OVERRIDE;
    virtual void setShowPaintRects(bool) OVERRIDE;
    virtual void setShrinksStandaloneImagesToFit(bool) OVERRIDE;
    virtual void setShrinksViewportContentToFit(bool) OVERRIDE;
    virtual void setSmartInsertDeleteEnabled(bool) OVERRIDE;
    virtual void setSpatialNavigationEnabled(bool) OVERRIDE;
    virtual void setStandardFontFamily(const WebString&, UScriptCode = USCRIPT_COMMON) OVERRIDE;
    virtual void setSupportDeprecatedTargetDensityDPI(bool) OVERRIDE;
    virtual void setSupportsMultipleWindows(bool) OVERRIDE;
    virtual void setSyncXHRInDocumentsEnabled(bool) OVERRIDE;
    virtual void setTextAreasAreResizable(bool) OVERRIDE;
    virtual void setTextAutosizingEnabled(bool) OVERRIDE;
    virtual void setAccessibilityFontScaleFactor(float) OVERRIDE;
    virtual void setTouchDragDropEnabled(bool) OVERRIDE;
    virtual void setTouchEditingEnabled(bool) OVERRIDE;
    virtual void setUnifiedTextCheckerEnabled(bool) OVERRIDE;
    virtual void setUnsafePluginPastingEnabled(bool) OVERRIDE;
    virtual void setUsesEncodingDetector(bool) OVERRIDE;
    virtual void setUseExpandedHeuristicsForGpuRasterization(bool) OVERRIDE;
    virtual void setUseLegacyBackgroundSizeShorthandBehavior(bool) OVERRIDE;
    virtual void setUseSolidColorScrollbars(bool) OVERRIDE;
    virtual void setUseWideViewport(bool) OVERRIDE;
    virtual void setValidationMessageTimerMagnification(int) OVERRIDE;
    virtual void setViewportEnabled(bool) OVERRIDE;
    virtual void setViewportMetaEnabled(bool) OVERRIDE;
    virtual void setViewportMetaLayoutSizeQuirk(bool) OVERRIDE;
    virtual void setViewportMetaMergeContentQuirk(bool) OVERRIDE;
    virtual void setViewportMetaNonUserScalableQuirk(bool) OVERRIDE;
    virtual void setViewportMetaZeroValuesQuirk(bool) OVERRIDE;
    virtual void setWebAudioEnabled(bool) OVERRIDE;
    virtual void setWebGLErrorsToConsoleEnabled(bool) OVERRIDE;
    virtual void setWebSecurityEnabled(bool) OVERRIDE;
    virtual void setWideViewportQuirkEnabled(bool) OVERRIDE;
    virtual void setXSSAuditorEnabled(bool) OVERRIDE;

    bool showFPSCounter() const { return m_showFPSCounter; }
    bool showPaintRects() const { return m_showPaintRects; }
    bool renderVSyncNotificationEnabled() const { return m_renderVSyncNotificationEnabled; }
    bool autoZoomFocusedNodeToLegibleScale() const { return m_autoZoomFocusedNodeToLegibleScale; }
    bool gestureTapHighlightEnabled() const { return m_gestureTapHighlightEnabled; }
    bool doubleTapToZoomEnabled() const { return m_doubleTapToZoomEnabled; }
    bool perTilePaintingEnabled() const { return m_perTilePaintingEnabled; }
    bool supportDeprecatedTargetDensityDPI() const { return m_supportDeprecatedTargetDensityDPI; }
    bool useExpandedHeuristicsForGpuRasterization() const { return m_useExpandedHeuristicsForGpuRasterization; }
    bool viewportMetaLayoutSizeQuirk() const { return m_viewportMetaLayoutSizeQuirk; }
    bool viewportMetaNonUserScalableQuirk() const { return m_viewportMetaNonUserScalableQuirk; }
    bool clobberUserAgentInitialScaleQuirk() const { return m_clobberUserAgentInitialScaleQuirk; }

private:
    WebCore::Settings* m_settings;
    WebCore::InspectorController* m_inspectorController;
    bool m_showFPSCounter;
    bool m_showPaintRects;
    bool m_renderVSyncNotificationEnabled;
    bool m_gestureTapHighlightEnabled;
    bool m_autoZoomFocusedNodeToLegibleScale;
    bool m_deferredImageDecodingEnabled;
    bool m_doubleTapToZoomEnabled;
    bool m_perTilePaintingEnabled;
    bool m_supportDeprecatedTargetDensityDPI;
    bool m_shrinksViewportContentToFit;
    bool m_useExpandedHeuristicsForGpuRasterization;
    // This quirk is to maintain compatibility with Android apps built on
    // the Android SDK prior to and including version 18. Presumably, this
    // can be removed any time after 2015. See http://crbug.com/277369.
    bool m_viewportMetaLayoutSizeQuirk;
    // This quirk is to maintain compatibility with Android apps built on
    // the Android SDK prior to and including version 18. Presumably, this
    // can be removed any time after 2015. See http://crbug.com/312691.
    bool m_viewportMetaNonUserScalableQuirk;
    // This quirk is to maintain compatibility with Android apps built on
    // the Android SDK prior to and including version 18. Presumably, this
    // can be removed any time after 2015. See http://crbug.com/313754.
    bool m_clobberUserAgentInitialScaleQuirk;
    bool m_mainFrameResizesAreOrientationChanges;
};

} // namespace blink

#endif
