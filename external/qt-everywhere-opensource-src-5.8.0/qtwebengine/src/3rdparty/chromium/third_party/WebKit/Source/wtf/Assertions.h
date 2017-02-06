/*
 * Copyright (C) 2003, 2006, 2007 Apple Inc.  All rights reserved.
 * Copyright (C) 2013 Google Inc. All rights reserved.
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

#ifndef WTF_Assertions_h
#define WTF_Assertions_h

/*
   No namespaces because this file has to be includable from C and Objective-C.

   Note, this file uses many GCC extensions, but it should be compatible with
   C, Objective C, C++, and Objective C++.

   For non-debug builds, everything is disabled by default, except for the
   RELEASE_ASSERT family of macros.

   Defining any of the symbols explicitly prevents this from having any effect.

*/

#include "base/gtest_prod_util.h"
#include "base/logging.h"
#include "wtf/Compiler.h"
#include "wtf/Noncopyable.h"
#include "wtf/WTFExport.h"
#include "wtf/build_config.h"
#include <stdarg.h>

// Users must test "#if ENABLE(ASSERT)", which helps ensure that code
// testing this macro has included this header.
#ifndef ENABLE_ASSERT
#if defined(NDEBUG) && !defined(DCHECK_ALWAYS_ON)
/* Disable ASSERT* macros in release mode by default. */
#define ENABLE_ASSERT 0
#else
#define ENABLE_ASSERT 1
#endif /* defined(NDEBUG) && !defined(DCHECK_ALWAYS_ON) */
#endif

#ifndef ASSERT_MSG_DISABLED
#define ASSERT_MSG_DISABLED !ENABLE(ASSERT)
#endif

#ifndef LOG_DISABLED
#define LOG_DISABLED !ENABLE(ASSERT)
#endif

#if COMPILER(GCC)
#define WTF_ATTRIBUTE_PRINTF(formatStringArgument, extraArguments) __attribute__((__format__(printf, formatStringArgument, extraArguments)))
#else
#define WTF_ATTRIBUTE_PRINTF(formatStringArgument, extraArguments)
#endif

/* These helper functions are always declared, but not necessarily always defined if the corresponding function is disabled. */

typedef enum { WTFLogChannelOff, WTFLogChannelOn } WTFLogChannelState;

typedef struct {
    WTFLogChannelState state;
} WTFLogChannel;

WTF_EXPORT void WTFReportAssertionFailure(const char* file, int line, const char* function, const char* assertion);
WTF_EXPORT void WTFLog(WTFLogChannel*, const char* format, ...) WTF_ATTRIBUTE_PRINTF(2, 3);
WTF_EXPORT void WTFLogAlways(const char* format, ...) WTF_ATTRIBUTE_PRINTF(1, 2);

WTF_EXPORT void WTFGetBacktrace(void** stack, int* size);
WTF_EXPORT void WTFReportBacktrace(int framesToShow = 31);
WTF_EXPORT void WTFPrintBacktrace(void** stack, int size);

namespace WTF {

#if LOG_DISABLED

#define WTF_CREATE_SCOPED_LOGGER(...) ((void) 0)
#define WTF_CREATE_SCOPED_LOGGER_IF(...) ((void) 0)
#define WTF_APPEND_SCOPED_LOGGER(...) ((void) 0)

#else

// ScopedLogger wraps log messages in parentheses, with indentation proportional
// to the number of instances. This makes it easy to see the flow of control in
// the output, particularly when instrumenting recursive functions.
//
// NOTE: This class is a debugging tool, not intended for use by checked-in
// code. Please do not remove it.
//
class WTF_EXPORT ScopedLogger {
    WTF_MAKE_NONCOPYABLE(ScopedLogger);
public:
    // The first message is passed to the constructor.  Additional messages for
    // the same scope can be added with log(). If condition is false, produce no
    // output and do not create a scope.
    ScopedLogger(bool condition, const char* format, ...) WTF_ATTRIBUTE_PRINTF(3, 4);
    ~ScopedLogger();
    void log(const char* format, ...) WTF_ATTRIBUTE_PRINTF(2, 3);

private:
    FRIEND_TEST_ALL_PREFIXES(AssertionsTest, ScopedLogger);
    using PrintFunctionPtr = void (*)(const char* format, va_list args);
    static void setPrintFuncForTests(PrintFunctionPtr p) { m_printFunc = p; } // Note: not thread safe.

    void init(const char* format, va_list args);
    void writeNewlineIfNeeded();
    void indent();
    void print(const char* format, ...);
    void printIndent();
    static ScopedLogger*& current();

