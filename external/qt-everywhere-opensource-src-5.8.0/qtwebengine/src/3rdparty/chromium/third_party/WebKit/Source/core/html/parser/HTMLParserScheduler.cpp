/*
 * Copyright (C) 2010 Google, Inc. All Rights Reserved.
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
 * THIS SOFTWARE IS PROVIDED BY APPLE INC. ``AS IS'' AND ANY
 * EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR
 * PURPOSE ARE DISCLAIMED.  IN NO EVENT SHALL APPLE INC. OR
 * CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL,
 * EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO,
 * PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR
 * PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY
 * OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
 * OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

#include "core/html/parser/HTMLParserScheduler.h"

#include "core/dom/Document.h"
#include "core/frame/FrameView.h"
#include "core/html/parser/HTMLDocumentParser.h"
#include "public/platform/Platform.h"
#include "public/platform/WebScheduler.h"
#include "public/platform/WebThread.h"
#include "wtf/CurrentTime.h"
#include "wtf/PtrUtil.h"

namespace blink {

PumpSession::PumpSession(unsigned& nestingLevel)
    : NestingLevelIncrementer(nestingLevel)
{
}

PumpSession::~PumpSession()
{
}

SpeculationsPumpSession::SpeculationsPumpSession(unsigned& nestingLevel)
    : NestingLevelIncrementer(nestingLevel)
    , m_startTime(currentTime())
    , m_processedElementTokens(0)
{
}

SpeculationsPumpSession::~SpeculationsPumpSession()
{
}

inline double SpeculationsPumpSession::elapsedTime() const
{
    return currentTime() - m_startTime;
}

void SpeculationsPumpSession::addedElementTokens(size_t count)
{
    m_processedElementTokens += count;
}

HTMLParserScheduler::HTMLParserScheduler(HTMLDocumentParser* parser, WebTaskRunner* loadingTaskRunner)
    : m_parser(parser)
    , m_loadingTaskRunner(wrapUnique(loadingTaskRunner->clone()))
    , m_cancellableContinueParse(CancellableTaskFactory::create(this, &HTMLParserScheduler::continueParsing))
    , m_isSuspendedWithActiveTimer(false)
{
}

HTMLParserScheduler::~HTMLParserScheduler()
{
}

DEFINE_TRACE(HTMLParserScheduler)
{
    visitor->trace(m_parser);
}

void HTMLParserScheduler::scheduleForResume()
{
    ASSERT(!m_isSuspendedWithActiveTimer);
    m_loadingTaskRunner->postTask(BLINK_FROM_HERE, m_cancellableContinueParse->cancelAndCreate());
}

void HTMLParserScheduler::suspend()
{
    ASSERT(!m_isSuspendedWithActiveTimer);
    if (!m_cancellableContinueParse->isPending())
        return;
    m_isSuspendedWithActiveTimer = true;
    m_cancellableContinueParse->cancel();
}

void HTMLParserScheduler::resume()
{
    ASSERT(!m_cancellableContinueParse->isPending());
    if (!m_isSuspendedWithActiveTimer)
        return;
    m_isSuspendedWithActiveTimer = false;

    m_loadingTaskRunner->postTask(BLINK_FROM_HERE, m_cancellableContinueParse->cancelAndCreate());
}

void HTMLParserScheduler::detach()
{
    m_cancellableContinueParse->cancel();
    m_isSuspendedWithActiveTimer = false;
}

inline bool HTMLParserScheduler::shouldYield(const SpeculationsPumpSession& session, bool startingScript) const
{
    if (Platform::current()->currentThread()->scheduler()->shouldYieldForHighPriorityWork())
        return true;

    const double parserTimeLimit = 0.5;
    if (session.elapsedTime() > parserTimeLimit)
        return true;

    // Yield if a lot of DOM work has been done in this session and a script tag is
    // about to be parsed. This significantly improves render performance for documents
    // that place their scripts at the bottom of the page. Yielding too often
    // significantly slows down the parsing so a balance needs to be struck to
    // only yield when enough changes have happened to make it worthwhile.
    // Emperical testing shows that anything > ~40 and < ~200 gives all of the benefit
    // without impacting parser performance, only adding a few yields per page but at
    // just the right times.
    const size_t sufficientWork = 50;
    if (startingScript && session.processedElementTokens() > sufficientWork)
        return true;

    return false;
}

bool HTMLParserScheduler::yieldIfNeeded(const SpeculationsPumpSession& session, bool startingScript)
{
    if (shouldYield(session, startingScript)) {
        scheduleForResume();
        return true;
    }

    return false;
}

void HTMLParserScheduler::forceResumeAfterYield()
{
    ASSERT(!m_cancellableContinueParse->isPending());
    m_isSuspendedWithActiveTimer = true;
}

void HTMLParserScheduler::continueParsing()
{
    m_parser->resumeParsingAfterYield();
}

} // namespace blink
