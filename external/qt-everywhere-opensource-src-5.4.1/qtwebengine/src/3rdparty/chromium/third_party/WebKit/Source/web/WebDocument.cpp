/*
 * Copyright (C) 2009 Google Inc. All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions are
 * met:
 *
 *     * Redistributions of source code must retain the above copyright
 * notice, this list of conditions and the following disclaimer.
 *     * Redistributions in binary form must reproduce the above
 * copyright notice, this list of conditions and the following disclaimer
 * in the documentation and/or other materials provided with the
 * distribution.
 *     * Neither the name of Google Inc. nor the names of its
 * contributors may be used to endorse or promote products derived from
 * this software without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS
 * "AS IS" AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT
 * LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR
 * A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT
 * OWNER OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL,
 * SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT
 * LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE,
 * DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY
 * THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
 * OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

#include "config.h"
#include "public/web/WebDocument.h"

#include "bindings/v8/Dictionary.h"
#include "bindings/v8/ExceptionState.h"
#include "bindings/v8/ScriptState.h"
#include "bindings/v8/ScriptValue.h"
#include "core/accessibility/AXObjectCache.h"
#include "core/css/CSSParserMode.h"
#include "core/css/StyleSheetContents.h"
#include "core/dom/CSSSelectorWatch.h"
#include "core/dom/Document.h"
#include "core/dom/DocumentType.h"
#include "core/dom/Element.h"
#include "core/dom/FullscreenElementStack.h"
#include "core/dom/StyleEngine.h"
#include "core/html/HTMLAllCollection.h"
#include "core/html/HTMLBodyElement.h"
#include "core/html/HTMLCollection.h"
#include "core/html/HTMLElement.h"
#include "core/html/HTMLFormElement.h"
#include "core/html/HTMLHeadElement.h"
#include "core/loader/DocumentLoader.h"
#include "core/rendering/RenderObject.h"
#include "core/rendering/RenderView.h"
#include "platform/weborigin/SecurityOrigin.h"
#include "public/platform/WebURL.h"
#include "public/web/WebAXObject.h"
#include "public/web/WebDOMEvent.h"
#include "public/web/WebDocumentType.h"
#include "public/web/WebElement.h"
#include "public/web/WebElementCollection.h"
#include "public/web/WebFormElement.h"
#include "public/web/WebNodeList.h"
#include "web/WebLocalFrameImpl.h"
#include "wtf/PassRefPtr.h"
#include <v8.h>

using namespace WebCore;

namespace blink {

WebURL WebDocument::url() const
{
    return constUnwrap<Document>()->url();
}

WebSecurityOrigin WebDocument::securityOrigin() const
{
    if (!constUnwrap<Document>())
        return WebSecurityOrigin();
    return WebSecurityOrigin(constUnwrap<Document>()->securityOrigin());
}

WebString WebDocument::encoding() const
{
    return constUnwrap<Document>()->encodingName();
}

WebString WebDocument::contentLanguage() const
{
    return constUnwrap<Document>()->contentLanguage();
}

WebString WebDocument::referrer() const
{
    return constUnwrap<Document>()->referrer();
}

WebColor WebDocument::themeColor() const
{
    return constUnwrap<Document>()->themeColor().rgb();
}

WebURL WebDocument::openSearchDescriptionURL() const
{
    return const_cast<Document*>(constUnwrap<Document>())->openSearchDescriptionURL();
}

WebLocalFrame* WebDocument::frame() const
{
    return WebLocalFrameImpl::fromFrame(constUnwrap<Document>()->frame());
}

bool WebDocument::isHTMLDocument() const
{
    return constUnwrap<Document>()->isHTMLDocument();
}

bool WebDocument::isXHTMLDocument() const
{
    return constUnwrap<Document>()->isXHTMLDocument();
}

bool WebDocument::isPluginDocument() const
{
    return constUnwrap<Document>()->isPluginDocument();
}

WebURL WebDocument::baseURL() const
{
    return constUnwrap<Document>()->baseURL();
}

WebURL WebDocument::firstPartyForCookies() const
{
    return constUnwrap<Document>()->firstPartyForCookies();
}

WebElement WebDocument::documentElement() const
{
    return WebElement(constUnwrap<Document>()->documentElement());
}

WebElement WebDocument::body() const
{
    return WebElement(constUnwrap<Document>()->body());
}

WebElement WebDocument::head()
{
    return WebElement(unwrap<Document>()->head());
}

WebString WebDocument::title() const
{
    return WebString(constUnwrap<Document>()->title());
}

WebElementCollection WebDocument::all()
{
    return WebElementCollection(unwrap<Document>()->all());
}

void WebDocument::images(WebVector<WebElement>& results)
{
    RefPtrWillBeRawPtr<HTMLCollection> images = unwrap<Document>()->images();
    size_t sourceLength = images->length();
    Vector<WebElement> temp;
    temp.reserveCapacity(sourceLength);
    for (size_t i = 0; i < sourceLength; ++i) {
        Element* element = images->item(i);
        if (element && element->isHTMLElement())
            temp.append(WebElement(element));
    }
    results.assign(temp);
}

void WebDocument::forms(WebVector<WebFormElement>& results) const
{
    RefPtrWillBeRawPtr<HTMLCollection> forms = const_cast<Document*>(constUnwrap<Document>())->forms();
    size_t sourceLength = forms->length();
    Vector<WebFormElement> temp;
    temp.reserveCapacity(sourceLength);
    for (size_t i = 0; i < sourceLength; ++i) {
        Element* element = forms->item(i);
        // Strange but true, sometimes node can be 0.
        if (element && element->isHTMLElement())
            temp.append(WebFormElement(toHTMLFormElement(element)));
    }
    results.assign(temp);
}

WebURL WebDocument::completeURL(const WebString& partialURL) const
{
    return constUnwrap<Document>()->completeURL(partialURL);
}

WebElement WebDocument::getElementById(const WebString& id) const
{
    return WebElement(constUnwrap<Document>()->getElementById(id));
}

WebElement WebDocument::focusedElement() const
{
    return WebElement(constUnwrap<Document>()->focusedElement());
}

WebDocumentType WebDocument::doctype() const
{
    return WebDocumentType(constUnwrap<Document>()->doctype());
}

void WebDocument::insertStyleSheet(const WebString& sourceCode)
{
    RefPtrWillBeRawPtr<Document> document = unwrap<Document>();
    ASSERT(document);
    RefPtrWillBeRawPtr<StyleSheetContents> parsedSheet = StyleSheetContents::create(CSSParserContext(*document, 0));
    parsedSheet->parseString(sourceCode);
    document->styleEngine()->addAuthorSheet(parsedSheet);
}

void WebDocument::watchCSSSelectors(const WebVector<WebString>& webSelectors)
{
    RefPtrWillBeRawPtr<Document> document = unwrap<Document>();
    Vector<String> selectors;
    selectors.append(webSelectors.data(), webSelectors.size());
    CSSSelectorWatch::from(*document).watchCSSSelectors(selectors);
}

void WebDocument::cancelFullScreen()
{
    if (FullscreenElementStack* fullscreen = FullscreenElementStack::fromIfExists(*unwrap<Document>()))
        fullscreen->webkitCancelFullScreen();
}

WebElement WebDocument::fullScreenElement() const
{
    Element* fullScreenElement = 0;
    if (FullscreenElementStack* fullscreen = FullscreenElementStack::fromIfExists(*const_cast<WebDocument*>(this)->unwrap<Document>()))
        fullScreenElement = fullscreen->webkitCurrentFullScreenElement();
    return WebElement(fullScreenElement);
}

WebDOMEvent WebDocument::createEvent(const WebString& eventType)
{
    TrackExceptionState exceptionState;
    WebDOMEvent event(unwrap<Document>()->createEvent(eventType, exceptionState));
    if (exceptionState.hadException())
        return WebDOMEvent();
    return event;
}

WebReferrerPolicy WebDocument::referrerPolicy() const
{
    return static_cast<WebReferrerPolicy>(constUnwrap<Document>()->referrerPolicy());
}

WebElement WebDocument::createElement(const WebString& tagName)
{
    TrackExceptionState exceptionState;
    WebElement element(unwrap<Document>()->createElement(tagName, exceptionState));
    if (exceptionState.hadException())
        return WebElement();
    return element;
}

WebAXObject WebDocument::accessibilityObject() const
{
    const Document* document = constUnwrap<Document>();
    return WebAXObject(document->axObjectCache()->getOrCreate(document->renderView()));
}

WebAXObject WebDocument::accessibilityObjectFromID(int axID) const
{
    const Document* document = constUnwrap<Document>();
    return WebAXObject(document->axObjectCache()->objectFromAXID(axID));
}

WebVector<WebDraggableRegion> WebDocument::draggableRegions() const
{
    WebVector<WebDraggableRegion> draggableRegions;
    const Document* document = constUnwrap<Document>();
    if (document->hasAnnotatedRegions()) {
        const Vector<AnnotatedRegionValue>& regions = document->annotatedRegions();
        draggableRegions = WebVector<WebDraggableRegion>(regions.size());
        for (size_t i = 0; i < regions.size(); i++) {
            const AnnotatedRegionValue& value = regions[i];
            draggableRegions[i].draggable = value.draggable;
            draggableRegions[i].bounds = WebCore::IntRect(value.bounds);
        }
    }
    return draggableRegions;
}

v8::Handle<v8::Value> WebDocument::registerEmbedderCustomElement(const WebString& name, v8::Handle<v8::Value> options, WebExceptionCode& ec)
{
    v8::Isolate* isolate = v8::Isolate::GetCurrent();
    Document* document = unwrap<Document>();
    Dictionary dictionary(options, isolate);
    TrackExceptionState exceptionState;
    ScriptValue constructor = document->registerElement(ScriptState::current(isolate), name, dictionary, exceptionState, CustomElement::EmbedderNames);
    ec = exceptionState.code();
    if (exceptionState.hadException())
        return v8::Handle<v8::Value>();
    return constructor.v8Value();
}

WebDocument::WebDocument(const PassRefPtrWillBeRawPtr<Document>& elem)
    : WebNode(elem)
{
}

WebDocument& WebDocument::operator=(const PassRefPtrWillBeRawPtr<Document>& elem)
{
    m_private = elem;
    return *this;
}

WebDocument::operator PassRefPtrWillBeRawPtr<Document>() const
{
    return toDocument(m_private.get());
}

} // namespace blink
