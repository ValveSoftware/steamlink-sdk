// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "core/dom/DocumentParserTiming.h"

#include "core/dom/Document.h"
#include "core/loader/DocumentLoader.h"
#include "platform/TraceEvent.h"

namespace blink {

static const char kSupplementName[] = "DocumentParserTiming";

DocumentParserTiming& DocumentParserTiming::from(Document& document)
{
    DocumentParserTiming* timing = static_cast<DocumentParserTiming*>(Supplement<Document>::from(document, kSupplementName));
    if (!timing) {
        timing = new DocumentParserTiming(document);
        Supplement<Document>::provideTo(document, kSupplementName, timing);
    }
    return *timing;
}

void DocumentParserTiming::markParserStart()
{
    if (m_parserDetached || m_parserStart > 0.0)
        return;
    DCHECK_EQ(m_parserStop, 0.0);
    m_parserStart = monotonicallyIncreasingTime();
    notifyDocumentParserTimingChanged();
}

void DocumentParserTiming::markParserStop()
{
    if (m_parserDetached || m_parserStart == 0.0 || m_parserStop > 0.0)
        return;
    m_parserStop = monotonicallyIncreasingTime();
    notifyDocumentParserTimingChanged();
}

void DocumentParserTiming::markParserDetached()
{
    DCHECK_GT(m_parserStart, 0.0);
    m_parserDetached = true;
}

void DocumentParserTiming::recordParserBlockedOnScriptLoadDuration(
    double duration, bool scriptInsertedViaDocumentWrite)
{
    if (m_parserDetached || m_parserStart == 0.0 || m_parserStop > 0.0)
        return;
    m_parserBlockedOnScriptLoadDuration += duration;
    if (scriptInsertedViaDocumentWrite)
        m_parserBlockedOnScriptLoadFromDocumentWriteDuration += duration;
    notifyDocumentParserTimingChanged();
}

DEFINE_TRACE(DocumentParserTiming)
{
    visitor->trace(m_document);
}

DocumentParserTiming::DocumentParserTiming(Document& document)
    : m_document(document)
{
}

void DocumentParserTiming::notifyDocumentParserTimingChanged()
{
    if (m_document->loader())
        m_document->loader()->didChangePerformanceTiming();
}

} // namespace blink
