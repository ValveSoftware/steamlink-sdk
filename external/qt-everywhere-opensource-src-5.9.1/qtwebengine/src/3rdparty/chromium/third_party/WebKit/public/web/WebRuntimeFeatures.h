/*
 * Copyright (C) 2013 Google Inc. All rights reserved.
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

#ifndef WebRuntimeFeatures_h
#define WebRuntimeFeatures_h

#include "../platform/WebCommon.h"
#include "../platform/WebString.h"

#include <string>

namespace blink {

// This class is used to enable runtime features of Blink.
// Stable features are enabled by default.
class WebRuntimeFeatures {
 public:
  // Enable features with status=experimental listed in
  // Source/platform/RuntimeEnabledFeatures.in.
  BLINK_EXPORT static void enableExperimentalFeatures(bool);

  // Enable features with status=test listed in
  // Source/platform/RuntimeEnabledFeatures.in.
  BLINK_EXPORT static void enableTestOnlyFeatures(bool);

  // Enables a feature by its string identifier from
  // Source/platform/RuntimeEnabledFeatures.in.
  // Note: We use std::string instead of WebString because this API can
  // be called before blink::initalize(). We can't create WebString objects
  // before blink::initialize().
  BLINK_EXPORT static void enableFeatureFromString(const std::string& name,
                                                   bool enable);

  BLINK_EXPORT static void enableCompositedSelectionUpdate(bool);
  BLINK_EXPORT static bool isCompositedSelectionUpdateEnabled();

  BLINK_EXPORT static void enableDisplayList2dCanvas(bool);
  BLINK_EXPORT static void forceDisplayList2dCanvas(bool);
  BLINK_EXPORT static void forceDisable2dCanvasCopyOnWrite(bool);

  BLINK_EXPORT static void enableOriginTrials(bool);
  BLINK_EXPORT static bool isOriginTrialsEnabled();

  BLINK_EXPORT static void enableAccelerated2dCanvas(bool);
  BLINK_EXPORT static void enableAudioOutputDevices(bool);
  BLINK_EXPORT static void enableCanvas2dImageChromium(bool);
  BLINK_EXPORT static void enableColorCorrectRendering(bool);
  BLINK_EXPORT static void enableCredentialManagerAPI(bool);
  BLINK_EXPORT static void enableDatabase(bool);
  BLINK_EXPORT static void enableDecodeToYUV(bool);
  BLINK_EXPORT static void enableDocumentWriteEvaluator(bool);
  BLINK_EXPORT static void enableExperimentalCanvasFeatures(bool);
  BLINK_EXPORT static void enableFastMobileScrolling(bool);
  BLINK_EXPORT static void enableFeaturePolicy(bool);
  BLINK_EXPORT static void enableFileSystem(bool);
  BLINK_EXPORT static void enableGamepadExtensions(bool);
  BLINK_EXPORT static void enableGenericSensor(bool);
  BLINK_EXPORT static void enableInputMultipleFieldsUI(bool);
  BLINK_EXPORT static void enableLazyParseCSS(bool);
  BLINK_EXPORT static void enableMediaCapture(bool);
  BLINK_EXPORT static void enableMediaDocumentDownloadButton(bool);
  BLINK_EXPORT static void enableMiddleClickAutoscroll(bool);
  BLINK_EXPORT static void enableNavigatorContentUtils(bool);
  BLINK_EXPORT static void enableNetworkInformation(bool);
  BLINK_EXPORT static void enableNotificationConstructor(bool);
  BLINK_EXPORT static void enableNotificationContentImage(bool);
  BLINK_EXPORT static void enableNotifications(bool);
  BLINK_EXPORT static void enableOrientationEvent(bool);
  BLINK_EXPORT static void enableOverlayScrollbars(bool);
  BLINK_EXPORT static void enablePagePopup(bool);
  BLINK_EXPORT static void enableParseHTMLOnMainThread(bool);
  BLINK_EXPORT static void enablePassiveDocumentEventListeners(bool);
  BLINK_EXPORT static void enablePassiveEventListenersDueToFling(bool);
  BLINK_EXPORT static void enablePaymentRequest(bool);
  BLINK_EXPORT static void enablePermissionsAPI(bool);
  BLINK_EXPORT static void enablePointerEvent(bool);
  BLINK_EXPORT static void enablePointerEventV1SpecCapturing(bool);
  BLINK_EXPORT static void enablePreciseMemoryInfo(bool);
  BLINK_EXPORT static void enablePresentationAPI(bool);
  BLINK_EXPORT static void enablePushMessaging(bool);
  BLINK_EXPORT static void enableReducedReferrerGranularity(bool);
  BLINK_EXPORT static void enableReloadwithoutSubResourceCacheRevalidation(
      bool);
  BLINK_EXPORT static void enableRenderingPipelineThrottling(bool);
  BLINK_EXPORT static void enableRemotePlaybackAPI(bool);
  BLINK_EXPORT static void enableRootLayerScrolling(bool);
  BLINK_EXPORT static void enableScriptedSpeech(bool);
  BLINK_EXPORT static void enableScrollAnchoring(bool);
  BLINK_EXPORT static void enableServiceWorkerNavigationPreload(bool);
  BLINK_EXPORT static void enableSharedArrayBuffer(bool);
  BLINK_EXPORT static void enableSharedWorker(bool);
  BLINK_EXPORT static void enableSlimmingPaintV2(bool);
  BLINK_EXPORT static void enableSpeculativeLaunchServiceWorker(bool);
  BLINK_EXPORT static void enableTouch(bool);
  BLINK_EXPORT static void enableV8IdleTasks(bool);
  BLINK_EXPORT static void enableWebAssemblySerialization(bool);
  BLINK_EXPORT static void enableWebBluetooth(bool);
  BLINK_EXPORT static void enableWebFontsInterventionV2With2G(bool);
  BLINK_EXPORT static void enableWebFontsInterventionV2With3G(bool);
  BLINK_EXPORT static void enableWebFontsInterventionV2WithSlow2G(bool);
  BLINK_EXPORT static void enableWebFontsInterventionTrigger(bool);
  BLINK_EXPORT static void enableWebGLDraftExtensions(bool);
  BLINK_EXPORT static void enableWebGLImageChromium(bool);
  BLINK_EXPORT static void enableWebUsb(bool);
  BLINK_EXPORT static void enableWebVR(bool);
  BLINK_EXPORT static void enableXSLT(bool);
  BLINK_EXPORT static void forceOverlayFullscreenVideo(bool);
  BLINK_EXPORT static void enableAutoplayMutedVideos(bool);
  BLINK_EXPORT static void enableTimerThrottlingForBackgroundTabs(bool);
  BLINK_EXPORT static void enableTimerThrottlingForHiddenFrames(bool);
  BLINK_EXPORT static void enableExpensiveBackgroundTimerThrottling(bool);
  BLINK_EXPORT static void enableCanvas2dDynamicRenderingModeSwitching(bool);
  BLINK_EXPORT static void enableSendBeaconThrowForBlobWithNonSimpleType(bool);

 private:
  WebRuntimeFeatures();
};

}  // namespace blink

#endif
