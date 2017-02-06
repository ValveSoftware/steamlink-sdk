/*
 * Copyright (C) 2013 Google Inc. All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions are
 * met:
 *
 *     * Redistributions of source code must retain the above copyright
 * notice, this list of conditions and the following disclaimer.
 *     * Redistributions in binary form must reproduce the above
 * copyright notice, this list of conditions and the following disclaimer
 * in the documentation and/or other materials provided with the
 * distribution.
 *     * Neither the name of Google Inc. nor the names of its
 * contributors may be used to endorse or promote products derived from
 * this software without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS
 * "AS IS" AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT
 * LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR
 * A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT
 * OWNER OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL,
 * SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT
 * LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE,
 * DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY
 * THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
 * OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

#ifndef WTF_SpinLock_h
#define WTF_SpinLock_h

#include "wtf/Compiler.h"
#include "wtf/WTFExport.h"
#include <atomic>
#include <memory>
#include <mutex>

// DESCRIPTION
// Spinlock is a simple spinlock class based on the standard CPU primitive of
// atomic increment and decrement of an int at a given memory address. These are
// intended only for very short duration locks and assume a system with multiple
// cores. For any potentially longer wait you should be using a real lock.

namespace WTF {

class SpinLock {
public:
    using Guard = std::lock_guard<SpinLock>;

    ALWAYS_INLINE void lock()
    {
        static_assert(sizeof(m_lock) == sizeof(int), "int and m_lock are different sizes");
        if (LIKELY(!m_lock.exchange(true, std::memory_order_acquire)))
            return;
        lockSlow();
    }

    ALWAYS_INLINE void unlock()
    {
        m_lock.store(false, std::memory_order_release);
    }

private:
    // This is called if the initial attempt to acquire the lock fails. It's
    // slower, but has a much better scheduling and power consumption behavior.
    WTF_EXPORT void lockSlow();

    std::atomic_int m_lock;
};


} // namespace WTF

using WTF::SpinLock;

#endif // WTF_SpinLock_h
