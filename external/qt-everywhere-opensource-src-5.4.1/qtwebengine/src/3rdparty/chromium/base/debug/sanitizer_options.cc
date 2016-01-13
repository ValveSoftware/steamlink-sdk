// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
//
// This file contains the default options for various compiler-based dynamic
// tools.

#include "build/build_config.h"

// Functions returning default options are declared weak in the tools' runtime
// libraries. To make the linker pick the strong replacements for those
// functions from this module, we explicitly force its inclusion by passing
// -Wl,-u_sanitizer_options_link_helper
extern "C"
void _sanitizer_options_link_helper() { }

#if defined(ADDRESS_SANITIZER)
// Default options for AddressSanitizer in various configurations:
//   strict_memcmp=1 - disable the strict memcmp() checking
//     (http://crbug.com/178677 and http://crbug.com/178404).
//   malloc_context_size=5 - limit the size of stack traces collected by ASan
//     for each malloc/free by 5 frames. These stack traces tend to accumulate
//     very fast in applications using JIT (v8 in Chrome's case), see
//     https://code.google.com/p/address-sanitizer/issues/detail?id=177
//   symbolize=false - disable the in-process symbolization, which isn't 100%
//     compatible with the existing sandboxes and doesn't make much sense for
//     stripped official binaries.
//   legacy_pthread_cond=1 - run in the libpthread 2.2.5 compatibility mode to
//     work around libGL.so using the obsolete API, see
//     http://crbug.com/341805. This may break if pthread_cond_t objects are
//     accessed by both instrumented and non-instrumented binaries (e.g. if
//     they reside in shared memory). This option is going to be deprecated in
//     upstream AddressSanitizer and must not be used anywhere except the
//     official builds.
//   replace_intrin=0 - do not intercept memcpy(), memmove() and memset() to
//     work around http://crbug.com/162461 (ASan report in OpenCL on Mac).
//   check_printf=1 - check the memory accesses to printf (and other formatted
//     output routines) arguments.
//   use_sigaltstack=1 - handle signals on an alternate signal stack. Useful
//     for stack overflow detection.
//   strip_path_prefix=Release/../../ - prefixes up to and including this
//     substring will be stripped from source file paths in symbolized reports
//     (if symbolize=true, which is set when running with LeakSanitizer).
#if defined(OS_LINUX)
#if defined(GOOGLE_CHROME_BUILD)
// Default AddressSanitizer options for the official build. These do not affect
// tests on buildbots (which don't set GOOGLE_CHROME_BUILD) or non-official
// Chromium builds.
const char kAsanDefaultOptions[] =
    "legacy_pthread_cond=1 malloc_context_size=5 strict_memcmp=0 "
    "symbolize=false check_printf=1 use_sigaltstack=1 detect_leaks=0 "
    "strip_path_prefix=Release/../../ ";
#else
// Default AddressSanitizer options for buildbots and non-official builds.
const char *kAsanDefaultOptions =
    "strict_memcmp=0 symbolize=false check_printf=1 use_sigaltstack=1 "
    "detect_leaks=0 strip_path_prefix=Release/../../ ";
#endif  // GOOGLE_CHROME_BUILD

#elif defined(OS_MACOSX)
const char *kAsanDefaultOptions =
    "strict_memcmp=0 replace_intrin=0 check_printf=1 use_sigaltstack=1 "
    "strip_path_prefix=Release/../../ ";
#endif  // OS_LINUX

#if defined(OS_LINUX) || defined(OS_MACOSX)
extern "C"
__attribute__((no_sanitize_address))
__attribute__((visibility("default")))
// The function isn't referenced from the executable itself. Make sure it isn't
// stripped by the linker.
__attribute__((used))
const char *__asan_default_options() {
  return kAsanDefaultOptions;
}
#endif  // OS_LINUX || OS_MACOSX
#endif  // ADDRESS_SANITIZER

#if defined(THREAD_SANITIZER) && defined(OS_LINUX)
// Default options for ThreadSanitizer in various configurations:
//   detect_deadlocks=1 - enable deadlock (lock inversion) detection.
//   second_deadlock_stack=1 - more verbose deadlock reports.
//   report_signal_unsafe=0 - do not report async-signal-unsafe functions
//     called from signal handlers.
//   report_thread_leaks=0 - do not report unjoined threads at the end of
//     the program execution.
//   print_suppressions=1 - print the list of matched suppressions.
//   strip_path_prefix=Release/../../ - prefixes up to and including this
//     substring will be stripped from source file paths in symbolized reports.
const char kTsanDefaultOptions[] =
    "detect_deadlocks=1 second_deadlock_stack=1 report_signal_unsafe=0 "
    "report_thread_leaks=0 print_suppressions=1 "
    "strip_path_prefix=Release/../../ ";

extern "C"
__attribute__((no_sanitize_thread))
__attribute__((visibility("default")))
// The function isn't referenced from the executable itself. Make sure it isn't
// stripped by the linker.
__attribute__((used))
const char *__tsan_default_options() {
  return kTsanDefaultOptions;
}

extern "C" char kTSanDefaultSuppressions[];

extern "C"
__attribute__((no_sanitize_thread))
__attribute__((visibility("default")))
// The function isn't referenced from the executable itself. Make sure it isn't
// stripped by the linker.
__attribute__((used))
const char *__tsan_default_suppressions() {
  return kTSanDefaultSuppressions;
}

#endif  // THREAD_SANITIZER && OS_LINUX
