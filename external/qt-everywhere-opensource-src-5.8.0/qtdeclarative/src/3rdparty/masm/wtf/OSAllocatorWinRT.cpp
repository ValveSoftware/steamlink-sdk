/*
 * Copyright (C) 2010 Apple Inc. All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 *
 * THIS SOFTWARE IS PROVIDED BY APPLE INC. AND ITS CONTRIBUTORS ``AS IS''
 * AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO,
 * THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR
 * PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL APPLE INC. OR ITS CONTRIBUTORS
 * BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR
 * CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF
 * SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS
 * INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN
 * CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE)
 * ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF
 * THE POSSIBILITY OF SUCH DAMAGE.
 */

#include "config.h"
#include "OSAllocator.h"
#include "PageBlock.h"

#if OS(WINRT)

#include "windows.h"
#include <wtf/Assertions.h>

#if _MSC_VER >= 1900
// Try to use JIT by default and fallback to non-JIT on first error
static bool qt_winrt_use_jit = true;
#else // _MSC_VER < 1900
#  define PAGE_EXECUTE           0x10
#  define PAGE_EXECUTE_READ      0x20
#  define PAGE_EXECUTE_READWRITE 0x40
#  define MEM_RELEASE 0x8000
inline void* VirtualAllocFromApp(void*, size_t, int, int) { return 0; }
inline bool VirtualProtectFromApp(void *, size_t, int, DWORD*) { return false; }
inline bool VirtualFree(void *, size_t, DWORD) { return false; }
static bool qt_winrt_use_jit = false;
#endif // _MSC_VER < 1900

namespace WTF {

inline size_t getPageSize()
{
    SYSTEM_INFO info;
    GetNativeSystemInfo(&info);
    return info.dwPageSize;
}

static inline DWORD protection(bool writable, bool executable)
{
    if (writable && executable)
        qFatal("read/write executable areas are not allowed on WinRT");
    return executable ?
                (writable ? PAGE_EXECUTE_READWRITE : PAGE_EXECUTE_READ) :
                (writable ? PAGE_READWRITE : PAGE_READONLY);
}

void* OSAllocator::reserveUncommitted(size_t bytes, Usage, bool writable, bool executable)
{
    void *result;
    if (qt_winrt_use_jit) {
        result = VirtualAllocFromApp(0, bytes, MEM_RESERVE, protection(writable, executable));
        if (!result) {
            qt_winrt_use_jit = false;
            return reserveUncommitted(bytes, UnknownUsage, writable, executable);
        }
    } else {
        static const size_t pageSize = getPageSize();
        result = _aligned_malloc(bytes, pageSize);
        if (!result)
            CRASH();
        memset(result, 0, bytes);
    }
    return result;
}

void* OSAllocator::reserveAndCommit(size_t bytes, Usage usage, bool writable, bool executable, bool includesGuardPages)
{
    void *result;
    if (qt_winrt_use_jit) {
        result = VirtualAllocFromApp(0, bytes, MEM_RESERVE | MEM_COMMIT,
                                           protection(writable, executable));
        if (!result) {
            qt_winrt_use_jit = false;
            return reserveAndCommit(bytes, usage, writable, executable, includesGuardPages);
        }

        if (includesGuardPages) {
            size_t guardSize = pageSize();
            DWORD oldProtect;
            if (!VirtualProtectFromApp(result, guardSize, protection(false, false), &oldProtect) ||
                !VirtualProtectFromApp(static_cast<char*>(result) + bytes - guardSize, guardSize,
                                       protection(false, false), &oldProtect)) {
                CRASH();
            }
        }
    } else {
        result = reserveUncommitted(bytes, usage, writable, executable);
    }
    return result;
}

void OSAllocator::commit(void*, size_t, bool, bool)
{
    CRASH(); // Unimplemented
}

void OSAllocator::decommit(void* address, size_t)
{
    if (qt_winrt_use_jit)
        Q_UNREACHABLE();
    else
        _aligned_free(address);
}

void OSAllocator::releaseDecommitted(void* address, size_t bytes)
{
    if (qt_winrt_use_jit) {
        bool result = VirtualFree(address, 0, MEM_RELEASE);
        if (!result)
            CRASH();
    } else {
        decommit(address, bytes);
    }
}

bool OSAllocator::canAllocateExecutableMemory()
{
    if (qt_winrt_use_jit) {
        // For WinRT we first check if code generation is enabled. If successful
        // we allow to use JIT, otherwise fallback to the interpreter
        const size_t pageSize = getPageSize();
        void *all = VirtualAllocFromApp(0, pageSize, MEM_RESERVE | MEM_COMMIT,
                                        protection(true, false));
        DWORD oldProtect;
        bool res = VirtualProtectFromApp(all, pageSize, PAGE_EXECUTE, &oldProtect);
        VirtualFree(all, 0, MEM_RELEASE);
        if (!res) {
            qt_winrt_use_jit = false;
            qWarning("Could not enable JIT, fallback to interpreter mode. "
                     "Consider setting the code-generation capability");
        }
    }
    return qt_winrt_use_jit;
}


} // namespace WTF

#endif // OS(WINRT)
