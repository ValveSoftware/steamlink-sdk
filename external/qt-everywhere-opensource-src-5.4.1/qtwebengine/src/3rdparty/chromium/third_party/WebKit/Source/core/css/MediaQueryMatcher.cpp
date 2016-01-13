/*
 * Copyright (C) 2010 Nokia Corporation and/or its subsidiary(-ies)
 *
 *  This library is free software; you can redistribute it and/or
 *  modify it under the terms of the GNU Library General Public
 *  License as published by the Free Software Foundation; either
 *  version 2 of the License, or (at your option) any later version.
 *
 *  This library is distributed in the hope that it will be useful,
 *  but WITHOUT ANY WARRANTY; without even the implied warranty of
 *  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 *  Library General Public License for more details.
 *
 *  You should have received a copy of the GNU Library General Public License
 *  along with this library; see the file COPYING.LIB.  If not, write to
 *  the Free Software Foundation, Inc., 51 Franklin Street, Fifth Floor,
 *  Boston, MA 02110-1301, USA.
 */

#include "config.h"
#include "core/css/MediaQueryMatcher.h"

#include "core/css/MediaList.h"
#include "core/css/MediaQueryEvaluator.h"
#include "core/css/MediaQueryList.h"
#include "core/css/MediaQueryListListener.h"
#include "core/css/resolver/StyleResolver.h"
#include "core/dom/Document.h"
#include "core/frame/FrameView.h"
#include "core/frame/LocalFrame.h"

namespace WebCore {

MediaQueryMatcher::Listener::Listener(PassRefPtrWillBeRawPtr<MediaQueryListListener> listener, PassRefPtrWillBeRawPtr<MediaQueryList> query)
    : m_listener(listener)
    , m_query(query)
{
}

void MediaQueryMatcher::Listener::evaluate(MediaQueryEvaluator* evaluator)
{
    if (m_query->evaluate(evaluator))
        m_listener->queryChanged(m_query.get());
}

void MediaQueryMatcher::Listener::trace(Visitor* visitor)
{
    visitor->trace(m_listener);
    visitor->trace(m_query);
}

MediaQueryMatcher::MediaQueryMatcher(Document* document)
    : m_document(document)
    , m_evaluationRound(1)
{
    ASSERT(m_document);
}

DEFINE_EMPTY_DESTRUCTOR_WILL_BE_REMOVED(MediaQueryMatcher)

void MediaQueryMatcher::documentDestroyed()
{
    m_listeners.clear();
    m_document = nullptr;
}

AtomicString MediaQueryMatcher::mediaType() const
{
    if (!m_document || !m_document->frame() || !m_document->frame()->view())
        return nullAtom;

    return m_document->frame()->view()->mediaType();
}

PassOwnPtr<MediaQueryEvaluator> MediaQueryMatcher::prepareEvaluator() const
{
    if (!m_document || !m_document->frame())
        return nullptr;

    return adoptPtr(new MediaQueryEvaluator(mediaType(), m_document->frame()));
}

bool MediaQueryMatcher::evaluate(const MediaQuerySet* media)
{
    if (!media)
        return false;

    OwnPtr<MediaQueryEvaluator> evaluator(prepareEvaluator());
    return evaluator && evaluator->eval(media);
}

PassRefPtrWillBeRawPtr<MediaQueryList> MediaQueryMatcher::matchMedia(const String& query)
{
    if (!m_document)
        return nullptr;

    RefPtrWillBeRawPtr<MediaQuerySet> media = MediaQuerySet::create(query);
    // Add warning message to inspector whenever dpi/dpcm values are used for "screen" media.
    reportMediaQueryWarningIfNeeded(m_document, media.get());
    return MediaQueryList::create(this, media, evaluate(media.get()));
}

void MediaQueryMatcher::addListener(PassRefPtrWillBeRawPtr<MediaQueryListListener> listener, PassRefPtrWillBeRawPtr<MediaQueryList> query)
{
    if (!m_document)
        return;

    for (size_t i = 0; i < m_listeners.size(); ++i) {
        if (*m_listeners[i]->listener() == *listener && m_listeners[i]->query() == query)
            return;
    }

    m_listeners.append(adoptPtrWillBeNoop(new Listener(listener, query)));
}

void MediaQueryMatcher::removeListener(MediaQueryListListener* listener, MediaQueryList* query)
{
    if (!m_document)
        return;

    for (size_t i = 0; i < m_listeners.size(); ++i) {
        if (*m_listeners[i]->listener() == *listener && m_listeners[i]->query() == query) {
            m_listeners.remove(i);
            return;
        }
    }
}

void MediaQueryMatcher::styleResolverChanged()
{
    if (!m_document)
        return;

    ++m_evaluationRound;
    OwnPtr<MediaQueryEvaluator> evaluator = prepareEvaluator();
    if (!evaluator)
        return;

    for (size_t i = 0; i < m_listeners.size(); ++i)
        m_listeners[i]->evaluate(evaluator.get());
}

void MediaQueryMatcher::trace(Visitor* visitor)
{
    visitor->trace(m_document);
    // We don't support tracing of vectors of OwnPtrs (ie. Vector<OwnPtr<Listener> >).
    // Since this is a transitional object we are just ifdef'ing it out when oilpan is not enabled.
#if ENABLE(OILPAN)
    visitor->trace(m_listeners);
#endif
}

}
