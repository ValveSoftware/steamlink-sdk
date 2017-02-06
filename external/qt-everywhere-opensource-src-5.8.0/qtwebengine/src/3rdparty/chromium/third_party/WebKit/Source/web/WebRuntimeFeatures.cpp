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

#include "public/web/WebRuntimeFeatures.h"

#include "platform/RuntimeEnabledFeatures.h"
#include "wtf/Assertions.h"

namespace blink {

void WebRuntimeFeatures::enableExperimentalFeatures(bool enable)
{
    RuntimeEnabledFeatures::setExperimentalFeaturesEnabled(enable);
}

void WebRuntimeFeatures::enableWebBluetooth(bool enable)
{
    RuntimeEnabledFeatures::setWebBluetoothEnabled(enable);
}

void WebRuntimeFeatures::enableWebUsb(bool enable)
{
    RuntimeEnabledFeatures::setWebUSBEnabled(enable);
}

void WebRuntimeFeatures::enableFeatureFromString(const std::string& name, bool enable)
{
    RuntimeEnabledFeatures::setFeatureEnabledFromString(name, enable);
}

void WebRuntimeFeatures::enableTestOnlyFeatures(bool enable)
{
    RuntimeEnabledFeatures::setTestFeaturesEnabled(enable);
}

void WebRuntimeFeatures::enableApplicationCache(bool enable)
{
    RuntimeEnabledFeatures::setApplicationCacheEnabled(enable);
}

void WebRuntimeFeatures::enableAudioOutputDevices(bool enable)
{
    RuntimeEnabledFeatures::setAudioOutputDevicesEnabled(enable);
}

void WebRuntimeFeatures::enableCanvas2dImageChromium(bool enable)
{
    RuntimeEnabledFeatures::setCanvas2dImageChromiumEnabled(enable);
}

void WebRuntimeFeatures::enableCompositedSelectionUpdate(bool enable)
{
    RuntimeEnabledFeatures::setCompositedSelectionUpdateEnabled(enable);
}

bool WebRuntimeFeatures::isCompositedSelectionUpdateEnabled()
{
    return RuntimeEnabledFeatures::compositedSelectionUpdateEnabled();
}

void WebRuntimeFeatures::enableDatabase(bool enable)
{
    RuntimeEnabledFeatures::setDatabaseEnabled(enable);
}

void WebRuntimeFeatures::enableDecodeToYUV(bool enable)
{
    RuntimeEnabledFeatures::setDecodeToYUVEnabled(enable);
}

void WebRuntimeFeatures::forceDisplayList2dCanvas(bool enable)
{
    RuntimeEnabledFeatures::setForceDisplayList2dCanvasEnabled(enable);
}

void WebRuntimeFeatures::forceDisable2dCanvasCopyOnWrite(bool enable)
{
    RuntimeEnabledFeatures::setForceDisable2dCanvasCopyOnWriteEnabled(enable);
}

void WebRuntimeFeatures::enableDisplayList2dCanvas(bool enable)
{
    RuntimeEnabledFeatures::setDisplayList2dCanvasEnabled(enable);
}

void WebRuntimeFeatures::enableDoNotUnlockSharedBuffer(bool enable)
{
    RuntimeEnabledFeatures::setDoNotUnlockSharedBufferEnabled(enable);
}

void WebRuntimeFeatures::enableDocumentWriteEvaluator(bool enable)
{
    RuntimeEnabledFeatures::setDocumentWriteEvaluatorEnabled(enable);
}

void WebRuntimeFeatures::enableExperimentalCanvasFeatures(bool enable)
{
    RuntimeEnabledFeatures::setExperimentalCanvasFeaturesEnabled(enable);
}

void WebRuntimeFeatures::enableFastMobileScrolling(bool enable)
{
    RuntimeEnabledFeatures::setFastMobileScrollingEnabled(enable);
}

void WebRuntimeFeatures::enableFileSystem(bool enable)
{
    RuntimeEnabledFeatures::setFileSystemEnabled(enable);
}

void WebRuntimeFeatures::enableImageColorProfiles(bool enable)
{
    RuntimeEnabledFeatures::setImageColorProfilesEnabled(enable);
}

void WebRuntimeFeatures::enableInputMultipleFieldsUI(bool enable)
{
    RuntimeEnabledFeatures::setInputMultipleFieldsUIEnabled(enable);
}

void WebRuntimeFeatures::enableMediaCapture(bool enable)
{
    RuntimeEnabledFeatures::setMediaCaptureEnabled(enable);
}

void WebRuntimeFeatures::enableMediaDocumentDownloadButton(bool enable)
{
    RuntimeEnabledFeatures::setMediaDocumentDownloadButtonEnabled(enable);
}

void WebRuntimeFeatures::enableNotificationConstructor(bool enable)
{
    RuntimeEnabledFeatures::setNotificationConstructorEnabled(enable);
}

void WebRuntimeFeatures::enableNotifications(bool enable)
{
    RuntimeEnabledFeatures::setNotificationsEnabled(enable);
}

void WebRuntimeFeatures::enableNavigatorContentUtils(bool enable)
{
    RuntimeEnabledFeatures::setNavigatorContentUtilsEnabled(enable);
}

void WebRuntimeFeatures::enableNetworkInformation(bool enable)
{
    RuntimeEnabledFeatures::setNetworkInformationEnabled(enable);
}

void WebRuntimeFeatures::enableOrientationEvent(bool enable)
{
    RuntimeEnabledFeatures::setOrientationEventEnabled(enable);
}

void WebRuntimeFeatures::enableOriginTrials(bool enable)
{
    RuntimeEnabledFeatures::setOriginTrialsEnabled(enable);
}

bool WebRuntimeFeatures::isOriginTrialsEnabled()
{
    return RuntimeEnabledFeatures::originTrialsEnabled();
}

void WebRuntimeFeatures::enablePagePopup(bool enable)
{
    RuntimeEnabledFeatures::setPagePopupEnabled(enable);
}

void WebRuntimeFeatures::enableParseHTMLOnMainThread(bool enable)
{
    RuntimeEnabledFeatures::setParseHTMLOnMainThreadEnabled(enable);
}

void WebRuntimeFeatures::enablePaymentRequest(bool enable)
{
    RuntimeEnabledFeatures::setPaymentRequestEnabled(enable);
}

void WebRuntimeFeatures::enablePermissionsAPI(bool enable)
{
    RuntimeEnabledFeatures::setPermissionsEnabled(enable);
}

void WebRuntimeFeatures::enableScriptedSpeech(bool enable)
{
    RuntimeEnabledFeatures::setScriptedSpeechEnabled(enable);
}

void WebRuntimeFeatures::enableSlimmingPaintV2(bool enable)
{
    RuntimeEnabledFeatures::setSlimmingPaintV2Enabled(enable);
}

void WebRuntimeFeatures::enableTouch(bool enable)
{
    RuntimeEnabledFeatures::setTouchEnabled(enable);
}

void WebRuntimeFeatures::enableWebGLDraftExtensions(bool enable)
{
    RuntimeEnabledFeatures::setWebGLDraftExtensionsEnabled(enable);
}

void WebRuntimeFeatures::enableWebGLImageChromium(bool enable)
{
    RuntimeEnabledFeatures::setWebGLImageChromiumEnabled(enable);
}

void WebRuntimeFeatures::enableXSLT(bool enable)
{
    RuntimeEnabledFeatures::setXSLTEnabled(enable);
}

void WebRuntimeFeatures::enableOverlayScrollbars(bool enable)
{
    RuntimeEnabledFeatures::setOverlayScrollbarsEnabled(enable);
}

void WebRuntimeFeatures::forceOverlayFullscreenVideo(bool enable)
{
    RuntimeEnabledFeatures::setForceOverlayFullscreenVideoEnabled(enable);
}

void WebRuntimeFeatures::enableSharedWorker(bool enable)
{
    RuntimeEnabledFeatures::setSharedWorkerEnabled(enable);
}

void WebRuntimeFeatures::enablePreciseMemoryInfo(bool enable)
{
    RuntimeEnabledFeatures::setPreciseMemoryInfoEnabled(enable);
}

void WebRuntimeFeatures::enableCredentialManagerAPI(bool enable)
{
    RuntimeEnabledFeatures::setCredentialManagerEnabled(enable);
}

void WebRuntimeFeatures::enableV8IdleTasks(bool enable)
{
    RuntimeEnabledFeatures::setV8IdleTasksEnabled(enable);
}

void WebRuntimeFeatures::enableReducedReferrerGranularity(bool enable)
{
    RuntimeEnabledFeatures::setReducedReferrerGranularityEnabled(enable);
}

void WebRuntimeFeatures::enablePushMessaging(bool enable)
{
    RuntimeEnabledFeatures::setPushMessagingEnabled(enable);
}

void WebRuntimeFeatures::enableUnsafeES3APIs(bool enable)
{
    RuntimeEnabledFeatures::setUnsafeES3APIsEnabled(enable);
}

void WebRuntimeFeatures::enableWeakMemoryCache(bool enable)
{
    RuntimeEnabledFeatures::setWeakMemoryCacheEnabled(enable);
}

void WebRuntimeFeatures::enableWebVR(bool enable)
{
    RuntimeEnabledFeatures::setWebVREnabled(enable);
}

void WebRuntimeFeatures::enableNewMediaPlaybackUi(bool enable)
{
    RuntimeEnabledFeatures::setNewMediaPlaybackUiEnabled(enable);
}

void WebRuntimeFeatures::enablePresentationAPI(bool enable)
{
    RuntimeEnabledFeatures::setPresentationEnabled(enable);
}

void WebRuntimeFeatures::enableWebFontsInterventionV2With2G(bool enable)
{
    RuntimeEnabledFeatures::setWebFontsInterventionV2With2GEnabled(enable);
}

void WebRuntimeFeatures::enableWebFontsInterventionV2WithSlow2G(bool enable)
{
    RuntimeEnabledFeatures::setWebFontsInterventionV2WithSlow2GEnabled(enable);
}

void WebRuntimeFeatures::enableWebFontsInterventionTrigger(bool enable)
{
    RuntimeEnabledFeatures::setWebFontsInterventionTriggerEnabled(enable);
}

void WebRuntimeFeatures::enableRenderingPipelineThrottling(bool enable)
{
    RuntimeEnabledFeatures::setRenderingPipelineThrottlingEnabled(enable);
}

void WebRuntimeFeatures::enableScrollAnchoring(bool enable)
{
    RuntimeEnabledFeatures::setScrollAnchoringEnabled(enable);
}

void WebRuntimeFeatures::enableAutoplayMutedVideos(bool enable)
{
    RuntimeEnabledFeatures::setAutoplayMutedVideosEnabled(enable);
}

} // namespace blink
