// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "wtf/SpinLock.h"

#include "wtf/Atomics.h"
#include "wtf/CPU.h"
#include "wtf/Compiler.h"

#if OS(WIN)
#include <windows.h>
#elif OS(POSIX)
#include <sched.h>
#endif

// The YIELD_PROCESSOR macro wraps an architecture specific-instruction that
// informs the processor we're in a busy wait, so it can handle the branch more
// intelligently and e.g. reduce power to our core or give more resources to the
// other hyper-thread on this core. See the following for context:
// https://software.intel.com/en-us/articles/benefitting-power-and-performance-sleep-loops
//
// The YIELD_THREAD macro tells the OS to relinquish our quanta. This is
// basically a worst-case fallback, and if you're hitting it with any frequency
// you really should be using proper lock rather than these spinlocks.
#if OS(WIN)
#define YIELD_PROCESSOR YieldProcessor()
#define YIELD_THREAD SwitchToThread()
#elif COMPILER(GCC) || COMPILER(CLANG)
#if CPU(X86_64) || CPU(X86)
#define YIELD_PROCESSOR __asm__ __volatile__("pause")
#elif CPU(ARM) || CPU(ARM64)
#define YIELD_PROCESSOR __asm__ __volatile__("yield")
#elif CPU(MIPS)
// The MIPS32 docs state that the PAUSE instruction is a no-op on older
// architectures (first added in MIPS32r2). To avoid assembler errors when
// targeting pre-r2, we must encode the instruction manually.
#define YIELD_PROCESSOR __asm__ __volatile__(".word 0x00000140")
#elif CPU(MIPS64) && __mips_isa_rev >= 2
// Don't bother doing using .word here since r2 is the lowest supported mips64
// that Chromium supports.
#define YIELD_PROCESSOR __asm__ __volatile__("pause")
#endif
#endif

#ifndef YIELD_PROCESSOR
#warning "Processor yield not supported on this architecture."
#define YIELD_PROCESSOR ((void)0)
#endif

#ifndef YIELD_THREAD
#if OS(POSIX)
#define YIELD_THREAD sched_yield()
#else
#warning "Thread yield not supported on this OS."
#define YIELD_THREAD ((void)0)
#endif
#endif

namespace WTF {

void SpinLock::lockSlow()
{
    // The value of kYieldProcessorTries is cargo culted from TCMalloc, Windows
    // critical section defaults, and various other recommendations.
    // TODO(jschuh): Further tuning may be warranted.
    static const int kYieldProcessorTries = 1000;
    do {
        do {
            for (int count = 0; count < kYieldProcessorTries; ++count) {
                // Let the Processor know we're spinning.
                YIELD_PROCESSOR;
                if (!m_lock.load(std::memory_order_relaxed) && LIKELY(!m_lock.exchange(true, std::memory_order_acquire)))
                    return;
            }

            // Give the OS a chance to schedule something on this core.
            YIELD_THREAD;
        } while (m_lock.load(std::memory_order_relaxed));
    } while (UNLIKELY(m_lock.exchange(true, std::memory_order_acquire)));
}

} // namespace WTF
