// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome_elf/ntdll_cache.h"

#include <stdint.h>
#include <windows.h>

#include "base/basictypes.h"
#include "base/memory/scoped_ptr.h"
#include "base/win/pe_image.h"
#include "chrome_elf/thunk_getter.h"
#include "sandbox/win/src/interception_internal.h"
#include "sandbox/win/src/internal_types.h"
#include "sandbox/win/src/service_resolver.h"

FunctionLookupTable g_ntdll_lookup;

// Allocate storage for thunks in a page of this module to save on doing
// an extra allocation at run time.
#pragma section(".crthunk",read,execute)
__declspec(allocate(".crthunk")) sandbox::ThunkData g_nt_thunk_storage;



namespace {

bool EnumExportsCallback(const base::win::PEImage& image,
                         DWORD ordinal,
                         DWORD hint,
                         LPCSTR name,
                         PVOID function_addr,
                         LPCSTR forward,
                         PVOID cookie) {
  // Our lookup only cares about named functions that are in ntdll, so skip
  // unnamed or forwarded exports.
  if (name && function_addr)
    g_ntdll_lookup[std::string(name)] = function_addr;

  return true;
}

}  // namespace

void InitCache() {
  HMODULE ntdll_handle = ::GetModuleHandle(L"ntdll.dll");

  base::win::PEImage ntdll_image(ntdll_handle);

  ntdll_image.EnumExports(EnumExportsCallback, NULL);

  // If ntdll has already been patched, don't copy it.
  const bool kRelaxed = false;

  // Create a thunk via the appropriate ServiceResolver instance.
  scoped_ptr<sandbox::ServiceResolverThunk> thunk(GetThunk(kRelaxed));

  if (thunk.get()) {
    BYTE* thunk_storage = reinterpret_cast<BYTE*>(&g_nt_thunk_storage);

    // Mark the thunk storage as readable and writeable, since we
    // are ready to write to it.
    DWORD old_protect = 0;
    if (!::VirtualProtect(&g_nt_thunk_storage,
                          sizeof(g_nt_thunk_storage),
                          PAGE_EXECUTE_READWRITE,
                          &old_protect)) {
      return;
    }

    size_t storage_used = 0;
    NTSTATUS ret = thunk->CopyThunk(::GetModuleHandle(sandbox::kNtdllName),
                                    "NtCreateFile",
                                    thunk_storage,
                                    sizeof(sandbox::ThunkData),
                                    &storage_used);

    if (!NT_SUCCESS(ret)) {
      memset(&g_nt_thunk_storage, 0, sizeof(g_nt_thunk_storage));
    }

    // Ensure that the pointer to the old function can't be changed.
    ::VirtualProtect(&g_nt_thunk_storage,
                     sizeof(g_nt_thunk_storage),
                     PAGE_EXECUTE_READ,
                     &old_protect);
  }
}
