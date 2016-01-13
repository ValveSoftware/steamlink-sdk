// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/public/renderer/render_font_warmup_win.h"

#include <dwrite.h>

#include "base/logging.h"
#include "base/win/iat_patch_function.h"
#include "base/win/windows_version.h"
#include "third_party/WebKit/public/web/win/WebFontRendering.h"
#include "third_party/skia/include/core/SkPaint.h"
#include "third_party/skia/include/ports/SkFontMgr.h"
#include "third_party/skia/include/ports/SkTypeface_win.h"

namespace content {

namespace {

SkFontMgr* g_warmup_fontmgr = NULL;

base::win::IATPatchFunction g_iat_patch_open_sc_manager;
base::win::IATPatchFunction g_iat_patch_close_service_handle;
base::win::IATPatchFunction g_iat_patch_open_service;
base::win::IATPatchFunction g_iat_patch_start_service;
base::win::IATPatchFunction g_iat_patch_nt_connect_port;

// These are from ntddk.h
#if !defined(STATUS_ACCESS_DENIED)
#define STATUS_ACCESS_DENIED   ((NTSTATUS)0xC0000022L)
#endif

typedef LONG NTSTATUS;

SC_HANDLE WINAPI OpenSCManagerWPatch(const wchar_t* machine_name,
                                     const wchar_t* database_name,
                                     DWORD access_mask) {
  ::SetLastError(0);
  return reinterpret_cast<SC_HANDLE>(0xdeadbeef);
}

SC_HANDLE WINAPI OpenServiceWPatch(SC_HANDLE sc_manager,
                                   const wchar_t* service_name,
                                   DWORD access_mask) {
  ::SetLastError(0);
  return reinterpret_cast<SC_HANDLE>(0xdeadbabe);
}

BOOL WINAPI CloseServiceHandlePatch(SC_HANDLE service_handle) {
  if (service_handle != reinterpret_cast<SC_HANDLE>(0xdeadbabe) &&
      service_handle != reinterpret_cast<SC_HANDLE>(0xdeadbeef))
    CHECK(false);
  ::SetLastError(0);
  return TRUE;
}

BOOL WINAPI StartServiceWPatch(SC_HANDLE service,
                               DWORD args,
                               const wchar_t** arg_vectors) {
  if (service != reinterpret_cast<SC_HANDLE>(0xdeadbabe))
    CHECK(false);
  ::SetLastError(ERROR_ACCESS_DENIED);
  return FALSE;
}

NTSTATUS WINAPI NtALpcConnectPortPatch(HANDLE* port_handle,
                                       void* port_name,
                                       void* object_attribs,
                                       void* port_attribs,
                                       DWORD flags,
                                       void* server_sid,
                                       void* message,
                                       DWORD* buffer_length,
                                       void* out_message_attributes,
                                       void* in_message_attributes,
                                       void* time_out) {
  return STATUS_ACCESS_DENIED;
}

// Directwrite connects to the font cache service to retrieve information about
// fonts installed on the system etc. This works well outside the sandbox and
// within the sandbox as long as the lpc connection maintained by the current
// process with the font cache service remains valid. It appears that there
// are cases when this connection is dropped after which directwrite is unable
// to connect to the font cache service which causes problems with characters
// disappearing.
// Directwrite has fallback code to enumerate fonts if it is unable to connect
// to the font cache service. We need to intercept the following APIs to
// ensure that it does not connect to the font cache service.
// NtALpcConnectPort
// OpenSCManagerW
// OpenServiceW
// StartServiceW
// CloseServiceHandle.
// These are all IAT patched.
void PatchServiceManagerCalls() {
  static bool is_patched = false;
  if (is_patched)
    return;
  const char* service_provider_dll =
      (base::win::GetVersion() >= base::win::VERSION_WIN8 ?
          "api-ms-win-service-management-l1-1-0.dll" : "advapi32.dll");

  is_patched = true;

  DWORD patched = g_iat_patch_open_sc_manager.Patch(L"dwrite.dll",
      service_provider_dll, "OpenSCManagerW", OpenSCManagerWPatch);
  DCHECK(patched == 0);

  patched = g_iat_patch_close_service_handle.Patch(L"dwrite.dll",
      service_provider_dll, "CloseServiceHandle", CloseServiceHandlePatch);
  DCHECK(patched == 0);

  patched = g_iat_patch_open_service.Patch(L"dwrite.dll",
      service_provider_dll, "OpenServiceW", OpenServiceWPatch);
  DCHECK(patched == 0);

  patched = g_iat_patch_start_service.Patch(L"dwrite.dll",
      service_provider_dll, "StartServiceW", StartServiceWPatch);
  DCHECK(patched == 0);

  patched = g_iat_patch_nt_connect_port.Patch(L"dwrite.dll",
      "ntdll.dll", "NtAlpcConnectPort", NtALpcConnectPortPatch);
  DCHECK(patched == 0);
}


// Windows-only DirectWrite support. These warm up the DirectWrite paths
// before sandbox lock down to allow Skia access to the Font Manager service.
void CreateDirectWriteFactory(IDWriteFactory** factory) {
  typedef decltype(DWriteCreateFactory)* DWriteCreateFactoryProc;
  PatchServiceManagerCalls();

  DWriteCreateFactoryProc dwrite_create_factory_proc =
      reinterpret_cast<DWriteCreateFactoryProc>(
          GetProcAddress(LoadLibraryW(L"dwrite.dll"), "DWriteCreateFactory"));
  CHECK(dwrite_create_factory_proc);
  CHECK(SUCCEEDED(
      dwrite_create_factory_proc(DWRITE_FACTORY_TYPE_ISOLATED,
                                 __uuidof(IDWriteFactory),
                                 reinterpret_cast<IUnknown**>(factory))));
}

}  // namespace

void DoPreSandboxWarmupForTypeface(SkTypeface* typeface) {
  SkPaint paint_warmup;
  paint_warmup.setTypeface(typeface);
  wchar_t glyph = L'S';
  paint_warmup.measureText(&glyph, 2);
}

SkFontMgr* GetPreSandboxWarmupFontMgr() {
  if (!g_warmup_fontmgr) {
    IDWriteFactory* factory;
    CreateDirectWriteFactory(&factory);
    blink::WebFontRendering::setDirectWriteFactory(factory);
    g_warmup_fontmgr = SkFontMgr_New_DirectWrite(factory);
  }
  return g_warmup_fontmgr;
}

}  // namespace content
