// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef SharedContextRateLimiter_h
#define SharedContextRateLimiter_h

#include "gpu/command_buffer/client/gles2_interface.h"
#include "wtf/Allocator.h"
#include "wtf/Deque.h"
#include "wtf/Noncopyable.h"
#include <memory>

namespace blink {

class WebGraphicsContext3DProvider;

// Purpose: to limit the amount of worked queued for execution
//   (backlog) on the GPU by blocking the main thread to allow the GPU
//   to catch up. The Prevents unsynchronized tight animation loops
//   from cause a GPU denial of service.
//
// How it works: The rate limiter uses GPU fences to mark each tick
//   and makes sure there are never more that 'maxPendingTicks' fences
//   that are awaiting completion. On platforms that do not support
//   fences, we use glFinish instead. glFinish will only be called in
//   unsynchronized cases that submit more than maxPendingTicks animation
//   tick per compositor frame, which should be quite rare.
//
// How to use it: Each unit of work that constitutes a complete animation
//   frame must call tick(). reset() must be called when the animation
//   is consumed by committing to the compositor. Several rate limiters can
//   be used concurrently: they will each use their own sequences of
//   fences which may be interleaved. When the graphics context is lost
//   and later restored, the existing rate limiter must be destroyed and
//   a new one created.

class SharedContextRateLimiter final {
    USING_FAST_MALLOC(SharedContextRateLimiter);
    WTF_MAKE_NONCOPYABLE(SharedContextRateLimiter);
public:
    static std::unique_ptr<SharedContextRateLimiter> create(unsigned maxPendingTicks);
    void tick();
    void reset();
private:
    SharedContextRateLimiter(unsigned maxPendingTicks);

    std::unique_ptr<WebGraphicsContext3DProvider> m_contextProvider;
    Deque<GLuint> m_queries;
    unsigned m_maxPendingTicks;
    bool m_canUseSyncQueries;
};

} // namespace blink

#endif
