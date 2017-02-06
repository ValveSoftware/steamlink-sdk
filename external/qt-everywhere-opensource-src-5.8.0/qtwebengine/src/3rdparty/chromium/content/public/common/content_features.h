// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// This file defines all the public base::FeatureList features for the content
// module.

#ifndef CONTENT_PUBLIC_COMMON_CONTENT_FEATURES_H_
#define CONTENT_PUBLIC_COMMON_CONTENT_FEATURES_H_

#include "base/feature_list.h"
#include "build/build_config.h"
#include "content/common/content_export.h"

namespace features {

// All features in alphabetical order. The features should be documented
// alongside the definition of their values in the .cc file.
CONTENT_EXPORT extern const base::Feature kBrotliEncoding;
CONTENT_EXPORT extern const base::Feature kCanvas2DImageChromium;
CONTENT_EXPORT extern const base::Feature kCredentialManagementAPI;
CONTENT_EXPORT extern const base::Feature kDefaultEnableGpuRasterization;
CONTENT_EXPORT extern const base::Feature kDoNotUnlockSharedBuffer;
CONTENT_EXPORT extern const base::Feature kDocumentWriteEvaluator;
CONTENT_EXPORT extern const base::Feature kFontCacheScaling;
CONTENT_EXPORT extern const base::Feature kMainFrameBeforeActivation;
CONTENT_EXPORT extern const base::Feature kMediaDocumentDownloadButton;
CONTENT_EXPORT extern const base::Feature kNewMediaPlaybackUi;
CONTENT_EXPORT extern const base::Feature kNonValidatingReloadOnNormalReload;
CONTENT_EXPORT extern const base::Feature kNonValidatingReloadOnRefreshContent;
CONTENT_EXPORT extern const base::Feature kOptimizeLoadingIPCForSmallResources;
CONTENT_EXPORT extern const base::Feature kOriginTrials;
CONTENT_EXPORT extern const base::Feature kPaintOptimizations;
CONTENT_EXPORT extern const base::Feature kParseHTMLOnMainThread;
CONTENT_EXPORT extern const base::Feature kPointerEvents;
CONTENT_EXPORT extern const base::Feature kRenderingPipelineThrottling;
CONTENT_EXPORT extern const base::Feature kScrollAnchoring;
CONTENT_EXPORT extern const base::Feature kStaleWhileRevalidate;
CONTENT_EXPORT extern const base::Feature kTokenBinding;
CONTENT_EXPORT extern const base::Feature kWeakMemoryCache;
CONTENT_EXPORT extern const base::Feature kWebGLImageChromium;
CONTENT_EXPORT extern const base::Feature kWebRtcEcdsaDefault;
CONTENT_EXPORT extern const base::Feature kWebUsb;

#if defined(OS_ANDROID)
CONTENT_EXPORT extern const base::Feature kAutoplayMutedVideos;
CONTENT_EXPORT extern const base::Feature kImeThread;
CONTENT_EXPORT extern const base::Feature kSeccompSandboxAndroid;
CONTENT_EXPORT extern const base::Feature kWebPayments;
#endif  // defined(OS_ANDROID)

#if defined(OS_WIN)
CONTENT_EXPORT extern const base::Feature kWinSboxDisableExtensionPoints;
#endif  // defined(OS_WIN)

// DON'T ADD RANDOM STUFF HERE. Put it in the main section above in
// alphabetical order, or in one of the ifdefs (also in order in each section).

}  // namespace features

#endif  // CONTENT_PUBLIC_COMMON_CONTENT_FEATURES_H_
