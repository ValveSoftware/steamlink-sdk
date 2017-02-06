/*
 * Copyright (C) 2013 Google Inc. All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions are met:
 *
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 *
 * THIS SOFTWARE IS PROVIDED BY APPLE INC. AND ITS CONTRIBUTORS ``AS IS'' AND
 * ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
 * ARE DISCLAIMED. IN NO EVENT SHALL APPLE INC. OR ITS CONTRIBUTORS BE LIABLE
 * FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL
 * DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR
 * SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER
 * CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT
 * LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY
 * OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH
 * DAMAGE.
 */

#ifndef FontFaceSet_h
#define FontFaceSet_h

#include "bindings/core/v8/Iterable.h"
#include "bindings/core/v8/ScriptPromise.h"
#include "core/css/FontFace.h"
#include "core/dom/ActiveDOMObject.h"
#include "core/dom/Document.h"
#include "core/events/EventListener.h"
#include "core/events/EventTarget.h"
#include "platform/AsyncMethodRunner.h"
#include "platform/heap/Handle.h"
#include "wtf/Allocator.h"
#include "wtf/Vector.h"

// Mac OS X 10.6 SDK defines check() macro that interfares with our check() method
#ifdef check
#undef check
#endif

namespace blink {

class CSSFontFace;
class CSSFontFaceSource;
class CSSFontSelector;
class Dictionary;
class ExceptionState;
class Font;
class FontFaceCache;
class FontResource;
class ExecutionContext;

using FontFaceSetIterable = PairIterable<Member<FontFace>, Member<FontFace>>;

class FontFaceSet final : public EventTargetWithInlineData, public Supplement<Document>, public ActiveDOMObject, public FontFaceSetIterable {
    USING_GARBAGE_COLLECTED_MIXIN(FontFaceSet);
    DEFINE_WRAPPERTYPEINFO();
    WTF_MAKE_NONCOPYABLE(FontFaceSet);
public:
    ~FontFaceSet() override;

    DEFINE_ATTRIBUTE_EVENT_LISTENER(loading);
    DEFINE_ATTRIBUTE_EVENT_LISTENER(loadingdone);
    DEFINE_ATTRIBUTE_EVENT_LISTENER(loadingerror);

    bool check(const String& font, const String& text, ExceptionState&);
    ScriptPromise load(ScriptState*, const String& font, const String& text);
    ScriptPromise ready(ScriptState*);

    FontFaceSet* addForBinding(ScriptState*, FontFace*, ExceptionState&);
    void clearForBinding(ScriptState*, ExceptionState&);
    bool deleteForBinding(ScriptState*, FontFace*, ExceptionState&);
    bool hasForBinding(ScriptState*, FontFace*, ExceptionState&) const;

    size_t size() const;
    AtomicString status() const;

    ExecutionContext* getExecutionContext() const override;
    const AtomicString& interfaceName() const override;

    Document* document() const;

    void didLayout();
    void beginFontLoading(FontFace*);
    void fontLoaded(FontFace*);
    void loadError(FontFace*);

    size_t approximateBlankCharacterCount() const;

    // ActiveDOMObject
    void suspend() override;
    void resume() override;
    void stop() override;

    static FontFaceSet* from(Document&);
    static void didLayout(Document&);
    static size_t approximateBlankCharacterCount(Document&);

    static const char* supplementName()
    {
        return "FontFaceSet";
    }

    void addFontFacesToFontFaceCache(FontFaceCache*, CSSFontSelector*);

    DECLARE_VIRTUAL_TRACE();

private:
    static FontFaceSet* create(Document& document)
    {
        return new FontFaceSet(document);
    }

    FontFaceSetIterable::IterationSource* startIteration(ScriptState*, ExceptionState&) override;

    class IterationSource final : public FontFaceSetIterable::IterationSource {
    public:
        explicit IterationSource(const HeapVector<Member<FontFace>>& fontFaces)
            : m_index(0)
            , m_fontFaces(fontFaces) { }
        bool next(ScriptState*, Member<FontFace>&, Member<FontFace>&, ExceptionState&) override;

        DEFINE_INLINE_VIRTUAL_TRACE()
        {
            visitor->trace(m_fontFaces);
            FontFaceSetIterable::IterationSource::trace(visitor);
        }

    private:
        size_t m_index;
        HeapVector<Member<FontFace>> m_fontFaces;
    };

    class FontLoadHistogram {
        DISALLOW_NEW();
    public:
        enum Status { NoWebFonts, HadBlankText, DidNotHaveBlankText, Reported };
        FontLoadHistogram() : m_status(NoWebFonts), m_count(0), m_recorded(false) { }
        void incrementCount() { m_count++; }
        void updateStatus(FontFace*);
        void record();

    private:
        Status m_status;
        int m_count;
        bool m_recorded;
    };

    FontFaceSet(Document&);

    bool inActiveDocumentContext() const;
    void addToLoadingFonts(FontFace*);
    void removeFromLoadingFonts(FontFace*);
    void fireLoadingEvent();
    void fireDoneEventIfPossible();
    bool resolveFontStyle(const String&, Font&);
    void handlePendingEventsAndPromisesSoon();
    void handlePendingEventsAndPromises();
    const HeapListHashSet<Member<FontFace>>& cssConnectedFontFaceList() const;
    bool isCSSConnectedFontFace(FontFace*) const;
    bool shouldSignalReady() const;

    using ReadyProperty = ScriptPromiseProperty<Member<FontFaceSet>, Member<FontFaceSet>, Member<DOMException>>;

    HeapHashSet<Member<FontFace>> m_loadingFonts;
    bool m_shouldFireLoadingEvent;
    bool m_isLoading;
    Member<ReadyProperty> m_ready;
    FontFaceArray m_loadedFonts;
    FontFaceArray m_failedFonts;
    HeapListHashSet<Member<FontFace>> m_nonCSSConnectedFaces;

    Member<AsyncMethodRunner<FontFaceSet>> m_asyncRunner;

    FontLoadHistogram m_histogram;
};

} // namespace blink

#endif // FontFaceSet_h
