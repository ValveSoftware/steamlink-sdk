/*
 * Copyright (C) 2003, 2006, 2007 Apple Inc.  All rights reserved.
 * Copyright (C) 2007-2009 Torch Mobile, Inc.
 * Copyright (C) 2011 University of Szeged. All rights reserved.
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
 * THIS SOFTWARE IS PROVIDED BY APPLE COMPUTER, INC. ``AS IS'' AND ANY
 * EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR
 * PURPOSE ARE DISCLAIMED.  IN NO EVENT SHALL APPLE COMPUTER, INC. OR
 * CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL,
 * EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO,
 * PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR
 * PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY
 * OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
 * OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

// The vprintf_stderr_common function triggers this error in the Mac build.
// Feel free to remove this pragma if this file builds on Mac.
// According to http://gcc.gnu.org/onlinedocs/gcc-4.2.1/gcc/Diagnostic-Pragmas.html#Diagnostic-Pragmas
// we need to place this directive before any data or functions are defined.
#pragma GCC diagnostic ignored "-Wmissing-format-attribute"

#include "wtf/Assertions.h"

#include "wtf/Compiler.h"
#include "wtf/PtrUtil.h"
#include "wtf/ThreadSpecific.h"
#include "wtf/Threading.h"
#include <memory>
#include <stdarg.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#if OS(MACOSX)
#include <AvailabilityMacros.h>
#if __MAC_OS_X_VERSION_MIN_REQUIRED >= 1080
#define WTF_USE_APPLE_SYSTEM_LOG 1
#include <asl.h>
#endif
#endif // OS(MACOSX)

#if COMPILER(MSVC)
#include <crtdbg.h>
#endif

#if OS(WIN)
#include <windows.h>
#endif

#if OS(MACOSX) || (OS(LINUX) && !defined(__UCLIBC__))
#include <cxxabi.h>
#include <dlfcn.h>
#include <execinfo.h>
#endif

#if OS(ANDROID)
#include <android/log.h>
#endif

WTF_ATTRIBUTE_PRINTF(1, 0)
static void vprintf_stderr_common(const char* format, va_list args)
{
#if OS(MACOSX) && USE(APPLE_SYSTEM_LOG)
    va_list copyOfArgs;
    va_copy(copyOfArgs, args);
    asl_vlog(0, 0, ASL_LEVEL_NOTICE, format, copyOfArgs);
    va_end(copyOfArgs);
#elif OS(ANDROID)
    __android_log_vprint(ANDROID_LOG_WARN, "WebKit", format, args);
#elif OS(WIN)
    if (IsDebuggerPresent()) {
        size_t size = 1024;

        do {
            char* buffer = (char*)malloc(size);
            if (!buffer)
                break;

            if (_vsnprintf(buffer, size, format, args) != -1) {
                OutputDebugStringA(buffer);
                free(buffer);
                break;
            }

            free(buffer);
            size *= 2;
        } while (size > 1024);
    }
#endif
    vfprintf(stderr, format, args);
}

#if COMPILER(CLANG) || (COMPILER(GCC) && GCC_VERSION_AT_LEAST(4, 6, 0))
#pragma GCC diagnostic push
#pragma GCC diagnostic ignored "-Wformat-nonliteral"
#endif

static void vprintf_stderr_with_trailing_newline(const char* format, va_list args)
{
    size_t formatLength = strlen(format);
    if (formatLength && format[formatLength - 1] == '\n') {
        vprintf_stderr_common(format, args);
        return;
    }

    std::unique_ptr<char[]> formatWithNewline = wrapArrayUnique(new char[formatLength + 2]);
    memcpy(formatWithNewline.get(), format, formatLength);
    formatWithNewline[formatLength] = '\n';
    formatWithNewline[formatLength + 1] = 0;

    vprintf_stderr_common(formatWithNewline.get(), args);
}

#if COMPILER(CLANG) || (COMPILER(GCC) && GCC_VERSION_AT_LEAST(4, 6, 0))
#pragma GCC diagnostic pop
#endif

WTF_ATTRIBUTE_PRINTF(1, 2)
static void printf_stderr_common(const char* format, ...)
{
    va_list args;
    va_start(args, format);
    vprintf_stderr_common(format, args);
    va_end(args);
}

static void printCallSite(const char* file, int line, const char* function)
{
#if OS(WIN) && defined(_DEBUG)
    _CrtDbgReport(_CRT_WARN, file, line, nullptr, "%s\n", function);
#else
    // By using this format, which matches the format used by MSVC for compiler errors, developers
    // using Visual Studio can double-click the file/line number in the Output Window to have the
    // editor navigate to that line of code. It seems fine for other developers, too.
    printf_stderr_common("%s(%d) : %s\n", file, line, function);
#endif
}

void WTFReportAssertionFailure(const char* file, int line, const char* function, const char* assertion)
{
    if (assertion)
        printf_stderr_common("ASSERTION FAILED: %s\n", assertion);
    else
        printf_stderr_common("SHOULD NEVER BE REACHED\n");
    printCallSite(file, line, function);
}

void WTFGetBacktrace(void** stack, int* size)
{
#if OS(MACOSX) || (OS(LINUX) && !defined(__UCLIBC__))
    *size = backtrace(stack, *size);
#elif OS(WIN)
    // The CaptureStackBackTrace function is available in XP, but it is not defined
    // in the Windows Server 2003 R2 Platform SDK. So, we'll grab the function
    // through GetProcAddress.
    typedef WORD (NTAPI* RtlCaptureStackBackTraceFunc)(DWORD, DWORD, PVOID*, PDWORD);
    HMODULE kernel32 = ::GetModuleHandleW(L"Kernel32.dll");
    if (!kernel32) {
        *size = 0;
        return;
    }
    RtlCaptureStackBackTraceFunc captureStackBackTraceFunc = reinterpret_cast<RtlCaptureStackBackTraceFunc>(
        ::GetProcAddress(kernel32, "RtlCaptureStackBackTrace"));
    if (captureStackBackTraceFunc)
        *size = captureStackBackTraceFunc(0, *size, stack, 0);
    else
        *size = 0;
#else
    *size = 0;
#endif
}

void WTFReportBacktrace(int framesToShow)
{
    static const int framesToSkip = 2;
    // Use alloca to allocate on the stack since this function is used in OOM situations.
    void** samples = static_cast<void**>(alloca((framesToShow + framesToSkip) * sizeof(void *)));
    int frames = framesToShow + framesToSkip;

    WTFGetBacktrace(samples, &frames);
    WTFPrintBacktrace(samples + framesToSkip, frames - framesToSkip);
}

namespace {

class FrameToNameScope {
public:
    explicit FrameToNameScope(void*);
    ~FrameToNameScope();
    const char* nullableName() { return m_name; }

private:
    const char* m_name;
    char* m_cxaDemangled;
};

FrameToNameScope::FrameToNameScope(void* addr)
    : m_name(0)
    , m_cxaDemangled(0)
{
#if OS(MACOSX) || (OS(LINUX) && !defined(__UCLIBC__))
    Dl_info info;
    if (!dladdr(addr, &info) || !info.dli_sname)
        return;
    const char* mangledName = info.dli_sname;
    if ((m_cxaDemangled = abi::__cxa_demangle(mangledName, 0, 0, 0)))
        m_name = m_cxaDemangled;
    else
        m_name = mangledName;
#else
    (void)addr;
#endif
}

FrameToNameScope::~FrameToNameScope()
{
    free(m_cxaDemangled);
}

} // anonymous namespace

#if !LOG_DISABLED
namespace WTF {

ScopedLogger::ScopedLogger(bool condition, const char* format, ...)
    : m_parent(condition ? current() : 0)
    , m_multiline(false)
{
    if (!condition)
        return;

    va_list args;
    va_start(args, format);
    init(format, args);
    va_end(args);
}

ScopedLogger::~ScopedLogger()
{
    if (current() == this) {
        if (m_multiline)
            indent();
        else
            print(" ");
        print(")\n");
        current() = m_parent;
    }
}

void ScopedLogger::init(const char* format, va_list args)
{
    current() = this;
    if (m_parent)
        m_parent->writeNewlineIfNeeded();
    indent();
    print("( ");
    m_printFunc(format, args);
}

void ScopedLogger::writeNewlineIfNeeded()
{
    if (!m_multiline) {
        print("\n");
        m_multiline = true;
    }
}

void ScopedLogger::indent()
{
    if (m_parent) {
        m_parent->indent();
        printIndent();
    }
}

void ScopedLogger::log(const char* format, ...)
{
    if (current() != this)
        return;

    va_list args;
    va_start(args, format);

    writeNewlineIfNeeded();
    indent();
    printIndent();
    m_printFunc(format, args);
    print("\n");

    va_end(args);
}

void ScopedLogger::print(const char* format, ...)
{
    va_list args;
    va_start(args, format);
    m_printFunc(format, args);
    va_end(args);
}

void ScopedLogger::printIndent()
{
    print("  ");
}

ScopedLogger*& ScopedLogger::current()
{
    DEFINE_THREAD_SAFE_STATIC_LOCAL(ThreadSpecific<ScopedLogger*>, ref, new ThreadSpecific<ScopedLogger*>);
    return *ref;
}

ScopedLogger::PrintFunctionPtr ScopedLogger::m_printFunc = vprintf_stderr_common;

} // namespace WTF
#endif // !LOG_DISABLED

void WTFPrintBacktrace(void** stack, int size)
{
    for (int i = 0; i < size; ++i) {
        FrameToNameScope frameToName(stack[i]);
        const int frameNumber = i + 1;
        if (frameToName.nullableName())
            printf_stderr_common("%-3d %p %s\n", frameNumber, stack[i], frameToName.nullableName());
        else
            printf_stderr_common("%-3d %p\n", frameNumber, stack[i]);
    }
}

void WTFLog(WTFLogChannel* channel, const char* format, ...)
{
    if (channel->state != WTFLogChannelOn)
        return;

    va_list args;
    va_start(args, format);
    vprintf_stderr_with_trailing_newline(format, args);
    va_end(args);
}

void WTFLogAlways(const char* format, ...)
{
    va_list args;
    va_start(args, format);
    vprintf_stderr_with_trailing_newline(format, args);
    va_end(args);
}
