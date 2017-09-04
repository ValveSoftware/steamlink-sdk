// Copyright (c) 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/browser/webui/content_web_ui_controller_factory.h"

#include "build/build_config.h"
#include "content/browser/accessibility/accessibility_ui.h"
#include "content/browser/appcache/appcache_internals_ui.h"
#include "content/browser/gpu/gpu_internals_ui.h"
#include "content/browser/indexed_db/indexed_db_internals_ui.h"
#include "content/browser/media/media_internals_ui.h"
#include "content/browser/net/network_errors_listing_ui.h"
#include "content/browser/service_worker/service_worker_internals_ui.h"
#include "content/browser/tracing/tracing_ui.h"
#include "content/public/browser/storage_partition.h"
#include "content/public/browser/web_contents.h"
#include "content/public/browser/web_ui.h"
#include "content/public/common/url_constants.h"

#if defined(ENABLE_WEBRTC)
#include "content/browser/webrtc/webrtc_internals_ui.h"
#endif

namespace content {

WebUI::TypeID ContentWebUIControllerFactory::GetWebUIType(
      BrowserContext* browser_context, const GURL& url) const {
  if (!url.SchemeIs(kChromeUIScheme))
    return WebUI::kNoWebUI;

  if (url.host_piece() == kChromeUIWebRTCInternalsHost ||
#if !defined(OS_ANDROID) && !defined(TOOLKIT_QT)
      url.host_piece() == kChromeUITracingHost ||
#endif
      url.host_piece() == kChromeUIGpuHost ||
      url.host_piece() == kChromeUIIndexedDBInternalsHost ||
      url.host_piece() == kChromeUIMediaInternalsHost ||
      url.host_piece() == kChromeUIServiceWorkerInternalsHost ||
      url.host_piece() == kChromeUIAccessibilityHost ||
      url.host_piece() == kChromeUIAppCacheInternalsHost ||
      url.host_piece() == kChromeUINetworkErrorsListingHost) {
    return const_cast<ContentWebUIControllerFactory*>(this);
  }
  return WebUI::kNoWebUI;
}

bool ContentWebUIControllerFactory::UseWebUIForURL(
    BrowserContext* browser_context, const GURL& url) const {
  return GetWebUIType(browser_context, url) != WebUI::kNoWebUI;
}

bool ContentWebUIControllerFactory::UseWebUIBindingsForURL(
    BrowserContext* browser_context, const GURL& url) const {
  return UseWebUIForURL(browser_context, url);
}

WebUIController* ContentWebUIControllerFactory::CreateWebUIControllerForURL(
    WebUI* web_ui, const GURL& url) const {
  if (!url.SchemeIs(kChromeUIScheme))
    return nullptr;

  if (url.host_piece() == kChromeUIAppCacheInternalsHost)
    return new AppCacheInternalsUI(web_ui);
  if (url.host_piece() == kChromeUIGpuHost)
    return new GpuInternalsUI(web_ui);
  if (url.host_piece() == kChromeUIIndexedDBInternalsHost)
    return new IndexedDBInternalsUI(web_ui);
  if (url.host_piece() == kChromeUIMediaInternalsHost)
    return new MediaInternalsUI(web_ui);
#if !defined(TOOLKIT_QT)
  if (url.host_piece() == kChromeUIAccessibilityHost)
    return new AccessibilityUI(web_ui);
#endif
  if (url.host_piece() == kChromeUIServiceWorkerInternalsHost)
    return new ServiceWorkerInternalsUI(web_ui);
  if (url.host_piece() == kChromeUINetworkErrorsListingHost)
    return new NetworkErrorsListingUI(web_ui);
#if !defined(OS_ANDROID) && !defined(TOOLKIT_QT)
  if (url.host_piece() == kChromeUITracingHost)
    return new TracingUI(web_ui);
#endif

#if defined(ENABLE_WEBRTC)
  if (url.host_piece() == kChromeUIWebRTCInternalsHost)
    return new WebRTCInternalsUI(web_ui);
#endif

  return nullptr;
}

// static
ContentWebUIControllerFactory* ContentWebUIControllerFactory::GetInstance() {
  return base::Singleton<ContentWebUIControllerFactory>::get();
}

ContentWebUIControllerFactory::ContentWebUIControllerFactory() {
}

ContentWebUIControllerFactory::~ContentWebUIControllerFactory() {
}

}  // namespace content
