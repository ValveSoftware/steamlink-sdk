/*
 * Copyright (C) 2007 Apple Inc.  All rights reserved.
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

#ifndef ProgressTracker_h
#define ProgressTracker_h

#include "core/CoreExport.h"
#include "platform/heap/Handle.h"
#include "wtf/Allocator.h"
#include "wtf/Forward.h"
#include "wtf/HashMap.h"
#include "wtf/Noncopyable.h"
#include <memory>

namespace blink {

class LocalFrame;
class Resource;
class ResourceResponse;
struct ProgressItem;

// FIXME: This is only used on Android. Android is the only Chrome
// browser which shows a progress bar during loading.
// We should find a better way for Android to get this data and remove this!
class CORE_EXPORT ProgressTracker final : public GarbageCollectedFinalized<ProgressTracker> {
    WTF_MAKE_NONCOPYABLE(ProgressTracker);
public:
    static ProgressTracker* create(LocalFrame*);

    ~ProgressTracker();
    DECLARE_TRACE();
    void dispose();

    double estimatedProgress() const;

    void progressStarted();
    void progressCompleted();

    void finishedParsing();

    void willStartLoading(unsigned long identifier);
    void incrementProgress(unsigned long identifier, const ResourceResponse&);
    void incrementProgress(unsigned long identifier, int);
    void completeProgress(unsigned long identifier);

private:
    explicit ProgressTracker(LocalFrame*);

    void incrementProgressForMainResourceOnly(unsigned long identifier, int length);
    void maybeSendProgress();
    void sendFinalProgress();
    void reset();

    Member<LocalFrame> m_frame;
    double m_lastNotifiedProgressValue;
    double m_lastNotifiedProgressTime;
    bool m_finishedParsing;
    double m_progressValue;

    HashMap<unsigned long, std::unique_ptr<ProgressItem>> m_progressItems;
};

} // namespace blink

#endif
