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

#ifndef MediaQueryMatcher_h
#define MediaQueryMatcher_h

#include "platform/heap/Handle.h"
#include "wtf/Forward.h"
#include "wtf/RefCounted.h"
#include "wtf/Vector.h"

namespace WebCore {

class Document;
class MediaQueryList;
class MediaQueryListListener;
class MediaQueryEvaluator;
class MediaQuerySet;

// MediaQueryMatcher class is responsible for keeping a vector of pairs
// MediaQueryList x MediaQueryListListener. It is responsible for evaluating the queries
// whenever it is needed and to call the listeners if the corresponding query has changed.
// The listeners must be called in the very same order in which they have been added.

class MediaQueryMatcher FINAL : public RefCountedWillBeGarbageCollected<MediaQueryMatcher> {
    DECLARE_EMPTY_DESTRUCTOR_WILL_BE_REMOVED(MediaQueryMatcher);
public:
    static PassRefPtrWillBeRawPtr<MediaQueryMatcher> create(Document* document) { return adoptRefWillBeNoop(new MediaQueryMatcher(document)); }
    void documentDestroyed();

    void addListener(PassRefPtrWillBeRawPtr<MediaQueryListListener>, PassRefPtrWillBeRawPtr<MediaQueryList>);
    void removeListener(MediaQueryListListener*, MediaQueryList*);

    PassRefPtrWillBeRawPtr<MediaQueryList> matchMedia(const String&);

    unsigned evaluationRound() const { return m_evaluationRound; }
    void styleResolverChanged();
    bool evaluate(const MediaQuerySet*);

    void trace(Visitor*);

private:
    class Listener FINAL : public NoBaseWillBeGarbageCollected<Listener> {
    public:
        Listener(PassRefPtrWillBeRawPtr<MediaQueryListListener>, PassRefPtrWillBeRawPtr<MediaQueryList>);
        void evaluate(MediaQueryEvaluator*);

        MediaQueryListListener* listener() { return m_listener.get(); }
        MediaQueryList* query() { return m_query.get(); }

        void trace(Visitor*);

    private:
        RefPtrWillBeMember<MediaQueryListListener> m_listener;
        RefPtrWillBeMember<MediaQueryList> m_query;
    };

    MediaQueryMatcher(Document*);
    PassOwnPtr<MediaQueryEvaluator> prepareEvaluator() const;
    AtomicString mediaType() const;

    RawPtrWillBeMember<Document> m_document;
    WillBeHeapVector<OwnPtrWillBeMember<Listener> > m_listeners;

    // This value is incremented at style selector changes.
    // It is used to avoid evaluating queries more then once and to make sure
    // that a media query result change is notified exactly once.
    unsigned m_evaluationRound;
};

}

#endif // MediaQueryMatcher_h
