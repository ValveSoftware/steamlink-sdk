// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_ELF_NTDLL_CACHE_H_
#define CHROME_ELF_NTDLL_CACHE_H_

#include "chrome_elf/chrome_elf_types.h"

namespace sandbox {
struct ThunkData;
}

// Caches the addresses of all functions exported by ntdll in  |g_ntdll_lookup|.
void InitCache();

extern FunctionLookupTable g_ntdll_lookup;

extern sandbox::ThunkData g_nt_thunk_storage;

#endif  // CHROME_ELF_NTDLL_CACHE_H_
