// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "core/xml/DocumentXSLT.h"

#include "bindings/core/v8/DOMWrapperWorld.h"
#include "bindings/core/v8/ScriptState.h"
#include "bindings/core/v8/V8AbstractEventListener.h"
#include "bindings/core/v8/V8Binding.h"
#include "core/dom/Document.h"
#include "core/dom/Node.h"
#include "core/dom/ProcessingInstruction.h"
#include "core/events/Event.h"
#include "core/events/EventListener.h"
#include "core/frame/UseCounter.h"
#include "core/inspector/InspectorInstrumentation.h"
#include "core/xml/XSLStyleSheet.h"
#include "core/xml/XSLTProcessor.h"

namespace blink {

class DOMContentLoadedListener final : public V8AbstractEventListener, public ProcessingInstruction::DetachableEventListener {
    USING_GARBAGE_COLLECTED_MIXIN(DOMContentLoadedListener);
public:
    static DOMContentLoadedListener* create(ScriptState* scriptState, ProcessingInstruction* pi)
    {
        return new DOMContentLoadedListener(scriptState, pi);
    }

    bool operator==(const EventListener&) const override
    {
        return true;
    }

    virtual void handleEvent(ScriptState* scriptState, Event* event)
    {
        ASSERT(RuntimeEnabledFeatures::xsltEnabled());
        ASSERT(event->type() == "DOMContentLoaded");
        ScriptState::Scope scope(scriptState);

        Document& document = *toDocument(scriptState->getExecutionContext());
        ASSERT(!document.parsing());

        // Processing instruction (XML documents only).
        // We don't support linking to embedded CSS stylesheets,
        // see <https://bugs.webkit.org/show_bug.cgi?id=49281> for discussion.
        // Don't apply XSL transforms to already transformed documents.
        if (DocumentXSLT::hasTransformSourceDocument(document))
            return;

        ProcessingInstruction* pi = DocumentXSLT::findXSLStyleSheet(document);
        if (!pi || pi != m_processingInstruction || pi->isLoading())
            return;
        DocumentXSLT::applyXSLTransform(document, pi);
    }

    void detach() override
    {
        m_processingInstruction = nullptr;
    }

    EventListener* toEventListener() override
    {
        return this;
    }

    DEFINE_INLINE_VIRTUAL_TRACE()
    {
        visitor->trace(m_processingInstruction);
        V8AbstractEventListener::trace(visitor);
        ProcessingInstruction::DetachableEventListener::trace(visitor);
    }

private:
    DOMContentLoadedListener(ScriptState* scriptState, ProcessingInstruction* pi)
        : V8AbstractEventListener(false, scriptState->world(), scriptState->isolate())
        , m_processingInstruction(pi)
    {
    }

    virtual v8::Local<v8::Value> callListenerFunction(ScriptState*, v8::Local<v8::Value>, Event*)
    {
        ASSERT_NOT_REACHED();
        return v8::Local<v8::Value>();
    }

    // If this event listener is attached to a ProcessingInstruction, keep a
    // weak reference back to it. That ProcessingInstruction is responsible for
    // detaching itself and clear out the reference.
    Member<ProcessingInstruction> m_processingInstruction;
};

DocumentXSLT::DocumentXSLT()
    : m_transformSourceDocument(nullptr)
{
}

void DocumentXSLT::applyXSLTransform(Document& document, ProcessingInstruction* pi)
{
    ASSERT(!pi->isLoading());
    UseCounter::count(document, UseCounter::XSLProcessingInstruction);
    XSLTProcessor* processor = XSLTProcessor::create(document);
    processor->setXSLStyleSheet(toXSLStyleSheet(pi->sheet()));
    String resultMIMEType;
    String newSource;
    String resultEncoding;
    document.setParsingState(Document::Parsing);
    if (!processor->transformToString(&document, resultMIMEType, newSource, resultEncoding)) {
        document.setParsingState(Document::FinishedParsing);
        return;
    }
    // FIXME: If the transform failed we should probably report an error (like Mozilla does).
    LocalFrame* ownerFrame = document.frame();
    processor->createDocumentFromSource(newSource, resultEncoding, resultMIMEType, &document, ownerFrame);
    InspectorInstrumentation::frameDocumentUpdated(ownerFrame);
    document.setParsingState(Document::FinishedParsing);
}

ProcessingInstruction* DocumentXSLT::findXSLStyleSheet(Document& document)
{
    for (Node* node = document.firstChild(); node; node = node->nextSibling()) {
        if (node->getNodeType() != Node::PROCESSING_INSTRUCTION_NODE)
            continue;

        ProcessingInstruction* pi = toProcessingInstruction(node);
        if (pi->isXSL())
            return pi;
    }
    return nullptr;
}

bool DocumentXSLT::processingInstructionInsertedIntoDocument(Document& document, ProcessingInstruction* pi)
{
    if (!pi->isXSL())
        return false;

    if (!RuntimeEnabledFeatures::xsltEnabled() || !document.frame())
        return true;

    ScriptState* scriptState = ScriptState::forMainWorld(document.frame());
    if (!scriptState)
        return false;
    DOMContentLoadedListener* listener = DOMContentLoadedListener::create(scriptState, pi);
    document.addEventListener(EventTypeNames::DOMContentLoaded, listener, false);
    ASSERT(!pi->eventListenerForXSLT());
    pi->setEventListenerForXSLT(listener);
    return true;
}

bool DocumentXSLT::processingInstructionRemovedFromDocument(Document& document, ProcessingInstruction* pi)
{
    if (!pi->isXSL())
        return false;

    if (!pi->eventListenerForXSLT())
        return true;

    ASSERT(RuntimeEnabledFeatures::xsltEnabled());
    document.removeEventListener(EventTypeNames::DOMContentLoaded, pi->eventListenerForXSLT(), false);
    pi->clearEventListenerForXSLT();
    return true;
}

bool DocumentXSLT::sheetLoaded(Document& document, ProcessingInstruction* pi)
{
    if (!pi->isXSL())
        return false;

    if (RuntimeEnabledFeatures::xsltEnabled() && !document.parsing() && !pi->isLoading()
        && !DocumentXSLT::hasTransformSourceDocument(document)) {
        if (findXSLStyleSheet(document) == pi)
            applyXSLTransform(document, pi);
    }
    return true;
}

const char* DocumentXSLT::supplementName()
{
    return "DocumentXSLT";
}

bool DocumentXSLT::hasTransformSourceDocument(Document& document)
{
    return static_cast<DocumentXSLT*>(Supplement<Document>::from(document, supplementName()));
}


DocumentXSLT& DocumentXSLT::from(Supplementable<Document>& document)
{
    DocumentXSLT* supplement = static_cast<DocumentXSLT*>(Supplement<Document>::from(document, supplementName()));
    if (!supplement) {
        supplement = new DocumentXSLT;
        Supplement<Document>::provideTo(document, supplementName(), supplement);
    }
    return *supplement;
}

DEFINE_TRACE(DocumentXSLT)
{
    visitor->trace(m_transformSourceDocument);
    Supplement<Document>::trace(visitor);
}

} // namespace blink
