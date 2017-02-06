// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "build/build_config.h"
#include "content/public/common/url_constants.h"

namespace content {

// Before adding new chrome schemes please check with security@chromium.org.
// There are security implications associated with introducing new schemes.
const char kChromeDevToolsScheme[] = "chrome-devtools";
const char kChromeUIScheme[] = "chrome";
const char kGuestScheme[] = "chrome-guest";
const char kViewSourceScheme[] = "view-source";
#if defined(OS_CHROMEOS)
const char kExternalFileScheme[] = "externalfile";
#endif

const char kAboutSrcDocURL[] = "about:srcdoc";

const char kChromeUIAppCacheInternalsHost[] = "appcache-internals";
const char kChromeUIIndexedDBInternalsHost[] = "indexeddb-internals";
const char kChromeUIAccessibilityHost[] = "accessibility";
const char kChromeUIBlobInternalsHost[] = "blob-internals";
const char kChromeUIBrowserCrashHost[] = "inducebrowsercrashforrealz";
const char kChromeUIGpuHost[] = "gpu";
const char kChromeUIHistogramHost[] = "histograms";
const char kChromeUIMediaInternalsHost[] = "media-internals";
const char kChromeUINetworkViewCacheHost[] = "view-http-cache";
const char kChromeUINetworkErrorHost[] = "network-error";
const char kChromeUINetworkErrorsListingHost[] = "network-errors";
const char kChromeUIResourcesHost[] = "resources";
const char kChromeUIServiceWorkerInternalsHost[] = "serviceworker-internals";
const char kChromeUITracingHost[] = "tracing";
const char kChromeUIWebRTCInternalsHost[] = "webrtc-internals";

const char kChromeUIBadCastCrashURL[] = "chrome://badcastcrash";
const char kChromeUIBrowserCrashURL[] = "chrome://inducebrowsercrashforrealz";
const char kChromeUIBrowserUIHang[] = "chrome://uithreadhang";
const char kChromeUICrashURL[] = "chrome://crash";
const char kChromeUIDelayedBrowserUIHang[] = "chrome://delayeduithreadhang";
const char kChromeUIDumpURL[] = "chrome://crashdump";
const char kChromeUIGpuCleanURL[] = "chrome://gpuclean";
const char kChromeUIGpuCrashURL[] = "chrome://gpucrash";
const char kChromeUIGpuHangURL[] = "chrome://gpuhang";
const char kChromeUIHangURL[] = "chrome://hang";
const char kChromeUIKillURL[] = "chrome://kill";
const char kChromeUINetworkErrorURL[] = "chrome://network-error";
const char kChromeUINetworkErrorsListingURL[] = "chrome://network-errors";
const char kChromeUIPpapiFlashCrashURL[] = "chrome://ppapiflashcrash";
const char kChromeUIPpapiFlashHangURL[] = "chrome://ppapiflashhang";

// This error URL is loaded in normal web renderer processes, so it should not
// have a chrome:// scheme that might let it be confused with a WebUI page.
const char kUnreachableWebDataURL[] = "data:text/html,chromewebdata";

const char kChromeUINetworkViewCacheURL[] = "chrome://view-http-cache/";
const char kChromeUIResourcesURL[] = "chrome://resources/";
const char kChromeUIShorthangURL[] = "chrome://shorthang";

}  // namespace content
