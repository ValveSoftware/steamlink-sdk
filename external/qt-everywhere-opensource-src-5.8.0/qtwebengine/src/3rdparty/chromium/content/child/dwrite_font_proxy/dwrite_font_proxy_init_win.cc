// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/child/dwrite_font_proxy/dwrite_font_proxy_init_win.h"

#include <dwrite.h>

#include "base/bind.h"
#include "base/callback.h"
#include "base/debug/alias.h"
#include "base/win/iat_patch_function.h"
#include "base/win/windows_version.h"
#include "content/child/child_thread_impl.h"
#include "content/child/dwrite_font_proxy/dwrite_font_proxy_win.h"
#include "content/child/dwrite_font_proxy/font_fallback_win.h"
#include "content/child/font_warmup_win.h"
#include "content/child/thread_safe_sender.h"
#include "skia/ext/fontmgr_default_win.h"
#include "third_party/WebKit/public/web/win/WebFontRendering.h"
#include "third_party/skia/include/ports/SkFontMgr.h"
#include "third_party/skia/include/ports/SkTypeface_win.h"

namespace mswr = Microsoft::WRL;

namespace content {

namespace {

mswr::ComPtr<DWriteFontCollectionProxy> g_font_collection;
IPC::Sender* g_sender_override = nullptr;

// Windows-only DirectWrite support. These warm up the DirectWrite paths
// before sandbox lock down to allow Skia access to the Font Manager service.
void CreateDirectWriteFactory(IDWriteFactory** factory) {
  typedef decltype(DWriteCreateFactory)* DWriteCreateFactoryProc;
  HMODULE dwrite_dll = LoadLibraryW(L"dwrite.dll");
  // TODO(scottmg): Temporary code to track crash in http://crbug.com/387867.
  if (!dwrite_dll) {
    DWORD load_library_get_last_error = GetLastError();
    base::debug::Alias(&dwrite_dll);
    base::debug::Alias(&load_library_get_last_error);
    CHECK(false);
  }

  // This shouldn't be necessary, but not having this causes breakage in
  // content_browsertests, and possibly other high-stress cases.
  PatchServiceManagerCalls();

  DWriteCreateFactoryProc dwrite_create_factory_proc =
      reinterpret_cast<DWriteCreateFactoryProc>(
          GetProcAddress(dwrite_dll, "DWriteCreateFactory"));
  // TODO(scottmg): Temporary code to track crash in http://crbug.com/387867.
  if (!dwrite_create_factory_proc) {
    DWORD get_proc_address_get_last_error = GetLastError();
    base::debug::Alias(&dwrite_create_factory_proc);
    base::debug::Alias(&get_proc_address_get_last_error);
    CHECK(false);
  }
  CHECK(SUCCEEDED(dwrite_create_factory_proc(
      DWRITE_FACTORY_TYPE_ISOLATED, __uuidof(IDWriteFactory),
      reinterpret_cast<IUnknown**>(factory))));
}

}  // namespace

void InitializeDWriteFontProxy() {
  mswr::ComPtr<IDWriteFactory> factory;

  CreateDirectWriteFactory(&factory);

  IPC::Sender* sender = g_sender_override;

  // Hack for crbug.com/631254: set the sender if we can get one, so that when
  // Flash calls into the font proxy from a different thread we will have a
  // sender available.
  if (!sender && ChildThreadImpl::current())
    sender = ChildThreadImpl::current()->thread_safe_sender();

  if (!g_font_collection) {
    mswr::MakeAndInitialize<DWriteFontCollectionProxy>(
        &g_font_collection, factory.Get(), sender);
  }

  mswr::ComPtr<IDWriteFontFallback> font_fallback;
  mswr::ComPtr<IDWriteFactory2> factory2;

  if (SUCCEEDED(factory.As(&factory2)) && factory2.Get()) {
    mswr::MakeAndInitialize<FontFallback>(
        &font_fallback, g_font_collection.Get(), sender);
  }

  sk_sp<SkFontMgr> skia_font_manager(SkFontMgr_New_DirectWrite(
      factory.Get(), g_font_collection.Get(), font_fallback.Get()));
  blink::WebFontRendering::setSkiaFontManager(skia_font_manager.get());

  // Add an extra ref for SetDefaultSkiaFactory, which keeps a ref but doesn't
  // addref.
  skia_font_manager->ref();
  SetDefaultSkiaFactory(skia_font_manager.get());

  // When IDWriteFontFallback is not available (prior to Win8.1) Skia will
  // still attempt to use DirectWrite to determine fallback fonts (in
  // SkFontMgr_DirectWrite::onMatchFamilyStyleCharacter), which will likely
  // result in trying to load the system font collection. To avoid that and
  // instead fall back on WebKit's fallback logic, we don't use Skia's font
  // fallback if IDWriteFontFallback is not available.
  // This flag can be removed when Win8.0 and earlier are no longer supported.
  bool fallback_available = font_fallback.Get() != nullptr;
  DCHECK_EQ(fallback_available,
    base::win::GetVersion() > base::win::VERSION_WIN8);
  blink::WebFontRendering::setUseSkiaFontFallback(fallback_available);
}

void UninitializeDWriteFontProxy() {
  if (g_font_collection)
    g_font_collection->Unregister();
}

void SetDWriteFontProxySenderForTesting(IPC::Sender* sender) {
  g_sender_override = sender;
}

}  // namespace content
