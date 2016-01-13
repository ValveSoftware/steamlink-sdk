// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_ELF_CREATE_FILE_CHROME_CREATE_FILE_H_
#define CHROME_ELF_CREATE_FILE_CHROME_CREATE_FILE_H_

#include <windows.h>

#include "chrome_elf/chrome_elf_types.h"

// A CreateFileW replacement that will call NTCreateFile directly when the
// criteria defined in ShouldBypass() are satisfied for |lp_file_name|.
extern "C" HANDLE WINAPI CreateFileWRedirect(
    LPCWSTR file_name,
    DWORD desired_access,
    DWORD share_mode,
    LPSECURITY_ATTRIBUTES security_attributes,
    DWORD creation_disposition,
    DWORD flags_and_attributes,
    HANDLE template_file);

// Returns the count of CreateFile calls redirected so far.
extern "C" int GetRedirectCount();

// Partial reimplementation of kernel32!CreateFile (very partial: only handles
// reading and writing to files in the User Data directory).
HANDLE CreateFileNTDLL(
    LPCWSTR file_name,
    DWORD desired_access,
    DWORD share_mode,
    LPSECURITY_ATTRIBUTES security_attributes,
    DWORD creation_disposition,
    DWORD flags_and_attributes,
    HANDLE template_file);

// Determines whether or not we should use our version of CreateFile, or the
// system version (only uses ours if we're writing to the user data directory).
bool ShouldBypass(LPCWSTR file_name);

#endif  // CHROME_ELF_CREATE_FILE_CHROME_CREATE_FILE_H_