    ScopedLogger* const m_parent;
    bool m_multiline; // The ')' will go on the same line if there is only one entry.
    static PrintFunctionPtr m_printFunc;
};

#define WTF_CREATE_SCOPED_LOGGER(name, ...) WTF::ScopedLogger name(true, __VA_ARGS__)
#define WTF_CREATE_SCOPED_LOGGER_IF(name, condition, ...) WTF::ScopedLogger name(condition, __VA_ARGS__)
#define WTF_APPEND_SCOPED_LOGGER(name, ...) (name.log(__VA_ARGS__))

#endif // LOG_DISABLED

} // namespace WTF

/* IMMEDIATE_CRASH() - Like CRASH() below but crashes in the fastest, simplest possible way with no attempt at logging. */
#ifndef IMMEDIATE_CRASH
#if COMPILER(GCC) || COMPILER(CLANG)
#define IMMEDIATE_CRASH() __builtin_trap()
#else
#define IMMEDIATE_CRASH() ((void)(*(volatile char*)0 = 0))
#endif
#endif

/* CRASH() - Raises a fatal error resulting in program termination and triggering either the debugger or the crash reporter.

   Use CRASH() in response to known, unrecoverable errors like out-of-memory.
   Macro is enabled in both debug and release mode.
   To test for unknown errors and verify assumptions, use ASSERT instead, to avoid impacting performance in release builds.

   Signals are ignored by the crash reporter on OS X so we must do better.
*/
#ifndef CRASH
#if COMPILER(MSVC)
#define CRASH() (__debugbreak(), IMMEDIATE_CRASH())
#else
#define CRASH() \
    (WTFReportBacktrace(), (*(int*)0xfbadbeef = 0), IMMEDIATE_CRASH())
#endif
#endif

#if COMPILER(CLANG)
#define NO_RETURN_DUE_TO_CRASH NO_RETURN
#else
#define NO_RETURN_DUE_TO_CRASH
#endif

// ASSERT, ASSERT_NOT_REACHED, ASSERT_UNUSED
//  These macros are compiled out of release builds.
//  Expressions inside them are evaluated in debug builds only.
//  They are deprecated. We should use:
//    - DCHECK() for ASSERT()
//    - NOTREACHED() for ASSERT_NOT_REACHED()
//    - DCHECK() and ALLOW_UNUSED_LOCAL() for ASSERT_UNUSED().
#if OS(WIN)
/* FIXME: Change to use something other than ASSERT to avoid this conflict with the underlying platform */
#undef ASSERT
#endif

#if ENABLE(ASSERT)

#define ASSERT(assertion) \
    (!(assertion) ? \
        (WTFReportAssertionFailure(__FILE__, __LINE__, WTF_PRETTY_FUNCTION, #assertion), \
            CRASH()) : \
        (void)0)

#define DCHECK_AT(assertion, file, line) LAZY_STREAM(logging::LogMessage(file, line, #assertion).stream(), !(assertion))

#define ASSERT_NOT_REACHED() do { \
    WTFReportAssertionFailure(__FILE__, __LINE__, WTF_PRETTY_FUNCTION, 0); \
    CRASH(); \
} while (0)

#define ASSERT_UNUSED(variable, assertion) ASSERT(assertion)

#define NO_RETURN_DUE_TO_ASSERT NO_RETURN_DUE_TO_CRASH

#else

#define ASSERT(assertion) ((void)0)
#define DCHECK_AT(assertion, file, line) EAT_STREAM_PARAMETERS
#define ASSERT_NOT_REACHED() ((void)0)
#define NO_RETURN_DUE_TO_ASSERT

#define ASSERT_UNUSED(variable, assertion) ((void)variable)

#endif

// ASSERT_WITH_SECURITY_IMPLICATION
// It is deprecated.  ASSERT_WITH_SECURITY_IMPLICATION should be replaced
// with SECURITY_DCHECK.
#ifdef ADDRESS_SANITIZER

