// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_PUBLIC_COMMON_URL_CONSTANTS_H_
#define CONTENT_PUBLIC_COMMON_URL_CONSTANTS_H_

#include "build/build_config.h"
#include "content/common/content_export.h"
#include "url/url_constants.h"

// Contains constants for known URLs and portions thereof.

namespace content {

// Canonical schemes you can use as input to GURL.SchemeIs().
// TODO(jam): some of these don't below in the content layer, but are accessed
// from there.
CONTENT_EXPORT extern const char kChromeDevToolsScheme[];
CONTENT_EXPORT extern const char kChromeUIScheme[];  // Used for WebUIs.
CONTENT_EXPORT extern const char kGuestScheme[];
CONTENT_EXPORT extern const char kViewSourceScheme[];
#if defined(OS_CHROMEOS)
CONTENT_EXPORT extern const char kExternalFileScheme[];
#endif

// Hosts for about URLs.
CONTENT_EXPORT extern const char kAboutSrcDocURL[];

CONTENT_EXPORT extern const char kChromeUIAccessibilityHost[];
CONTENT_EXPORT extern const char kChromeUIAppCacheInternalsHost[];
CONTENT_EXPORT extern const char kChromeUIBlobInternalsHost[];
CONTENT_EXPORT extern const char kChromeUIBrowserCrashHost[];
CONTENT_EXPORT extern const char kChromeUIGpuHost[];
CONTENT_EXPORT extern const char kChromeUIHistogramHost[];
CONTENT_EXPORT extern const char kChromeUIIndexedDBInternalsHost[];
CONTENT_EXPORT extern const char kChromeUIMediaInternalsHost[];
CONTENT_EXPORT extern const char kChromeUINetworkErrorHost[];
CONTENT_EXPORT extern const char kChromeUINetworkErrorsListingHost[];
CONTENT_EXPORT extern const char kChromeUINetworkViewCacheHost[];
CONTENT_EXPORT extern const char kChromeUIResourcesHost[];
CONTENT_EXPORT extern const char kChromeUIServiceWorkerInternalsHost[];
CONTENT_EXPORT extern const char kChromeUITracingHost[];
CONTENT_EXPORT extern const char kChromeUIWebRTCInternalsHost[];

// Full about URLs (including schemes).
CONTENT_EXPORT extern const char kChromeUIBadCastCrashURL[];
CONTENT_EXPORT extern const char kChromeUIBrowserCrashURL[];
CONTENT_EXPORT extern const char kChromeUIBrowserUIHang[];
CONTENT_EXPORT extern const char kChromeUICrashURL[];
CONTENT_EXPORT extern const char kChromeUIDelayedBrowserUIHang[];
CONTENT_EXPORT extern const char kChromeUIDumpURL[];
CONTENT_EXPORT extern const char kChromeUIGpuCleanURL[];
CONTENT_EXPORT extern const char kChromeUIGpuCrashURL[];
CONTENT_EXPORT extern const char kChromeUIGpuHangURL[];
CONTENT_EXPORT extern const char kChromeUIHangURL[];
CONTENT_EXPORT extern const char kChromeUIKillURL[];
CONTENT_EXPORT extern const char kChromeUINetworkErrorsListingURL[];
CONTENT_EXPORT extern const char kChromeUINetworkErrorURL[];
CONTENT_EXPORT extern const char kChromeUIPpapiFlashCrashURL[];
CONTENT_EXPORT extern const char kChromeUIPpapiFlashHangURL[];

// Special URL used to start a navigation to an error page.
CONTENT_EXPORT extern const char kUnreachableWebDataURL[];

// Full about URLs (including schemes).
CONTENT_EXPORT extern const char kChromeUINetworkViewCacheURL[];
CONTENT_EXPORT extern const char kChromeUIResourcesURL[];
CONTENT_EXPORT extern const char kChromeUIShorthangURL[];

}  // namespace content

#endif  // CONTENT_PUBLIC_COMMON_URL_CONSTANTS_H_
