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

#include "core/css/FontFaceSet.h"

#include "bindings/core/v8/Dictionary.h"
#include "bindings/core/v8/ScriptPromiseResolver.h"
#include "bindings/core/v8/ScriptState.h"
#include "core/css/CSSFontSelector.h"
#include "core/css/CSSSegmentedFontFace.h"
#include "core/css/FontFaceCache.h"
#include "core/css/FontFaceSetLoadEvent.h"
#include "core/css/StylePropertySet.h"
#include "core/css/parser/CSSParser.h"
#include "core/css/resolver/StyleResolver.h"
#include "core/dom/Document.h"
#include "core/dom/StyleEngine.h"
#include "core/frame/FrameView.h"
#include "core/frame/LocalFrame.h"
#include "core/style/StyleInheritedData.h"
#include "platform/Histogram.h"

namespace blink {

static const int defaultFontSize = 10;
static const char defaultFontFamily[] = "sans-serif";

class LoadFontPromiseResolver final : public FontFace::LoadFontCallback {
public:
    static LoadFontPromiseResolver* create(FontFaceArray faces, ScriptState* scriptState)
    {
        return new LoadFontPromiseResolver(faces, scriptState);
    }

    void loadFonts(ExecutionContext*);
    ScriptPromise promise() { return m_resolver->promise(); }

    void notifyLoaded(FontFace*) override;
    void notifyError(FontFace*) override;

    DECLARE_VIRTUAL_TRACE();

private:
    LoadFontPromiseResolver(FontFaceArray faces, ScriptState* scriptState)
        : m_numLoading(faces.size())
        , m_errorOccured(false)
        , m_resolver(ScriptPromiseResolver::create(scriptState))
    {
        m_fontFaces.swap(faces);
    }

