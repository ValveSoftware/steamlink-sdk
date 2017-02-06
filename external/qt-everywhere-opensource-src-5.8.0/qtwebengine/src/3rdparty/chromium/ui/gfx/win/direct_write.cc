// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/gfx/win/direct_write.h"

#include "base/command_line.h"
#include "base/metrics/field_trial.h"
#include "base/win/registry.h"
#include "base/win/scoped_comptr.h"
#include "base/win/windows_version.h"
#include "skia/ext/fontmgr_default_win.h"
#include "third_party/skia/include/ports/SkTypeface_win.h"
#include "ui/gfx/platform_font_win.h"
#include "ui/gfx/switches.h"

namespace gfx {
namespace win {

void CreateDWriteFactory(IDWriteFactory** factory) {
  using DWriteCreateFactoryProc = decltype(DWriteCreateFactory)*;
  HMODULE dwrite_dll = LoadLibraryW(L"dwrite.dll");
  if (!dwrite_dll)
    return;

  DWriteCreateFactoryProc dwrite_create_factory_proc =
      reinterpret_cast<DWriteCreateFactoryProc>(
          GetProcAddress(dwrite_dll, "DWriteCreateFactory"));
  // Not finding the DWriteCreateFactory function indicates a corrupt dll.
  if (!dwrite_create_factory_proc)
    return;

  // Failure to create the DirectWrite factory indicates a corrupt dll.
  base::win::ScopedComPtr<IUnknown> factory_unknown;
  if (FAILED(dwrite_create_factory_proc(DWRITE_FACTORY_TYPE_SHARED,
                                        __uuidof(IDWriteFactory),
                                        factory_unknown.Receive()))) {
    return;
  }
  factory_unknown.QueryInterface<IDWriteFactory>(factory);
}

void MaybeInitializeDirectWrite() {
  static bool tried_dwrite_initialize = false;
  if (tried_dwrite_initialize)
    return;
  tried_dwrite_initialize = true;

  if (base::CommandLine::ForCurrentProcess()->HasSwitch(
          switches::kDisableDirectWriteForUI)) {
    return;
  }

  base::win::ScopedComPtr<IDWriteFactory> factory;
  CreateDWriteFactory(factory.Receive());

  if (!factory)
    return;

  // The skia call to create a new DirectWrite font manager instance can fail
  // if we are unable to get the system font collection from the DirectWrite
  // factory. The GetSystemFontCollection method in the IDWriteFactory
  // interface fails with E_INVALIDARG on certain Windows 7 gold versions
  // (6.1.7600.*). We should just use GDI in these cases.
  SkFontMgr* direct_write_font_mgr = SkFontMgr_New_DirectWrite(factory.get());
  if (!direct_write_font_mgr)
    return;
  SetDefaultSkiaFactory(direct_write_font_mgr);
  gfx::PlatformFontWin::SetDirectWriteFactory(factory.get());
}

}  // namespace win
}  // namespace gfx
