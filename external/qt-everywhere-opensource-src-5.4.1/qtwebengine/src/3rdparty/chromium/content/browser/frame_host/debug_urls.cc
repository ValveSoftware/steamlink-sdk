// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/browser/frame_host/debug_urls.h"

#include <vector>

#include "base/debug/asan_invalid_access.h"
#include "base/debug/profiler.h"
#include "base/strings/utf_string_conversions.h"
#include "content/browser/gpu/gpu_process_host_ui_shim.h"
#include "content/browser/ppapi_plugin_process_host.h"
#include "content/public/browser/browser_thread.h"
#include "content/public/common/content_constants.h"
#include "content/public/common/url_constants.h"
#include "ppapi/proxy/ppapi_messages.h"
#include "url/gurl.h"

namespace content {

namespace {

// Define the Asan debug URLs.
const char kAsanCrashDomain[] = "crash";
const char kAsanHeapOverflow[] = "/browser-heap-overflow";
const char kAsanHeapUnderflow[] = "/browser-heap-underflow";
const char kAsanUseAfterFree[] = "/browser-use-after-free";
#if defined(SYZYASAN)
const char kAsanCorruptHeapBlock[] = "/browser-corrupt-heap-block";
const char kAsanCorruptHeap[] = "/browser-corrupt-heap";
#endif

void HandlePpapiFlashDebugURL(const GURL& url) {
#if defined(ENABLE_PLUGINS)
  bool crash = url == GURL(kChromeUIPpapiFlashCrashURL);

  std::vector<PpapiPluginProcessHost*> hosts;
  PpapiPluginProcessHost::FindByName(
      base::UTF8ToUTF16(kFlashPluginName), &hosts);
  for (std::vector<PpapiPluginProcessHost*>::iterator iter = hosts.begin();
       iter != hosts.end(); ++iter) {
    if (crash)
      (*iter)->Send(new PpapiMsg_Crash());
    else
      (*iter)->Send(new PpapiMsg_Hang());
  }
#endif
}

bool IsAsanDebugURL(const GURL& url) {
#if defined(SYZYASAN)
  if (!base::debug::IsBinaryInstrumented())
    return false;
#endif

  if (!(url.is_valid() && url.SchemeIs(kChromeUIScheme) &&
        url.DomainIs(kAsanCrashDomain, sizeof(kAsanCrashDomain) - 1) &&
        url.has_path())) {
    return false;
  }

  if (url.path() == kAsanHeapOverflow || url.path() == kAsanHeapUnderflow ||
      url.path() == kAsanUseAfterFree) {
    return true;
  }

#if defined(SYZYASAN)
  if (url.path() == kAsanCorruptHeapBlock || url.path() == kAsanCorruptHeap)
    return true;
#endif

  return false;
}

bool HandleAsanDebugURL(const GURL& url) {
#if defined(SYZYASAN)
  if (!base::debug::IsBinaryInstrumented())
    return false;

  if (url.path() == kAsanCorruptHeapBlock) {
    base::debug::AsanCorruptHeapBlock();
    return true;
  } else if (url.path() == kAsanCorruptHeap) {
    base::debug::AsanCorruptHeap();
    return true;
  }
#endif

#if defined(ADDRESS_SANITIZER) || defined(SYZYASAN)
  if (url.path() == kAsanHeapOverflow) {
    base::debug::AsanHeapOverflow();
  } else if (url.path() == kAsanHeapUnderflow) {
    base::debug::AsanHeapUnderflow();
  } else if (url.path() == kAsanUseAfterFree) {
    base::debug::AsanHeapUseAfterFree();
  } else {
    return false;
  }
#endif

  return true;
}


}  // namespace

bool HandleDebugURL(const GURL& url, PageTransition transition) {
  // Ensure that the user explicitly navigated to this URL.
  if (!(transition & PAGE_TRANSITION_FROM_ADDRESS_BAR))
    return false;

  // NOTE: when you add handling of any URLs to this function, also
  // update IsDebugURL, below.

  if (IsAsanDebugURL(url))
    return HandleAsanDebugURL(url);

  if (url.host() == kChromeUIBrowserCrashHost) {
    // Induce an intentional crash in the browser process.
    CHECK(false);
    return true;
  }

  if (url == GURL(kChromeUIGpuCleanURL)) {
    GpuProcessHostUIShim* shim = GpuProcessHostUIShim::GetOneInstance();
    if (shim)
      shim->SimulateRemoveAllContext();
    return true;
  }

  if (url == GURL(kChromeUIGpuCrashURL)) {
    GpuProcessHostUIShim* shim = GpuProcessHostUIShim::GetOneInstance();
    if (shim)
      shim->SimulateCrash();
    return true;
  }

  if (url == GURL(kChromeUIGpuHangURL)) {
    GpuProcessHostUIShim* shim = GpuProcessHostUIShim::GetOneInstance();
    if (shim)
      shim->SimulateHang();
    return true;
  }

  if (url == GURL(kChromeUIPpapiFlashCrashURL) ||
      url == GURL(kChromeUIPpapiFlashHangURL)) {
    BrowserThread::PostTask(BrowserThread::IO, FROM_HERE,
                            base::Bind(&HandlePpapiFlashDebugURL, url));
    return true;
  }

  return false;
}

bool IsDebugURL(const GURL& url) {
  // NOTE: when you add any URLs to this list, also update
  // HandleDebugURL, above.
  return IsRendererDebugURL(url) || IsAsanDebugURL(url) ||
      (url.is_valid() &&
       (url.host() == kChromeUIBrowserCrashHost ||
        url == GURL(kChromeUIGpuCleanURL) ||
        url == GURL(kChromeUIGpuCrashURL) ||
        url == GURL(kChromeUIGpuHangURL) ||
        url == GURL(kChromeUIPpapiFlashCrashURL) ||
        url == GURL(kChromeUIPpapiFlashHangURL)));
}

bool IsRendererDebugURL(const GURL& url) {
  if (!url.is_valid())
    return false;

  if (url.SchemeIs(url::kJavaScriptScheme))
    return true;

  return url == GURL(kChromeUICrashURL) ||
         url == GURL(kChromeUIDumpURL) ||
         url == GURL(kChromeUIKillURL) ||
         url == GURL(kChromeUIHangURL) ||
         url == GURL(kChromeUIShorthangURL);
}

}  // namespace content
