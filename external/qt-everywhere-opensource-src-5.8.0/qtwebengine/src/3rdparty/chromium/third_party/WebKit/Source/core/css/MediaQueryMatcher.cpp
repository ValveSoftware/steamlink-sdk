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

#include "core/css/MediaQueryMatcher.h"

#include "core/css/MediaList.h"
#include "core/css/MediaQueryEvaluator.h"
#include "core/css/MediaQueryList.h"
#include "core/css/MediaQueryListEvent.h"
#include "core/css/MediaQueryListListener.h"
#include "core/css/resolver/StyleResolver.h"
#include "core/dom/Document.h"
#include "core/frame/FrameView.h"
#include "core/frame/LocalFrame.h"
#include "wtf/Vector.h"

namespace blink {

MediaQueryMatcher* MediaQueryMatcher::create(Document& document)
{
    return new MediaQueryMatcher(document);
}

MediaQueryMatcher::MediaQueryMatcher(Document& document)
    : m_document(&document)
{
    ASSERT(m_document);
}

MediaQueryMatcher::~MediaQueryMatcher()
{
}

void MediaQueryMatcher::documentDetached()
{
    m_document = nullptr;
    m_evaluator = nullptr;
}

MediaQueryEvaluator* MediaQueryMatcher::createEvaluator() const
{
    if (!m_document || !m_document->frame())
        return nullptr;

    return new MediaQueryEvaluator(m_document->frame());
}

bool MediaQueryMatcher::evaluate(const MediaQuerySet* media)
{
    ASSERT(!m_document || m_document->frame() || !m_evaluator);

    if (!media)
        return false;

    // Cache the evaluator to avoid allocating one per evaluation.
    if (!m_evaluator)
        m_evaluator = createEvaluator();

    if (m_evaluator)
        return m_evaluator->eval(media);

    return false;
}

MediaQueryList* MediaQueryMatcher::matchMedia(const String& query)
{
    if (!m_document)
        return nullptr;

    MediaQuerySet* media = MediaQuerySet::create(query);
    return MediaQueryList::create(m_document, this, media);
}

void MediaQueryMatcher::addMediaQueryList(MediaQueryList* query)
{
    if (!m_document)
        return;
    m_mediaLists.add(query);
}

void MediaQueryMatcher::removeMediaQueryList(MediaQueryList* query)
{
    if (!m_document)
        return;
    m_mediaLists.remove(query);
}

void MediaQueryMatcher::addViewportListener(MediaQueryListListener* listener)
{
    if (!m_document)
        return;
    m_viewportListeners.add(listener);
}

void MediaQueryMatcher::removeViewportListener(MediaQueryListListener* listener)
{
    if (!m_document)
        return;
    m_viewportListeners.remove(listener);
}

void MediaQueryMatcher::mediaFeaturesChanged()
{
    if (!m_document)
        return;

    HeapVector<Member<MediaQueryListListener>> listenersToNotify;
    for (const auto& list : m_mediaLists) {
        if (list->mediaFeaturesChanged(&listenersToNotify)) {
            Event* event = MediaQueryListEvent::create(list);
            event->setTarget(list);
            m_document->enqueueUniqueAnimationFrameEvent(event);
        }
    }
    m_document->enqueueMediaQueryChangeListeners(listenersToNotify);
}

void MediaQueryMatcher::viewportChanged()
{
    if (!m_document)
        return;

    HeapVector<Member<MediaQueryListListener>> listenersToNotify;
    for (const auto& listener : m_viewportListeners)
        listenersToNotify.append(listener);

    m_document->enqueueMediaQueryChangeListeners(listenersToNotify);
}

DEFINE_TRACE(MediaQueryMatcher)
{
    visitor->trace(m_document);
    visitor->trace(m_evaluator);
    visitor->trace(m_mediaLists);
    visitor->trace(m_viewportListeners);
}

} // namespace blink