    HeapVector<Member<FontFace>> m_fontFaces;
    int m_numLoading;
    bool m_errorOccured;
    Member<ScriptPromiseResolver> m_resolver;
};

void LoadFontPromiseResolver::loadFonts(ExecutionContext* context)
{
    if (!m_numLoading) {
        m_resolver->resolve(m_fontFaces);
        return;
    }

    for (size_t i = 0; i < m_fontFaces.size(); i++)
        m_fontFaces[i]->loadWithCallback(this, context);
}

void LoadFontPromiseResolver::notifyLoaded(FontFace* fontFace)
{
    m_numLoading--;
    if (m_numLoading || m_errorOccured)
        return;

    m_resolver->resolve(m_fontFaces);
}

void LoadFontPromiseResolver::notifyError(FontFace* fontFace)
{
    m_numLoading--;
    if (!m_errorOccured) {
        m_errorOccured = true;
        m_resolver->reject(fontFace->error());
    }
}

DEFINE_TRACE(LoadFontPromiseResolver)
{
    visitor->trace(m_fontFaces);
    visitor->trace(m_resolver);
    LoadFontCallback::trace(visitor);
}

FontFaceSet::FontFaceSet(Document& document)
    : ActiveDOMObject(&document)
    , m_shouldFireLoadingEvent(false)
    , m_isLoading(false)
    , m_ready(new ReadyProperty(getExecutionContext(), this, ReadyProperty::Ready))
    , m_asyncRunner(AsyncMethodRunner<FontFaceSet>::create(this, &FontFaceSet::handlePendingEventsAndPromises))
{
    suspendIfNeeded();
}

FontFaceSet::~FontFaceSet()
{
}

Document* FontFaceSet::document() const
{
    return toDocument(getExecutionContext());
}

bool FontFaceSet::inActiveDocumentContext() const
{
    ExecutionContext* context = getExecutionContext();
    return context && toDocument(context)->isActive();
}

void FontFaceSet::addFontFacesToFontFaceCache(FontFaceCache* fontFaceCache, CSSFontSelector* fontSelector)
{
    for (const auto& fontFace : m_nonCSSConnectedFaces)
        fontFaceCache->addFontFace(fontSelector, fontFace, false);
}

const AtomicString& FontFaceSet::interfaceName() const
{
    return EventTargetNames::FontFaceSet;
}

ExecutionContext* FontFaceSet::getExecutionContext() const
{
    return ActiveDOMObject::getExecutionContext();
}

AtomicString FontFaceSet::status() const
{
    DEFINE_STATIC_LOCAL(AtomicString, loading, ("loading"));
    DEFINE_STATIC_LOCAL(AtomicString, loaded, ("loaded"));
    return m_isLoading ? loading : loaded;
}

void FontFaceSet::handlePendingEventsAndPromisesSoon()
{
    // m_asyncRunner will be automatically stopped on destruction.
    m_asyncRunner->runAsync();
}

void FontFaceSet::didLayout()
{
    if (document()->frame()->isMainFrame() && m_loadingFonts.isEmpty())
        m_histogram.record();
    if (!shouldSignalReady())
        return;
    handlePendingEventsAndPromisesSoon();
}

bool FontFaceSet::shouldSignalReady() const
{
    if (!m_loadingFonts.isEmpty())
        return false;
    return m_isLoading || m_ready->getState() == ReadyProperty::Pending;
}

void FontFaceSet::handlePendingEventsAndPromises()
{
    fireLoadingEvent();
    fireDoneEventIfPossible();
}

void FontFaceSet::fireLoadingEvent()
{
    if (m_shouldFireLoadingEvent) {
        m_shouldFireLoadingEvent = false;
        dispatchEvent(FontFaceSetLoadEvent::createForFontFaces(EventTypeNames::loading));
    }
}

void FontFaceSet::suspend()
{
    m_asyncRunner->suspend();
}

void FontFaceSet::resume()
{
    m_asyncRunner->resume();
}

void FontFaceSet::stop()
{
    m_asyncRunner->stop();
}

void FontFaceSet::beginFontLoading(FontFace* fontFace)
{
    m_histogram.incrementCount();
    addToLoadingFonts(fontFace);
}

void FontFaceSet::fontLoaded(FontFace* fontFace)
{
    m_histogram.updateStatus(fontFace);
    m_loadedFonts.append(fontFace);
    removeFromLoadingFonts(fontFace);
}

void FontFaceSet::loadError(FontFace* fontFace)
{
    m_histogram.updateStatus(fontFace);
    m_failedFonts.append(fontFace);
    removeFromLoadingFonts(fontFace);
}

size_t FontFaceSet::approximateBlankCharacterCount() const
{
    size_t count = 0;
    for (auto& fontFace : m_loadingFonts)
        count += fontFace->approximateBlankCharacterCount();
    return count;
}

void FontFaceSet::addToLoadingFonts(FontFace* fontFace)
{
    if (!m_isLoading) {
        m_isLoading = true;
        m_shouldFireLoadingEvent = true;
        if (m_ready->getState() != ReadyProperty::Pending)
            m_ready->reset();
        handlePendingEventsAndPromisesSoon();
    }
    m_loadingFonts.add(fontFace);
}

void FontFaceSet::removeFromLoadingFonts(FontFace* fontFace)
{
    m_loadingFonts.remove(fontFace);
    if (m_loadingFonts.isEmpty())
        handlePendingEventsAndPromisesSoon();
}

ScriptPromise FontFaceSet::ready(ScriptState* scriptState)
{
    return m_ready->promise(scriptState->world());
}

FontFaceSet* FontFaceSet::addForBinding(ScriptState*, FontFace* fontFace, ExceptionState&)
{
    ASSERT(fontFace);
    if (!inActiveDocumentContext())
        return this;
    if (m_nonCSSConnectedFaces.contains(fontFace))
        return this;
    if (isCSSConnectedFontFace(fontFace))
        return this;
    CSSFontSelector* fontSelector = document()->styleEngine().fontSelector();
    m_nonCSSConnectedFaces.add(fontFace);
    fontSelector->fontFaceCache()->addFontFace(fontSelector, fontFace, false);
    if (fontFace->loadStatus() == FontFace::Loading)
        addToLoadingFonts(fontFace);
    fontSelector->fontFaceInvalidated();
    return this;
}

void FontFaceSet::clearForBinding(ScriptState*, ExceptionState&)
{
    if (!inActiveDocumentContext() || m_nonCSSConnectedFaces.isEmpty())
        return;
    CSSFontSelector* fontSelector = document()->styleEngine().fontSelector();
    FontFaceCache* fontFaceCache = fontSelector->fontFaceCache();
    for (const auto& fontFace : m_nonCSSConnectedFaces) {
        fontFaceCache->removeFontFace(fontFace.get(), false);
        if (fontFace->loadStatus() == FontFace::Loading)
            removeFromLoadingFonts(fontFace);
    }
    m_nonCSSConnectedFaces.clear();
    fontSelector->fontFaceInvalidated();
}

bool FontFaceSet::deleteForBinding(ScriptState*, FontFace* fontFace, ExceptionState&)
{
    ASSERT(fontFace);
    if (!inActiveDocumentContext())
        return false;
    HeapListHashSet<Member<FontFace>>::iterator it = m_nonCSSConnectedFaces.find(fontFace);
    if (it != m_nonCSSConnectedFaces.end()) {
        m_nonCSSConnectedFaces.remove(it);
        CSSFontSelector* fontSelector = document()->styleEngine().fontSelector();
        fontSelector->fontFaceCache()->removeFontFace(fontFace, false);
        if (fontFace->loadStatus() == FontFace::Loading)
            removeFromLoadingFonts(fontFace);
        fontSelector->fontFaceInvalidated();
        return true;
    }
    return false;
}

bool FontFaceSet::hasForBinding(ScriptState*, FontFace* fontFace, ExceptionState&) const
{
    ASSERT(fontFace);
    if (!inActiveDocumentContext())
        return false;
    return m_nonCSSConnectedFaces.contains(fontFace) || isCSSConnectedFontFace(fontFace);
}

const HeapListHashSet<Member<FontFace>>& FontFaceSet::cssConnectedFontFaceList() const
{
    Document* d = document();
    d->ensureStyleResolver(); // Flush pending style changes.
    return d->styleEngine().fontSelector()->fontFaceCache()->cssConnectedFontFaces();
}

bool FontFaceSet::isCSSConnectedFontFace(FontFace* fontFace) const
{
    return cssConnectedFontFaceList().contains(fontFace);
}

size_t FontFaceSet::size() const
{
    if (!inActiveDocumentContext())
        return m_nonCSSConnectedFaces.size();
    return cssConnectedFontFaceList().size() + m_nonCSSConnectedFaces.size();
}

void FontFaceSet::fireDoneEventIfPossible()
{
    if (m_shouldFireLoadingEvent)
        return;
    if (!shouldSignalReady())
        return;

    // If the layout was invalidated in between when we thought layout
    // was updated and when we're ready to fire the event, just wait
    // until after the next layout before firing events.
    Document* d = document();
    if (!d->view() || d->view()->needsLayout())
        return;

    if (m_isLoading) {
        FontFaceSetLoadEvent* doneEvent = nullptr;
        FontFaceSetLoadEvent* errorEvent = nullptr;
        doneEvent = FontFaceSetLoadEvent::createForFontFaces(EventTypeNames::loadingdone, m_loadedFonts);
        m_loadedFonts.clear();
        if (!m_failedFonts.isEmpty()) {
            errorEvent = FontFaceSetLoadEvent::createForFontFaces(EventTypeNames::loadingerror, m_failedFonts);
            m_failedFonts.clear();
        }
        m_isLoading = false;
        dispatchEvent(doneEvent);
        if (errorEvent)
            dispatchEvent(errorEvent);
    }

    if (m_ready->getState() == ReadyProperty::Pending)
        m_ready->resolve(this);
}

ScriptPromise FontFaceSet::load(ScriptState* scriptState, const String& fontString, const String& text)
{
    if (!inActiveDocumentContext())
        return ScriptPromise();

    Font font;
    if (!resolveFontStyle(fontString, font)) {
        ScriptPromiseResolver* resolver = ScriptPromiseResolver::create(scriptState);
        ScriptPromise promise = resolver->promise();
        resolver->reject(DOMException::create(SyntaxError, "Could not resolve '" + fontString + "' as a font."));
        return promise;
    }

    FontFaceCache* fontFaceCache = document()->styleEngine().fontSelector()->fontFaceCache();
    FontFaceArray faces;
    for (const FontFamily* f = &font.getFontDescription().family(); f; f = f->next()) {
        CSSSegmentedFontFace* segmentedFontFace = fontFaceCache->get(font.getFontDescription(), f->family());
        if (segmentedFontFace)
            segmentedFontFace->match(text, faces);
    }

    LoadFontPromiseResolver* resolver = LoadFontPromiseResolver::create(faces, scriptState);
    ScriptPromise promise = resolver->promise();
    resolver->loadFonts(getExecutionContext()); // After this, resolver->promise() may return null.
    return promise;
}

bool FontFaceSet::check(const String& fontString, const String& text, ExceptionState& exceptionState)
{
    if (!inActiveDocumentContext())
        return false;

    Font font;
    if (!resolveFontStyle(fontString, font)) {
        exceptionState.throwDOMException(SyntaxError, "Could not resolve '" + fontString + "' as a font.");
        return false;
    }

    CSSFontSelector* fontSelector = document()->styleEngine().fontSelector();
    FontFaceCache* fontFaceCache = fontSelector->fontFaceCache();

    bool hasLoadedFaces = false;
    for (const FontFamily* f = &font.getFontDescription().family(); f; f = f->next()) {
        CSSSegmentedFontFace* face = fontFaceCache->get(font.getFontDescription(), f->family());
        if (face) {
            if (!face->checkFont(text))
                return false;
            hasLoadedFaces = true;
        }
    }
    if (hasLoadedFaces)
        return true;
    for (const FontFamily* f = &font.getFontDescription().family(); f; f = f->next()) {
        if (fontSelector->isPlatformFontAvailable(font.getFontDescription(), f->family()))
            return true;
    }
    return false;
}

bool FontFaceSet::resolveFontStyle(const String& fontString, Font& font)
{
    if (fontString.isEmpty())
        return false;

    // Interpret fontString in the same way as the 'font' attribute of CanvasRenderingContext2D.
    MutableStylePropertySet* parsedStyle = MutableStylePropertySet::create(HTMLStandardMode);
    CSSParser::parseValue(parsedStyle, CSSPropertyFont, fontString, true, 0);
    if (parsedStyle->isEmpty())
        return false;

    String fontValue = parsedStyle->getPropertyValue(CSSPropertyFont);
    if (fontValue == "inherit" || fontValue == "initial")
        return false;

    RefPtr<ComputedStyle> style = ComputedStyle::create();

    FontFamily fontFamily;
    fontFamily.setFamily(defaultFontFamily);

    FontDescription defaultFontDescription;
    defaultFontDescription.setFamily(fontFamily);
    defaultFontDescription.setSpecifiedSize(defaultFontSize);
    defaultFontDescription.setComputedSize(defaultFontSize);

    style->setFontDescription(defaultFontDescription);

    style->font().update(style->font().getFontSelector());

    document()->ensureStyleResolver().computeFont(style.get(), *parsedStyle);

    font = style->font();
    font.update(document()->styleEngine().fontSelector());
    return true;
}

void FontFaceSet::FontLoadHistogram::updateStatus(FontFace* fontFace)
{
    if (m_status == Reported)
        return;
    if (fontFace->hadBlankText())
        m_status = HadBlankText;
    else if (m_status == NoWebFonts)
        m_status = DidNotHaveBlankText;
}

void FontFaceSet::FontLoadHistogram::record()
{
    if (!m_recorded) {
        m_recorded = true;
        DEFINE_STATIC_LOCAL(CustomCountHistogram, webFontsInPageHistogram, ("WebFont.WebFontsInPage", 1, 100, 50));
        webFontsInPageHistogram.count(m_count);
    }
    if (m_status == HadBlankText || m_status == DidNotHaveBlankText) {
        DEFINE_STATIC_LOCAL(EnumerationHistogram, hadBlankTextHistogram, ("WebFont.HadBlankText", 2));
        hadBlankTextHistogram.count(m_status == HadBlankText ? 1 : 0);
        m_status = Reported;
    }
}

FontFaceSet* FontFaceSet::from(Document& document)
{
    FontFaceSet* fonts = static_cast<FontFaceSet*>(Supplement<Document>::from(document, supplementName()));
    if (!fonts) {
        fonts = FontFaceSet::create(document);
        Supplement<Document>::provideTo(document, supplementName(), fonts);
    }

    return fonts;
}

void FontFaceSet::didLayout(Document& document)
{
    if (FontFaceSet* fonts = static_cast<FontFaceSet*>(Supplement<Document>::from(document, supplementName())))
        fonts->didLayout();
}

size_t FontFaceSet::approximateBlankCharacterCount(Document& document)
{
    if (FontFaceSet* fonts = static_cast<FontFaceSet*>(Supplement<Document>::from(document, supplementName())))
        return fonts->approximateBlankCharacterCount();
    return 0;
}

FontFaceSetIterable::IterationSource* FontFaceSet::startIteration(ScriptState*, ExceptionState&)
{
    // Setlike should iterate each item in insertion order, and items should
    // be keep on up to date. But since blink does not have a way to hook up CSS
    // modification, take a snapshot here, and make it ordered as follows.
    HeapVector<Member<FontFace>> fontFaces;
    if (inActiveDocumentContext()) {
        const HeapListHashSet<Member<FontFace>>& cssConnectedFaces = cssConnectedFontFaceList();
        fontFaces.reserveInitialCapacity(cssConnectedFaces.size() + m_nonCSSConnectedFaces.size());
        for (const auto& fontFace : cssConnectedFaces)
            fontFaces.append(fontFace);
        for (const auto& fontFace : m_nonCSSConnectedFaces)
            fontFaces.append(fontFace);
    }
    return new IterationSource(fontFaces);
}

bool FontFaceSet::IterationSource::next(ScriptState*, Member<FontFace>& key, Member<FontFace>& value, ExceptionState&)
{
    if (m_fontFaces.size() <= m_index)
        return false;
    key = value = m_fontFaces[m_index++];
    return true;
}

DEFINE_TRACE(FontFaceSet)
{
    visitor->trace(m_ready);
    visitor->trace(m_loadingFonts);
    visitor->trace(m_loadedFonts);
    visitor->trace(m_failedFonts);
    visitor->trace(m_nonCSSConnectedFaces);
    visitor->trace(m_asyncRunner);
    EventTargetWithInlineData::trace(visitor);
    Supplement<Document>::trace(visitor);
    ActiveDOMObject::trace(visitor);
}

} // namespace blink
