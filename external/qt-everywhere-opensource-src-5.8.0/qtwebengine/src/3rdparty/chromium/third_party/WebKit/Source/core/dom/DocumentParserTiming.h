// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef DocumentParserTiming_h
#define DocumentParserTiming_h

#include "core/dom/Document.h"
#include "platform/Supplementable.h"
#include "platform/heap/Handle.h"
#include "wtf/Noncopyable.h"

namespace blink {

class LocalFrame;

// DocumentParserTiming is responsible for tracking parser-related timings for a
// given document.
class DocumentParserTiming final : public GarbageCollectedFinalized<DocumentParserTiming>, public Supplement<Document> {
    WTF_MAKE_NONCOPYABLE(DocumentParserTiming);
    USING_GARBAGE_COLLECTED_MIXIN(DocumentParserTiming);

public:
    virtual ~DocumentParserTiming() {}

    static DocumentParserTiming& from(Document&);

    // markParserStart and markParserStop methods record the time that the
    // parser was first started/stopped, and notify that the document parser
    // timing has changed. These methods do nothing (early return) if a time has
    // already been recorded for the given parser event, or if a parser has
    // already been detached.
    void markParserStart();
    void markParserStop();

    // markParserDetached records that the parser is detached from the
    // document. A single document may have multiple parsers, if e.g. the
    // document is re-opened using document.write. DocumentParserTiming only
    // wants to record parser start and stop time for the first parser. To avoid
    // recording parser start and stop times for re-opened documents, we keep
    // track of whether a parser has been detached, and avoid recording
    // start/stop times for subsequent parsers, after the first parser has been
    // detached.
    void markParserDetached();

    // Record a duration of time that the parser yielded due to loading a
    // script, in seconds. scriptInsertedViaDocumentWrite indicates whether the
    // script causing blocking was inserted via document.write. This may be
    // called multiple times, once for each time the parser yields on a script
    // load.
    void recordParserBlockedOnScriptLoadDuration(
        double duration, bool scriptInsertedViaDocumentWrite);

    // The getters below return monotonically-increasing seconds, or zero if the
    // given parser event has not yet occurred.  See the comments for
    // monotonicallyIncreasingTime in wtf/CurrentTime.h for additional details.

    double parserStart() const { return m_parserStart; }
    double parserStop() const { return m_parserStop; }

    // Returns the sum of all blocking script load durations reported via
    // recordParseBlockedOnScriptLoadDuration.
    double parserBlockedOnScriptLoadDuration() const { return m_parserBlockedOnScriptLoadDuration; }

    // Returns the sum of all blocking script load durations due to
    // document.write reported via recordParseBlockedOnScriptLoadDuration. Note
    // that some uncommon cases are not currently covered by this method. See
    // crbug/600711 for details.
    double parserBlockedOnScriptLoadFromDocumentWriteDuration() const { return m_parserBlockedOnScriptLoadFromDocumentWriteDuration; }

    DECLARE_VIRTUAL_TRACE();

private:
    explicit DocumentParserTiming(Document&);
    void notifyDocumentParserTimingChanged();

    double m_parserStart = 0.0;
    double m_parserStop = 0.0;
    double m_parserBlockedOnScriptLoadDuration = 0.0;
    double m_parserBlockedOnScriptLoadFromDocumentWriteDuration = 0.0;
    bool m_parserDetached = false;

    Member<Document> m_document;
};

} // namespace blink

#endif
