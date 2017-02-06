// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_PPAPI_PLUGIN_PPAPI_BLINK_PLATFORM_IMPL_H_
#define CONTENT_PPAPI_PLUGIN_PPAPI_BLINK_PLATFORM_IMPL_H_

#include <stddef.h>

#include <memory>

#include "base/macros.h"
#include "build/build_config.h"
#include "content/child/blink_platform_impl.h"

namespace content {

class PpapiBlinkPlatformImpl : public BlinkPlatformImpl {
 public:
  PpapiBlinkPlatformImpl();
  ~PpapiBlinkPlatformImpl() override;

  // Shutdown must be called just prior to shutting down blink.
  void Shutdown();

  // BlinkPlatformImpl methods:
  blink::WebThread* currentThread() override;
  blink::WebClipboard* clipboard() override;
  blink::WebMimeRegistry* mimeRegistry() override;
  blink::WebFileUtilities* fileUtilities() override;
  blink::WebSandboxSupport* sandboxSupport() override;
  virtual bool sandboxEnabled();
  unsigned long long visitedLinkHash(const char* canonicalURL,
                                     size_t length) override;
  bool isLinkVisited(unsigned long long linkHash) override;
  void createMessageChannel(blink::WebMessagePortChannel** channel1,
                            blink::WebMessagePortChannel** channel2) override;
  virtual void setCookies(const blink::WebURL& url,
                          const blink::WebURL& first_party_for_cookies,
                          const blink::WebString& value);
  virtual blink::WebString cookies(
      const blink::WebURL& url,
      const blink::WebURL& first_party_for_cookies);
  blink::WebString defaultLocale() override;
  blink::WebThemeEngine* themeEngine() override;
  blink::WebURLLoader* createURLLoader() override;
  void getPluginList(bool refresh, blink::WebPluginListBuilder*) override;
  blink::WebData loadResource(const char* name) override;
  blink::WebStorageNamespace* createLocalStorageNamespace() override;
  virtual void dispatchStorageEvent(const blink::WebString& key,
      const blink::WebString& oldValue, const blink::WebString& newValue,
      const blink::WebString& origin, const blink::WebURL& url,
      bool isLocalStorage);
  int databaseDeleteFile(const blink::WebString& vfs_file_name,
                         bool sync_dir) override;

 private:
#if !defined(OS_ANDROID) && !defined(OS_WIN)
  class SandboxSupport;
  std::unique_ptr<SandboxSupport> sandbox_support_;
#endif

  DISALLOW_COPY_AND_ASSIGN(PpapiBlinkPlatformImpl);
};

}  // namespace content

#endif  // CONTENT_PPAPI_PLUGIN_PPAPI_BLINK_PLATFORM_IMPL_H_
