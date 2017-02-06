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

#include "core/loader/ProgressTracker.h"

#include "core/fetch/Resource.h"
#include "core/fetch/ResourceFetcher.h"
#include "core/frame/FrameView.h"
#include "core/frame/LocalFrame.h"
#include "core/frame/Settings.h"
#include "core/inspector/InspectorInstrumentation.h"
#include "core/loader/DocumentLoader.h"
#include "core/loader/FrameLoader.h"
#include "core/loader/FrameLoaderClient.h"
#include "platform/Logging.h"
#include "platform/network/ResourceResponse.h"
#include "wtf/CurrentTime.h"
#include "wtf/PtrUtil.h"
#include "wtf/text/CString.h"

using namespace std;

namespace blink {

// Always start progress at initialProgressValue. This helps provide feedback as
// soon as a load starts.
static const double initialProgressValue = 0.1;

static const int progressItemDefaultEstimatedLength = 1024 * 1024;

static const double progressNotificationInterval = 0.02;
static const double progressNotificationTimeInterval = 0.1;

struct ProgressItem {
    WTF_MAKE_NONCOPYABLE(ProgressItem); USING_FAST_MALLOC(ProgressItem);
public:
    ProgressItem(long long length)
        : bytesReceived(0)
        , estimatedLength(length) { }

    long long bytesReceived;
    long long estimatedLength;
};

ProgressTracker* ProgressTracker::create(LocalFrame* frame)
{
    return new ProgressTracker(frame);
}

ProgressTracker::ProgressTracker(LocalFrame* frame)
    : m_frame(frame)
    , m_lastNotifiedProgressValue(0)
    , m_lastNotifiedProgressTime(0)
    , m_finishedParsing(false)
    , m_progressValue(0)
{
}

ProgressTracker::~ProgressTracker()
{
}

DEFINE_TRACE(ProgressTracker)
{
    visitor->trace(m_frame);
}

void ProgressTracker::dispose()
{
    if (m_frame->isLoading())
        progressCompleted();
    ASSERT(!m_frame->isLoading());
}

double ProgressTracker::estimatedProgress() const
{
    return m_progressValue;
}

void ProgressTracker::reset()
{
    m_progressItems.clear();
    m_progressValue = 0;
    m_lastNotifiedProgressValue = 0;
    m_lastNotifiedProgressTime = 0;
    m_finishedParsing = false;
}

void ProgressTracker::progressStarted()
{
    if (!m_frame->isLoading())
        m_frame->loader().client()->didStartLoading(NavigationToDifferentDocument);
    reset();
    m_progressValue = initialProgressValue;
    m_frame->setIsLoading(true);
    InspectorInstrumentation::frameStartedLoading(m_frame);
}

void ProgressTracker::progressCompleted()
{
    ASSERT(m_frame->isLoading());
    m_frame->setIsLoading(false);
    sendFinalProgress();
    reset();
    m_frame->loader().client()->didStopLoading();
    InspectorInstrumentation::frameStoppedLoading(m_frame);
}

void ProgressTracker::finishedParsing()
{
    m_finishedParsing = true;
    maybeSendProgress();
}

void ProgressTracker::sendFinalProgress()
{
    if (m_progressValue == 1)
        return;
    m_progressValue = 1;
    m_frame->loader().client()->progressEstimateChanged(m_progressValue);
}

void ProgressTracker::willStartLoading(unsigned long identifier)
{
    if (!m_frame->isLoading())
        return;
    // All of the progress bar completion policies besides LoadEvent instead block on parsing
    // completion, which corresponds to finishing parsing. For those policies, don't consider
    // resource load that start after DOMContentLoaded finishes.
    if (m_frame->settings()->progressBarCompletion() != ProgressBarCompletion::LoadEvent && m_finishedParsing)
        return;
    DCHECK(!m_progressItems.get(identifier));
    m_progressItems.set(identifier, wrapUnique(new ProgressItem(progressItemDefaultEstimatedLength)));
}

void ProgressTracker::incrementProgress(unsigned long identifier, const ResourceResponse& response)
{
    ProgressItem* item = m_progressItems.get(identifier);
    if (!item)
        return;

    long long estimatedLength = response.expectedContentLength();
    if (estimatedLength < 0)
        estimatedLength = progressItemDefaultEstimatedLength;
    item->bytesReceived = 0;
    item->estimatedLength = estimatedLength;
}

void ProgressTracker::incrementProgress(unsigned long identifier, int length)
{
    ProgressItem* item = m_progressItems.get(identifier);
    if (!item)
        return;

    item->bytesReceived += length;
    if (item->bytesReceived > item->estimatedLength)
        item->estimatedLength = item->bytesReceived * 2;
    maybeSendProgress();
}

void ProgressTracker::maybeSendProgress()
{
    m_progressValue = initialProgressValue + 0.1; // +0.1 for committing
    if (m_finishedParsing)
        m_progressValue += 0.2;

    long long bytesReceived = 0;
    long long estimatedBytesForPendingRequests = 0;
    for (const auto& progressItem : m_progressItems) {
        bytesReceived += progressItem.value->bytesReceived;
        estimatedBytesForPendingRequests += progressItem.value->estimatedLength;
    }
    DCHECK_GE(estimatedBytesForPendingRequests, 0);
    DCHECK_GE(estimatedBytesForPendingRequests, bytesReceived);

    if (m_finishedParsing) {
        if (m_frame->settings()->progressBarCompletion() == ProgressBarCompletion::DOMContentLoaded) {
            sendFinalProgress();
            return;
        }
        if (m_frame->settings()->progressBarCompletion() != ProgressBarCompletion::LoadEvent && estimatedBytesForPendingRequests == bytesReceived) {
            sendFinalProgress();
            return;
        }
    }

    double percentOfBytesReceived = !estimatedBytesForPendingRequests ? 1.0 : (double)bytesReceived / (double)estimatedBytesForPendingRequests;
    m_progressValue += percentOfBytesReceived / 2;

    DCHECK_GE(m_progressValue, initialProgressValue);
    // Always leave space at the end. This helps show the user that we're not
    // done until we're done.
    DCHECK(m_progressValue <= 0.9);
    if (m_progressValue < m_lastNotifiedProgressValue)
        return;

    double now = currentTime();
    double notifiedProgressTimeDelta = now - m_lastNotifiedProgressTime;

    double notificationProgressDelta = m_progressValue - m_lastNotifiedProgressValue;
    if (notificationProgressDelta >= progressNotificationInterval || notifiedProgressTimeDelta >= progressNotificationTimeInterval) {
        m_frame->loader().client()->progressEstimateChanged(m_progressValue);
        m_lastNotifiedProgressValue = m_progressValue;
        m_lastNotifiedProgressTime = now;
    }
}

void ProgressTracker::completeProgress(unsigned long identifier)
{
    ProgressItem* item = m_progressItems.get(identifier);
    if (!item)
        return;

    item->estimatedLength = item->bytesReceived;
    maybeSendProgress();
}

} // namespace blink
