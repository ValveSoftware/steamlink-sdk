// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/ppapi_plugin/ppapi_webkitplatformsupport_impl.h"

#include <map>

#include "base/logging.h"
#include "base/strings/string16.h"
#include "base/threading/platform_thread.h"
#include "build/build_config.h"
#include "content/child/child_thread.h"
#include "content/common/child_process_messages.h"
#include "ppapi/proxy/plugin_globals.h"
#include "ppapi/shared_impl/proxy_lock.h"
#include "third_party/WebKit/public/platform/WebString.h"

#if defined(OS_WIN)
#include "third_party/WebKit/public/platform/win/WebSandboxSupport.h"
#elif defined(OS_MACOSX)
#include "third_party/WebKit/public/platform/mac/WebSandboxSupport.h"
#elif defined(OS_ANDROID)
#include "third_party/WebKit/public/platform/android/WebSandboxSupport.h"
#elif defined(OS_POSIX)
#include "content/common/child_process_sandbox_support_impl_linux.h"
#include "third_party/WebKit/public/platform/linux/WebFallbackFont.h"
#include "third_party/WebKit/public/platform/linux/WebSandboxSupport.h"
#include "third_party/icu/source/common/unicode/utf16.h"
#endif

using blink::WebSandboxSupport;
using blink::WebString;
using blink::WebUChar;
using blink::WebUChar32;

typedef struct CGFont* CGFontRef;

