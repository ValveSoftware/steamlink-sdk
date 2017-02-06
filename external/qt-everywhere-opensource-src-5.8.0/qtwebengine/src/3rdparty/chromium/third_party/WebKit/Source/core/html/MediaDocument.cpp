/*
 * Copyright (C) 2008 Apple Inc. All Rights Reserved.
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

#include "core/html/MediaDocument.h"

#include "bindings/core/v8/ExceptionStatePlaceholder.h"
#include "core/HTMLNames.h"
#include "core/dom/ElementTraversal.h"
#include "core/dom/RawDataDocumentParser.h"
#include "core/dom/shadow/ShadowRoot.h"
#include "core/events/Event.h"
#include "core/events/EventListener.h"
#include "core/events/KeyboardEvent.h"
#include "core/frame/LocalFrame.h"
#include "core/frame/UseCounter.h"
#include "core/html/HTMLAnchorElement.h"
#include "core/html/HTMLBodyElement.h"
#include "core/html/HTMLContentElement.h"
#include "core/html/HTMLDivElement.h"
#include "core/html/HTMLHeadElement.h"
#include "core/html/HTMLHtmlElement.h"
#include "core/html/HTMLMetaElement.h"
#include "core/html/HTMLSourceElement.h"
#include "core/html/HTMLStyleElement.h"
#include "core/html/HTMLVideoElement.h"
#include "core/loader/DocumentLoader.h"
#include "core/loader/FrameLoader.h"
#include "core/loader/FrameLoaderClient.h"
#include "platform/Histogram.h"
#include "platform/KeyboardCodes.h"
#include "platform/text/PlatformLocale.h"

namespace blink {

using namespace HTMLNames;

// Enums used for UMA histogram.
enum MediaDocumentDownloadButtonValue {
    MediaDocumentDownloadButtonShown,
    MediaDocumentDownloadButtonClicked,
    // Only append new enums here.
    MediaDocumentDownloadButtonMax
};

void recordDownloadMetric(MediaDocumentDownloadButtonValue value)
{
    DEFINE_STATIC_LOCAL(EnumerationHistogram, mediaDocumentDownloadButtonHistogram, ("Blink.MediaDocument.DownloadButton", MediaDocumentDownloadButtonMax));
    mediaDocumentDownloadButtonHistogram.count(value);
}

// FIXME: Share more code with PluginDocumentParser.
class MediaDocumentParser : public RawDataDocumentParser {
public:
    static MediaDocumentParser* create(MediaDocument* document)
    {
        return new MediaDocumentParser(document);
    }

private:
    explicit MediaDocumentParser(Document* document)
        : RawDataDocumentParser(document)
        , m_didBuildDocumentStructure(false)
    {
    }

    void appendBytes(const char*, size_t) override;

    void createDocumentStructure();

    bool m_didBuildDocumentStructure;
};

class MediaDownloadEventListener final : public EventListener {
public:
    static MediaDownloadEventListener* create()
    {
        return new MediaDownloadEventListener();
    }

    bool operator==(const EventListener& other) const override
    {
        return this == &other;
    }

private:
    MediaDownloadEventListener()
        : EventListener(CPPEventListenerType)
        , m_clicked(false)
    {
    }

    void handleEvent(ExecutionContext* context, Event* event) override
    {
        if (!m_clicked) {
            recordDownloadMetric(MediaDocumentDownloadButtonClicked);
            m_clicked = true;
        }
    }

    bool m_clicked;
};

void MediaDocumentParser::createDocumentStructure()
{
    ASSERT(document());
    HTMLHtmlElement* rootElement = HTMLHtmlElement::create(*document());
    document()->appendChild(rootElement);
    rootElement->insertedByParser();

    if (isDetached())
        return; // runScriptsAtDocumentElementAvailable can detach the frame.

    HTMLHeadElement* head = HTMLHeadElement::create(*document());
    HTMLMetaElement* meta = HTMLMetaElement::create(*document());
    meta->setAttribute(nameAttr, "viewport");
    meta->setAttribute(contentAttr, "width=device-width");
    head->appendChild(meta);

    HTMLVideoElement* media = HTMLVideoElement::create(*document());
    media->setAttribute(controlsAttr, "");
    media->setAttribute(autoplayAttr, "");
    media->setAttribute(nameAttr, "media");

    HTMLSourceElement* source = HTMLSourceElement::create(*document());
    source->setSrc(document()->url());

    if (DocumentLoader* loader = document()->loader())
        source->setType(loader->responseMIMEType());

    media->appendChild(source);

    HTMLBodyElement* body = HTMLBodyElement::create(*document());
    body->setAttribute(styleAttr, "margin: 0px;");

    HTMLDivElement* div = HTMLDivElement::create(*document());
    // Style sheets for media controls are lazily loaded until a media element is encountered.
    // As a result, elements encountered before the media element will not get the right
    // style at first if we put the styles in mediacontrols.css. To solve this issue, set the
    // styles inline so that they will be applied when the page loads.
    // See w3c example on how to centering an element: https://www.w3.org/Style/Examples/007/center.en.html
    div->setAttribute(styleAttr,
        "display: flex;"
        "flex-direction: column;"
        "justify-content: center;"
        "align-items: center;"
        "min-height: min-content;"
        "height: 100%;");
    HTMLContentElement* content = HTMLContentElement::create(*document());
    div->appendChild(content);

    if (RuntimeEnabledFeatures::mediaDocumentDownloadButtonEnabled()) {
        HTMLAnchorElement* anchor = HTMLAnchorElement::create(*document());
        anchor->setAttribute(downloadAttr, "");
        anchor->setURL(document()->url());
        anchor->setTextContent(document()->getCachedLocale(document()->contentLanguage()).queryString(WebLocalizedString::DownloadButtonLabel).upper());
        // Using CSS style according to Android material design.
        anchor->setAttribute(styleAttr,
            "display: inline-block;"
            "margin-top: 32px;"
            "padding: 0 16px 0 16px;"
            "height: 36px;"
            "background: #000000;"
            "-webkit-tap-highlight-color: rgba(255, 255, 255, 0.12);"
            "font-family: Roboto;"
            "font-size: 14px;"
            "border-radius: 5px;"
            "color: white;"
            "font-weight: 500;"
            "text-decoration: none;"
            "line-height: 36px;");
        EventListener* listener = MediaDownloadEventListener::create();
        anchor->addEventListener(EventTypeNames::click, listener, false);
        HTMLDivElement* buttonContainer = HTMLDivElement::create(*document());
        buttonContainer->setAttribute(styleAttr,
            "text-align: center;"
            "height: 0;"
            "flex: none");
        buttonContainer->appendChild(anchor);
        div->appendChild(buttonContainer);
        recordDownloadMetric(MediaDocumentDownloadButtonShown);
    }

    // According to https://html.spec.whatwg.org/multipage/browsers.html#read-media,
    // MediaDocument should have a single child which is the video element. Use
    // shadow root to hide all the elements we added here.
    ShadowRoot& shadowRoot = body->ensureUserAgentShadowRoot();
    shadowRoot.appendChild(div);
    body->appendChild(media);
    rootElement->appendChild(head);
    rootElement->appendChild(body);

    m_didBuildDocumentStructure = true;
}

void MediaDocumentParser::appendBytes(const char*, size_t)
{
    if (m_didBuildDocumentStructure)
        return;

    LocalFrame* frame = document()->frame();
    if (!frame->loader().client()->allowMedia(document()->url()))
        return;

    createDocumentStructure();
    finish();
}

MediaDocument::MediaDocument(const DocumentInit& initializer)
    : HTMLDocument(initializer, MediaDocumentClass)
{
    setCompatibilityMode(QuirksMode);
    lockCompatibilityMode();
    UseCounter::count(*this, UseCounter::MediaDocument);
    if (!isInMainFrame())
        UseCounter::count(*this, UseCounter::MediaDocumentInFrame);
}

DocumentParser* MediaDocument::createParser()
{
    return MediaDocumentParser::create(this);
}

void MediaDocument::defaultEventHandler(Event* event)
{
    Node* targetNode = event->target()->toNode();
    if (!targetNode)
        return;

    if (event->type() == EventTypeNames::keydown && event->isKeyboardEvent()) {
        HTMLVideoElement* video = Traversal<HTMLVideoElement>::firstWithin(*targetNode);
        if (!video)
            return;

        KeyboardEvent* keyboardEvent = toKeyboardEvent(event);
        if (keyboardEvent->key() == " " || keyboardEvent->keyCode() == VKEY_MEDIA_PLAY_PAUSE) {
            // space or media key (play/pause)
            video->togglePlayState();
            event->setDefaultHandled();
        }
    }
}

} // namespace blink
