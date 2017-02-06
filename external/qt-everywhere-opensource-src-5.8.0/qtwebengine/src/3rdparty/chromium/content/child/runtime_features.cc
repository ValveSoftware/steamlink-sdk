// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/child/runtime_features.h"

#include <vector>

#include "base/command_line.h"
#include "base/feature_list.h"
#include "base/metrics/field_trial.h"
#include "base/strings/string_split.h"
#include "base/strings/string_util.h"
#include "build/build_config.h"
#include "content/common/content_switches_internal.h"
#include "content/public/common/content_features.h"
#include "content/public/common/content_switches.h"
#include "third_party/WebKit/public/web/WebRuntimeFeatures.h"
#include "ui/gl/gl_switches.h"
#include "ui/native_theme/native_theme_switches.h"

using blink::WebRuntimeFeatures;

namespace content {

static void SetRuntimeFeatureDefaultsForPlatform() {
#if defined(OS_ANDROID)
  // Android does not have support for PagePopup
  WebRuntimeFeatures::enablePagePopup(false);
  // No plan to support complex UI for date/time INPUT types.
  WebRuntimeFeatures::enableInputMultipleFieldsUI(false);
  // Android does not yet support SharedWorker. crbug.com/154571
  WebRuntimeFeatures::enableSharedWorker(false);
  // Android does not yet support NavigatorContentUtils.
  WebRuntimeFeatures::enableNavigatorContentUtils(false);
  WebRuntimeFeatures::enableOrientationEvent(true);
  WebRuntimeFeatures::enableFastMobileScrolling(true);
  WebRuntimeFeatures::enableMediaCapture(true);
  // Android won't be able to reliably support non-persistent notifications, the
  // intended behavior for which is in flux by itself.
  WebRuntimeFeatures::enableNotificationConstructor(false);
  WebRuntimeFeatures::enableNewMediaPlaybackUi(true);
  // Android does not yet support switching of audio output devices
  WebRuntimeFeatures::enableAudioOutputDevices(false);
#else
  WebRuntimeFeatures::enableNavigatorContentUtils(true);
#endif  // defined(OS_ANDROID)

#if defined(OS_ANDROID) || defined(USE_AURA)
  WebRuntimeFeatures::enableCompositedSelectionUpdate(true);
#endif

#if !(defined OS_ANDROID || defined OS_CHROMEOS)
    // Only Android, ChromeOS support NetInfo right now.
    WebRuntimeFeatures::enableNetworkInformation(false);
#endif
}

void SetRuntimeFeaturesDefaultsAndUpdateFromArgs(
    const base::CommandLine& command_line) {
  if (command_line.HasSwitch(switches::kEnableExperimentalWebPlatformFeatures))
    WebRuntimeFeatures::enableExperimentalFeatures(true);

  WebRuntimeFeatures::enableOriginTrials(
      base::FeatureList::IsEnabled(features::kOriginTrials));

  if (command_line.HasSwitch(switches::kEnableWebBluetooth))
    WebRuntimeFeatures::enableWebBluetooth(true);

  if (!base::FeatureList::IsEnabled(features::kWebUsb))
    WebRuntimeFeatures::enableWebUsb(false);

  SetRuntimeFeatureDefaultsForPlatform();

  if (command_line.HasSwitch(switches::kDisableDatabases))
    WebRuntimeFeatures::enableDatabase(false);

  if (command_line.HasSwitch(switches::kDisableNotifications)) {
    WebRuntimeFeatures::enableNotifications(false);

    // Chrome's Push Messaging implementation relies on Web Notifications.
    WebRuntimeFeatures::enablePushMessaging(false);
  }

  if (command_line.HasSwitch(switches::kDisableSharedWorkers))
    WebRuntimeFeatures::enableSharedWorker(false);

  if (command_line.HasSwitch(switches::kDisableSpeechAPI))
    WebRuntimeFeatures::enableScriptedSpeech(false);

  if (command_line.HasSwitch(switches::kDisableFileSystem))
    WebRuntimeFeatures::enableFileSystem(false);

  if (command_line.HasSwitch(switches::kEnableExperimentalCanvasFeatures))
    WebRuntimeFeatures::enableExperimentalCanvasFeatures(true);

  if (!command_line.HasSwitch(switches::kDisableAcceleratedJpegDecoding))
    WebRuntimeFeatures::enableDecodeToYUV(true);

  if (command_line.HasSwitch(switches::kEnableDisplayList2dCanvas))
    WebRuntimeFeatures::enableDisplayList2dCanvas(true);

  if (command_line.HasSwitch(switches::kDisableDisplayList2dCanvas))
    WebRuntimeFeatures::enableDisplayList2dCanvas(false);

  if (command_line.HasSwitch(switches::kForceDisplayList2dCanvas))
    WebRuntimeFeatures::forceDisplayList2dCanvas(true);

  if (command_line.HasSwitch(switches::kEnableWebGLDraftExtensions))
    WebRuntimeFeatures::enableWebGLDraftExtensions(true);

#if defined(OS_MACOSX)
  bool enable_canvas_2d_image_chromium = command_line.HasSwitch(
      switches::kEnableGpuMemoryBufferCompositorResources) &&
      !command_line.HasSwitch(switches::kDisable2dCanvasImageChromium) &&
      !command_line.HasSwitch(switches::kDisableGpu);

  if (enable_canvas_2d_image_chromium) {
    enable_canvas_2d_image_chromium =
        base::FeatureList::IsEnabled(features::kCanvas2DImageChromium);
  }
#else
  bool enable_canvas_2d_image_chromium = false;
#endif
  WebRuntimeFeatures::enableCanvas2dImageChromium(
      enable_canvas_2d_image_chromium);

#if defined(OS_MACOSX)
  bool enable_web_gl_image_chromium = command_line.HasSwitch(
      switches::kEnableGpuMemoryBufferCompositorResources) &&
      !command_line.HasSwitch(switches::kDisableWebGLImageChromium) &&
      !command_line.HasSwitch(switches::kDisableGpu);

  if (enable_web_gl_image_chromium) {
    enable_web_gl_image_chromium =
        base::FeatureList::IsEnabled(features::kWebGLImageChromium);
  }
#else
  bool enable_web_gl_image_chromium =
      command_line.HasSwitch(switches::kEnableWebGLImageChromium);
#endif
  WebRuntimeFeatures::enableWebGLImageChromium(enable_web_gl_image_chromium);

  if (command_line.HasSwitch(switches::kForceOverlayFullscreenVideo))
    WebRuntimeFeatures::forceOverlayFullscreenVideo(true);

  if (ui::IsOverlayScrollbarEnabled())
    WebRuntimeFeatures::enableOverlayScrollbars(true);

  if (command_line.HasSwitch(switches::kEnablePreciseMemoryInfo))
    WebRuntimeFeatures::enablePreciseMemoryInfo(true);

  if (command_line.HasSwitch(switches::kEnableNetworkInformation) ||
      command_line.HasSwitch(
          switches::kEnableExperimentalWebPlatformFeatures)) {
    WebRuntimeFeatures::enableNetworkInformation(true);
  }

  if (!base::FeatureList::IsEnabled(features::kCredentialManagementAPI))
    WebRuntimeFeatures::enableCredentialManagerAPI(false);

  if (command_line.HasSwitch(switches::kReducedReferrerGranularity))
    WebRuntimeFeatures::enableReducedReferrerGranularity(true);

  if (command_line.HasSwitch(switches::kDisablePermissionsAPI))
    WebRuntimeFeatures::enablePermissionsAPI(false);

  if (command_line.HasSwitch(switches::kDisableV8IdleTasks))
    WebRuntimeFeatures::enableV8IdleTasks(false);
  else
    WebRuntimeFeatures::enableV8IdleTasks(true);

  if (command_line.HasSwitch(switches::kEnableUnsafeES3APIs))
    WebRuntimeFeatures::enableUnsafeES3APIs(true);

  if (command_line.HasSwitch(switches::kEnableWebVR))
    WebRuntimeFeatures::enableWebVR(true);

  if (command_line.HasSwitch(switches::kDisablePresentationAPI))
    WebRuntimeFeatures::enablePresentationAPI(false);

  if (base::FeatureList::IsEnabled(features::kWeakMemoryCache))
    WebRuntimeFeatures::enableWeakMemoryCache(true);

  if (base::FeatureList::IsEnabled(features::kDoNotUnlockSharedBuffer))
    WebRuntimeFeatures::enableDoNotUnlockSharedBuffer(true);

  const std::string webfonts_intervention_v2_group_name =
      base::FieldTrialList::FindFullName("WebFontsInterventionV2");
  const std::string webfonts_intervention_v2_about_flag =
      command_line.GetSwitchValueASCII(switches::kEnableWebFontsInterventionV2);
  if (!webfonts_intervention_v2_about_flag.empty()) {
    WebRuntimeFeatures::enableWebFontsInterventionV2With2G(
        webfonts_intervention_v2_about_flag.compare(
            switches::kEnableWebFontsInterventionV2SwitchValueEnabledWith2G) ==
        0);
    WebRuntimeFeatures::enableWebFontsInterventionV2WithSlow2G(
        webfonts_intervention_v2_about_flag.compare(
            switches::
                kEnableWebFontsInterventionV2SwitchValueEnabledWithSlow2G) ==
        0);
  } else {
    WebRuntimeFeatures::enableWebFontsInterventionV2With2G(base::StartsWith(
        webfonts_intervention_v2_group_name,
        switches::kEnableWebFontsInterventionV2SwitchValueEnabledWith2G,
        base::CompareCase::INSENSITIVE_ASCII));
    WebRuntimeFeatures::enableWebFontsInterventionV2WithSlow2G(base::StartsWith(
        webfonts_intervention_v2_group_name,
        switches::kEnableWebFontsInterventionV2SwitchValueEnabledWithSlow2G,
        base::CompareCase::INSENSITIVE_ASCII));
  }
  if (command_line.HasSwitch(switches::kEnableWebFontsInterventionTrigger))
    WebRuntimeFeatures::enableWebFontsInterventionTrigger(true);

  if (base::FeatureList::IsEnabled(features::kScrollAnchoring))
    WebRuntimeFeatures::enableScrollAnchoring(true);

  if (command_line.HasSwitch(switches::kEnableSlimmingPaintV2))
    WebRuntimeFeatures::enableSlimmingPaintV2(true);

  // Note that it might already by true for OS_ANDROID, above.  This is for
  // non-android versions.
  if (base::FeatureList::IsEnabled(features::kNewMediaPlaybackUi))
    WebRuntimeFeatures::enableNewMediaPlaybackUi(true);

  if (base::FeatureList::IsEnabled(features::kDocumentWriteEvaluator))
    WebRuntimeFeatures::enableDocumentWriteEvaluator(true);

  WebRuntimeFeatures::enableMediaDocumentDownloadButton(
      base::FeatureList::IsEnabled(features::kMediaDocumentDownloadButton));

  if (base::FeatureList::IsEnabled(features::kPointerEvents))
    WebRuntimeFeatures::enableFeatureFromString("PointerEvent", true);

  WebRuntimeFeatures::enableFeatureFromString(
      "FontCacheScaling",
      base::FeatureList::IsEnabled(features::kFontCacheScaling));

  if (!base::FeatureList::IsEnabled(features::kPaintOptimizations))
    WebRuntimeFeatures::enableFeatureFromString("PaintOptimizations", false);

  if (base::FeatureList::IsEnabled(features::kParseHTMLOnMainThread))
    WebRuntimeFeatures::enableFeatureFromString("ParseHTMLOnMainThread", true);

  WebRuntimeFeatures::enableRenderingPipelineThrottling(
    base::FeatureList::IsEnabled(features::kRenderingPipelineThrottling));

#if defined(OS_ANDROID)
  WebRuntimeFeatures::enablePaymentRequest(
      base::FeatureList::IsEnabled(features::kWebPayments));
#endif

  // Enable explicitly enabled features, and then disable explicitly disabled
  // ones.
  if (command_line.HasSwitch(switches::kEnableBlinkFeatures)) {
    std::vector<std::string> enabled_features = base::SplitString(
        command_line.GetSwitchValueASCII(switches::kEnableBlinkFeatures),
        ",", base::TRIM_WHITESPACE, base::SPLIT_WANT_ALL);
    for (const std::string& feature : enabled_features)
      WebRuntimeFeatures::enableFeatureFromString(feature, true);
  }
  if (command_line.HasSwitch(switches::kDisableBlinkFeatures)) {
    std::vector<std::string> disabled_features = base::SplitString(
        command_line.GetSwitchValueASCII(switches::kDisableBlinkFeatures),
        ",", base::TRIM_WHITESPACE, base::SPLIT_WANT_ALL);
    for (const std::string& feature : disabled_features)
      WebRuntimeFeatures::enableFeatureFromString(feature, false);
  }
}

}  // namespace content
