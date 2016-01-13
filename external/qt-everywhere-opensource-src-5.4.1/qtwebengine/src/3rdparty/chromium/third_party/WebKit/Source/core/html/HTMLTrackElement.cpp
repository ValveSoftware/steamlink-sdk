/*
 * Copyright (C) 2011 Google Inc. All rights reserved.
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

#include "config.h"
#include "core/html/HTMLTrackElement.h"

#include "bindings/v8/ExceptionStatePlaceholder.h"
#include "core/HTMLNames.h"
#include "core/events/Event.h"
#include "core/frame/csp/ContentSecurityPolicy.h"
#include "core/html/HTMLMediaElement.h"
#include "platform/Logging.h"

namespace WebCore {

using namespace HTMLNames;

#if !LOG_DISABLED
static String urlForLoggingTrack(const KURL& url)
{
    static const unsigned maximumURLLengthForLogging = 128;

    if (url.string().length() < maximumURLLengthForLogging)
        return url.string();
    return url.string().substring(0, maximumURLLengthForLogging) + "...";
}
#endif

inline HTMLTrackElement::HTMLTrackElement(Document& document)
    : HTMLElement(trackTag, document)
    , m_loadTimer(this, &HTMLTrackElement::loadTimerFired)
{
    WTF_LOG(Media, "HTMLTrackElement::HTMLTrackElement - %p", this);
    ScriptWrappable::init(this);
}

DEFINE_NODE_FACTORY(HTMLTrackElement)

HTMLTrackElement::~HTMLTrackElement()
{
#if !ENABLE(OILPAN)
    if (m_track)
        m_track->clearTrackElement();
#endif
}

Node::InsertionNotificationRequest HTMLTrackElement::insertedInto(ContainerNode* insertionPoint)
{
    WTF_LOG(Media, "HTMLTrackElement::insertedInto");

    // Since we've moved to a new parent, we may now be able to load.
    scheduleLoad();

    HTMLElement::insertedInto(insertionPoint);
    HTMLMediaElement* parent = mediaElement();
    if (insertionPoint == parent)
        parent->didAddTrackElement(this);
    return InsertionDone;
}

void HTMLTrackElement::removedFrom(ContainerNode* insertionPoint)
{
    if (!parentNode() && isHTMLMediaElement(*insertionPoint))
        toHTMLMediaElement(insertionPoint)->didRemoveTrackElement(this);
    HTMLElement::removedFrom(insertionPoint);
}

void HTMLTrackElement::parseAttribute(const QualifiedName& name, const AtomicString& value)
{
    if (name == srcAttr) {
        if (!value.isEmpty())
            scheduleLoad();
        else if (m_track)
            m_track->removeAllCues();

    // 4.8.10.12.3 Sourcing out-of-band text tracks
    // As the kind, label, and srclang attributes are set, changed, or removed, the text track must update accordingly...
    } else if (name == kindAttr) {
        track()->setKind(value.lower());
    } else if (name == labelAttr) {
        track()->setLabel(value);
    } else if (name == srclangAttr) {
        track()->setLanguage(value);
    } else if (name == idAttr) {
        track()->setId(value);
    } else if (name == defaultAttr) {
        track()->setIsDefault(!value.isNull());
    }

    HTMLElement::parseAttribute(name, value);
}

const AtomicString& HTMLTrackElement::kind()
{
    return track()->kind();
}

void HTMLTrackElement::setKind(const AtomicString& kind)
{
    setAttribute(kindAttr, kind);
}

LoadableTextTrack* HTMLTrackElement::ensureTrack()
{
    if (!m_track) {
        // kind, label and language are updated by parseAttribute
        m_track = LoadableTextTrack::create(this);
    }
    return m_track.get();
}

TextTrack* HTMLTrackElement::track()
{
    return ensureTrack();
}

bool HTMLTrackElement::isURLAttribute(const Attribute& attribute) const
{
    return attribute.name() == srcAttr || HTMLElement::isURLAttribute(attribute);
}

void HTMLTrackElement::scheduleLoad()
{
    WTF_LOG(Media, "HTMLTrackElement::scheduleLoad");

    // 1. If another occurrence of this algorithm is already running for this text track and its track element,
    // abort these steps, letting that other algorithm take care of this element.
    if (m_loadTimer.isActive())
        return;

    // 2. If the text track's text track mode is not set to one of hidden or showing, abort these steps.
    if (ensureTrack()->mode() != TextTrack::hiddenKeyword() && ensureTrack()->mode() != TextTrack::showingKeyword())
        return;

    // 3. If the text track's track element does not have a media element as a parent, abort these steps.
    if (!mediaElement())
        return;

    // 4. Run the remainder of these steps asynchronously, allowing whatever caused these steps to run to continue.
    m_loadTimer.startOneShot(0, FROM_HERE);
}

void HTMLTrackElement::loadTimerFired(Timer<HTMLTrackElement>*)
{
    if (!fastHasAttribute(srcAttr))
        return;

    WTF_LOG(Media, "HTMLTrackElement::loadTimerFired");

    // 6. Set the text track readiness state to loading.
    setReadyState(HTMLTrackElement::LOADING);

    // 7. Let URL be the track URL of the track element.
    KURL url = getNonEmptyURLAttribute(srcAttr);

    // 8. If the track element's parent is a media element then let CORS mode be the state of the parent media
    // element's crossorigin content attribute. Otherwise, let CORS mode be No CORS.
    if (!canLoadUrl(url)) {
        didCompleteLoad(HTMLTrackElement::Failure);
        return;
    }

    ensureTrack()->scheduleLoad(url);
}

bool HTMLTrackElement::canLoadUrl(const KURL& url)
{
    HTMLMediaElement* parent = mediaElement();
    if (!parent)
        return false;

    // 4.8.10.12.3 Sourcing out-of-band text tracks

    // 4. Download: If URL is not the empty string, perform a potentially CORS-enabled fetch of URL, with the
    // mode being the state of the media element's crossorigin content attribute, the origin being the
    // origin of the media element's Document, and the default origin behaviour set to fail.
    if (url.isEmpty())
        return false;

    if (!document().contentSecurityPolicy()->allowMediaFromSource(url)) {
        WTF_LOG(Media, "HTMLTrackElement::canLoadUrl(%s) -> rejected by Content Security Policy", urlForLoggingTrack(url).utf8().data());
        return false;
    }

    return true;
}

void HTMLTrackElement::didCompleteLoad(LoadStatus status)
{
    // 4.8.10.12.3 Sourcing out-of-band text tracks (continued)

    // 4. Download: ...
    // If the fetching algorithm fails for any reason (network error, the server returns an error
    // code, a cross-origin check fails, etc), or if URL is the empty string or has the wrong origin
    // as determined by the condition at the start of this step, or if the fetched resource is not in
    // a supported format, then queue a task to first change the text track readiness state to failed
    // to load and then fire a simple event named error at the track element; and then, once that task
    // is queued, move on to the step below labeled monitoring.

    if (status == Failure) {
        setReadyState(HTMLTrackElement::TRACK_ERROR);
        dispatchEvent(Event::create(EventTypeNames::error), IGNORE_EXCEPTION);
        return;
    }

    // If the fetching algorithm does not fail, then the final task that is queued by the networking
    // task source must run the following steps:
    //     1. Change the text track readiness state to loaded.
    setReadyState(HTMLTrackElement::LOADED);

    //     2. If the file was successfully processed, fire a simple event named load at the
    //        track element.
    dispatchEvent(Event::create(EventTypeNames::load), IGNORE_EXCEPTION);
}

// NOTE: The values in the TextTrack::ReadinessState enum must stay in sync with those in HTMLTrackElement::ReadyState.
COMPILE_ASSERT(HTMLTrackElement::NONE == static_cast<HTMLTrackElement::ReadyState>(TextTrack::NotLoaded), TextTrackEnumNotLoaded_Is_Wrong_Should_Be_HTMLTrackElementEnumNONE);
COMPILE_ASSERT(HTMLTrackElement::LOADING == static_cast<HTMLTrackElement::ReadyState>(TextTrack::Loading), TextTrackEnumLoadingIsWrong_ShouldBe_HTMLTrackElementEnumLOADING);
COMPILE_ASSERT(HTMLTrackElement::LOADED == static_cast<HTMLTrackElement::ReadyState>(TextTrack::Loaded), TextTrackEnumLoaded_Is_Wrong_Should_Be_HTMLTrackElementEnumLOADED);
COMPILE_ASSERT(HTMLTrackElement::TRACK_ERROR == static_cast<HTMLTrackElement::ReadyState>(TextTrack::FailedToLoad), TextTrackEnumFailedToLoad_Is_Wrong_Should_Be_HTMLTrackElementEnumTRACK_ERROR);

void HTMLTrackElement::setReadyState(ReadyState state)
{
    ensureTrack()->setReadinessState(static_cast<TextTrack::ReadinessState>(state));
    if (HTMLMediaElement* parent = mediaElement())
        return parent->textTrackReadyStateChanged(m_track.get());
}

HTMLTrackElement::ReadyState HTMLTrackElement::readyState()
{
    return static_cast<ReadyState>(ensureTrack()->readinessState());
}

const AtomicString& HTMLTrackElement::mediaElementCrossOriginAttribute() const
{
    if (HTMLMediaElement* parent = mediaElement())
        return parent->fastGetAttribute(HTMLNames::crossoriginAttr);

    return nullAtom;
}

HTMLMediaElement* HTMLTrackElement::mediaElement() const
{
    Element* parent = parentElement();
    if (isHTMLMediaElement(parent))
        return toHTMLMediaElement(parent);
    return 0;
}

void HTMLTrackElement::trace(Visitor* visitor)
{
    visitor->trace(m_track);
    HTMLElement::trace(visitor);
}

}