#define ASSERT_WITH_SECURITY_IMPLICATION(assertion) \
    (!(assertion) ? \
        (WTFReportAssertionFailure(__FILE__, __LINE__, WTF_PRETTY_FUNCTION, #assertion), \
            CRASH()) : \
        (void)0)

#else
#define ASSERT_WITH_SECURITY_IMPLICATION(assertion) ASSERT(assertion)
#endif

// Users must test "#if ENABLE(SECURITY_ASSERT)", which helps ensure
// that code testing this macro has included this header.
#if defined(ADDRESS_SANITIZER) || ENABLE(ASSERT)
#define ENABLE_SECURITY_ASSERT 1
#else
#define ENABLE_SECURITY_ASSERT 0
#endif

// SECURITY_DCHECK and SECURITY_CHECK
// Use in places where failure of the assertion indicates a possible security
// vulnerability. Classes of these vulnerabilities include bad casts, out of
// bounds accesses, use-after-frees, etc. Please be sure to file bugs for these
// failures using the security template:
//    https://bugs.chromium.org/p/chromium/issues/entry?template=Security%20Bug
#if ENABLE_SECURITY_ASSERT
#define SECURITY_DCHECK(condition) LOG_IF(FATAL, !(condition)) << "Security check failed: " #condition ". "
// TODO(tkent): Should we make SECURITY_CHECK different from SECURITY_DCHECK?
// A SECURITY_CHECK failure is actually not vulnerable.
#define SECURITY_CHECK(condition) SECURITY_DCHECK(condition)
#else
#define SECURITY_DCHECK(condition) ((void)0)
#define SECURITY_CHECK(condition) CHECK(condition)
#endif

// WTF_LOG
// This is deprecated.  Should be replaced with DVLOG(verboselevel), which works
// only in debug build, or VLOG(verboselevel), which works in release build too.
#if LOG_DISABLED
#define WTF_LOG(channel, ...) ((void)0)
#else
#define WTF_LOG(channel, ...) WTFLog(&JOIN_LOG_CHANNEL_WITH_PREFIX(LOG_CHANNEL_PREFIX, channel), __VA_ARGS__)
#define JOIN_LOG_CHANNEL_WITH_PREFIX(prefix, channel) JOIN_LOG_CHANNEL_WITH_PREFIX_LEVEL_2(prefix, channel)
#define JOIN_LOG_CHANNEL_WITH_PREFIX_LEVEL_2(prefix, channel) prefix ## channel
#endif

/* RELEASE_ASSERT

   Use in places where failure of an assertion indicates a definite security
   vulnerability from which execution must not continue even in a release build.
   Please sure to file bugs for these failures using the security template:
      http://code.google.com/p/chromium/issues/entry?template=Security%20Bug
*/
// RELEASE_ASSERT is deprecated.  We should use CHECK() instead.
#if ENABLE(ASSERT)
#define RELEASE_ASSERT(assertion) ASSERT(assertion)
#else
#define RELEASE_ASSERT(assertion) (UNLIKELY(!(assertion)) ? (IMMEDIATE_CRASH()) : (void)0)
#endif
// TODO(tkent): Move this to base/logging.h?
#define RELEASE_NOTREACHED() LOG(FATAL)

/* DEFINE_COMPARISON_OPERATORS_WITH_REFERENCES */

// Allow equality comparisons of Objects by reference or pointer, interchangeably.
// This can be only used on types whose equality makes no other sense than pointer equality.
#define DEFINE_COMPARISON_OPERATORS_WITH_REFERENCES(thisType) \
    inline bool operator==(const thisType& a, const thisType& b) { return &a == &b; } \
    inline bool operator==(const thisType& a, const thisType* b) { return &a == b; } \
    inline bool operator==(const thisType* a, const thisType& b) { return a == &b; } \
    inline bool operator!=(const thisType& a, const thisType& b) { return !(a == b); } \
    inline bool operator!=(const thisType& a, const thisType* b) { return !(a == b); } \
    inline bool operator!=(const thisType* a, const thisType& b) { return !(a == b); }

#define DEFINE_COMPARISON_OPERATORS_WITH_REFERENCES_REFCOUNTED(thisType) \
    DEFINE_COMPARISON_OPERATORS_WITH_REFERENCES(thisType) \
    inline bool operator==(const PassRefPtr<thisType>& a, const thisType& b) { return a.get() == &b; } \
    inline bool operator==(const thisType& a, const PassRefPtr<thisType>& b) { return &a == b.get(); } \
    inline bool operator!=(const PassRefPtr<thisType>& a, const thisType& b) { return !(a == b); } \
    inline bool operator!=(const thisType& a, const PassRefPtr<thisType>& b) { return !(a == b); }

/* DEFINE_TYPE_CASTS */

#define DEFINE_TYPE_CASTS(thisType, argumentType, argumentName, pointerPredicate, referencePredicate) \
inline thisType* to##thisType(argumentType* argumentName) \
{ \
    ASSERT_WITH_SECURITY_IMPLICATION(!argumentName || (pointerPredicate)); \
    return static_cast<thisType*>(argumentName); \
} \
inline const thisType* to##thisType(const argumentType* argumentName) \
{ \
    ASSERT_WITH_SECURITY_IMPLICATION(!argumentName || (pointerPredicate)); \
    return static_cast<const thisType*>(argumentName); \
} \
inline thisType& to##thisType(argumentType& argumentName) \
{ \
    ASSERT_WITH_SECURITY_IMPLICATION(referencePredicate); \
    return static_cast<thisType&>(argumentName); \
} \
inline const thisType& to##thisType(const argumentType& argumentName) \
{ \
    ASSERT_WITH_SECURITY_IMPLICATION(referencePredicate); \
    return static_cast<const thisType&>(argumentName); \
} \
void to##thisType(const thisType*); \
void to##thisType(const thisType&)

#endif /* WTF_Assertions_h */
