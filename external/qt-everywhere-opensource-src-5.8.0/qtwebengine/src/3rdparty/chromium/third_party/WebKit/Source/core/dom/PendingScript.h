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

#ifndef PendingScript_h
#define PendingScript_h

#include "bindings/core/v8/ScriptStreamer.h"
#include "core/CoreExport.h"
#include "core/fetch/ResourceOwner.h"
#include "core/fetch/ScriptResource.h"
#include "platform/heap/Handle.h"
#include "wtf/Noncopyable.h"
#include "wtf/text/TextPosition.h"

namespace blink {

class Element;
class ScriptSourceCode;

// A container for an external script which may be loaded and executed.
//
// TODO(kochi): The comment below is from pre-oilpan age and may not be correct now.
// A RefPtr alone does not prevent the underlying Resource
// from purging its data buffer. This class holds a dummy client open for its
// lifetime in order to guarantee that the data buffer will not be purged.
class CORE_EXPORT PendingScript final : public GarbageCollectedFinalized<PendingScript>, public ResourceOwner<ScriptResource> {
    USING_GARBAGE_COLLECTED_MIXIN(PendingScript);
    USING_PRE_FINALIZER(PendingScript, dispose);
    WTF_MAKE_NONCOPYABLE(PendingScript);
public:
    static PendingScript* create(Element*, ScriptResource*);
    ~PendingScript() override;

    TextPosition startingPosition() const { return m_startingPosition; }
    void setStartingPosition(const TextPosition& position) { m_startingPosition = position; }
    void markParserBlockingLoadStartTime();
    // Returns the time the load of this script started blocking the parser, or
    // zero if this script hasn't yet blocked the parser, in
    // monotonicallyIncreasingTime.
    double parserBlockingLoadStartTime() const { return m_parserBlockingLoadStartTime; }

    void watchForLoad(ScriptResourceClient*);
    void stopWatchingForLoad();

    Element* element() const { return m_element.get(); }
    void setElement(Element*);
    Element* releaseElementAndClear();

    void setScriptResource(ScriptResource*);

    void notifyFinished(Resource*) override;
    String debugName() const override { return "PendingScript"; }
    void notifyAppendData(ScriptResource*) override;

    DECLARE_TRACE();

    ScriptSourceCode getSource(const KURL& documentURL, bool& errorOccurred) const;

    void setStreamer(ScriptStreamer*);
    void streamingFinished();

    bool isReady() const;
    bool errorOccurred() const;

    void dispose();

private:
    PendingScript(Element*, ScriptResource*);

    bool m_watchingForLoad;
    Member<Element> m_element;
    TextPosition m_startingPosition; // Only used for inline script tags.
    bool m_integrityFailure;
    double m_parserBlockingLoadStartTime;

    Member<ScriptStreamer> m_streamer;
    Member<ScriptResourceClient> m_client;
};

} // namespace blink

#endif // PendingScript_h