namespace content {

class PpapiWebKitPlatformSupportImpl::SandboxSupport
    : public WebSandboxSupport {
 public:
  virtual ~SandboxSupport() {}

#if defined(OS_WIN)
  virtual bool ensureFontLoaded(HFONT);
#elif defined(OS_MACOSX)
  virtual bool loadFont(
      NSFont* srcFont, CGFontRef* out, uint32_t* fontID);
#elif defined(OS_ANDROID)
  // Empty class.
#elif defined(OS_POSIX)
  SandboxSupport();
  virtual void getFallbackFontForCharacter(
      WebUChar32 character,
      const char* preferred_locale,
      blink::WebFallbackFont* fallbackFont);
  virtual void getRenderStyleForStrike(
      const char* family, int sizeAndStyle, blink::WebFontRenderStyle* out);

 private:
  // WebKit likes to ask us for the correct font family to use for a set of
  // unicode code points. It needs this information frequently so we cache it
  // here.
  std::map<int32_t, blink::WebFallbackFont> unicode_font_families_;
  // For debugging crbug.com/312965
  base::PlatformThreadId creation_thread_;
#endif
};

#if defined(OS_WIN)

bool PpapiWebKitPlatformSupportImpl::SandboxSupport::ensureFontLoaded(
    HFONT font) {
  LOGFONT logfont;
  GetObject(font, sizeof(LOGFONT), &logfont);

  // Use the proxy sender rather than going directly to the ChildThread since
  // the proxy browser sender will properly unlock during sync messages.
  return ppapi::proxy::PluginGlobals::Get()->GetBrowserSender()->Send(
      new ChildProcessHostMsg_PreCacheFont(logfont));
}

#elif defined(OS_MACOSX)

bool PpapiWebKitPlatformSupportImpl::SandboxSupport::loadFont(
    NSFont* src_font,
    CGFontRef* out,
    uint32_t* font_id) {
  // TODO(brettw) this should do the something similar to what
  // RendererWebKitClientImpl does and request that the browser load the font.
  // Note: need to unlock the proxy lock like ensureFontLoaded does.
  NOTIMPLEMENTED();
  return false;
}

#elif defined(OS_ANDROID)

// Empty class.

#elif defined(OS_POSIX)

PpapiWebKitPlatformSupportImpl::SandboxSupport::SandboxSupport()
    : creation_thread_(base::PlatformThread::CurrentId()) {
}

void
PpapiWebKitPlatformSupportImpl::SandboxSupport::getFallbackFontForCharacter(
    WebUChar32 character,
    const char* preferred_locale,
    blink::WebFallbackFont* fallbackFont) {
  ppapi::ProxyLock::AssertAcquired();
  // For debugging crbug.com/312965
  CHECK_EQ(creation_thread_, base::PlatformThread::CurrentId());
  const std::map<int32_t, blink::WebFallbackFont>::const_iterator iter =
      unicode_font_families_.find(character);
  if (iter != unicode_font_families_.end()) {
    fallbackFont->name = iter->second.name;
    fallbackFont->filename = iter->second.filename;
    fallbackFont->ttcIndex = iter->second.ttcIndex;
    fallbackFont->isBold = iter->second.isBold;
    fallbackFont->isItalic = iter->second.isItalic;
    return;
  }

  GetFallbackFontForCharacter(character, preferred_locale, fallbackFont);
  unicode_font_families_.insert(std::make_pair(character, *fallbackFont));
}

void PpapiWebKitPlatformSupportImpl::SandboxSupport::getRenderStyleForStrike(
    const char* family, int sizeAndStyle, blink::WebFontRenderStyle* out) {
  GetRenderStyleForStrike(family, sizeAndStyle, out);
}

#endif

PpapiWebKitPlatformSupportImpl::PpapiWebKitPlatformSupportImpl()
    : sandbox_support_(new PpapiWebKitPlatformSupportImpl::SandboxSupport()) {
}

PpapiWebKitPlatformSupportImpl::~PpapiWebKitPlatformSupportImpl() {
}

void PpapiWebKitPlatformSupportImpl::Shutdown() {
  // SandboxSupport contains a map of WebFontFamily objects, which hold
  // WebCStrings, which become invalidated when blink is shut down. Hence, we
  // need to clear that map now, just before blink::shutdown() is called.
  sandbox_support_.reset();
}

blink::WebClipboard* PpapiWebKitPlatformSupportImpl::clipboard() {
  NOTREACHED();
  return NULL;
}

blink::WebMimeRegistry* PpapiWebKitPlatformSupportImpl::mimeRegistry() {
  NOTREACHED();
  return NULL;
}

blink::WebFileUtilities* PpapiWebKitPlatformSupportImpl::fileUtilities() {
  NOTREACHED();
  return NULL;
}

blink::WebSandboxSupport* PpapiWebKitPlatformSupportImpl::sandboxSupport() {
  return sandbox_support_.get();
}

bool PpapiWebKitPlatformSupportImpl::sandboxEnabled() {
  return true;  // Assume PPAPI is always sandboxed.
}

unsigned long long PpapiWebKitPlatformSupportImpl::visitedLinkHash(
    const char* canonical_url,
    size_t length) {
  NOTREACHED();
  return 0;
}

bool PpapiWebKitPlatformSupportImpl::isLinkVisited(
    unsigned long long link_hash) {
  NOTREACHED();
  return false;
}

void PpapiWebKitPlatformSupportImpl::createMessageChannel(
    blink::WebMessagePortChannel** channel1,
    blink::WebMessagePortChannel** channel2) {
  NOTREACHED();
  *channel1 = NULL;
  *channel2 = NULL;
}

void PpapiWebKitPlatformSupportImpl::setCookies(
    const blink::WebURL& url,
    const blink::WebURL& first_party_for_cookies,
    const blink::WebString& value) {
  NOTREACHED();
}

blink::WebString PpapiWebKitPlatformSupportImpl::cookies(
    const blink::WebURL& url,
    const blink::WebURL& first_party_for_cookies) {
  NOTREACHED();
  return blink::WebString();
}

blink::WebString PpapiWebKitPlatformSupportImpl::defaultLocale() {
  NOTREACHED();
  return blink::WebString();
}

blink::WebThemeEngine* PpapiWebKitPlatformSupportImpl::themeEngine() {
  NOTREACHED();
  return NULL;
}

blink::WebURLLoader* PpapiWebKitPlatformSupportImpl::createURLLoader() {
  NOTREACHED();
  return NULL;
}

blink::WebSocketStreamHandle*
    PpapiWebKitPlatformSupportImpl::createSocketStreamHandle() {
  NOTREACHED();
  return NULL;
}

void PpapiWebKitPlatformSupportImpl::getPluginList(bool refresh,
    blink::WebPluginListBuilder* builder) {
  NOTREACHED();
}

blink::WebData PpapiWebKitPlatformSupportImpl::loadResource(const char* name) {
  NOTREACHED();
  return blink::WebData();
}

blink::WebStorageNamespace*
PpapiWebKitPlatformSupportImpl::createLocalStorageNamespace() {
  NOTREACHED();
  return 0;
}

void PpapiWebKitPlatformSupportImpl::dispatchStorageEvent(
    const blink::WebString& key, const blink::WebString& old_value,
    const blink::WebString& new_value, const blink::WebString& origin,
    const blink::WebURL& url, bool is_local_storage) {
  NOTREACHED();
}

int PpapiWebKitPlatformSupportImpl::databaseDeleteFile(
    const blink::WebString& vfs_file_name, bool sync_dir) {
  NOTREACHED();
  return 0;
}

}  // namespace content
