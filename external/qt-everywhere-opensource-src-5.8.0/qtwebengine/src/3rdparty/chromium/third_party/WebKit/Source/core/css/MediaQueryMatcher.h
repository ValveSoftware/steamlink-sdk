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

#include "core/CoreExport.h"
#include "platform/heap/Handle.h"
#include "wtf/Forward.h"

namespace blink {

class Document;
class MediaQueryList;
class MediaQueryListListener;
class MediaQueryEvaluator;
class MediaQuerySet;

// MediaQueryMatcher class is responsible for keeping a vector of pairs
// MediaQueryList x MediaQueryListListener. It is responsible for evaluating the queries
// whenever it is needed and to call the listeners if the corresponding query has changed.
// The listeners must be called in the very same order in which they have been added.

class CORE_EXPORT MediaQueryMatcher final : public GarbageCollectedFinalized<MediaQueryMatcher> {
    WTF_MAKE_NONCOPYABLE(MediaQueryMatcher);
public:
    static MediaQueryMatcher* create(Document&);
    ~MediaQueryMatcher();

    void documentDetached();

    void addMediaQueryList(MediaQueryList*);
    void removeMediaQueryList(MediaQueryList*);

    void addViewportListener(MediaQueryListListener*);
    void removeViewportListener(MediaQueryListListener*);

    MediaQueryList* matchMedia(const String&);

    void mediaFeaturesChanged();
    void viewportChanged();
    bool evaluate(const MediaQuerySet*);

    DECLARE_TRACE();

private:
    explicit MediaQueryMatcher(Document&);

    MediaQueryEvaluator* createEvaluator() const;

    Member<Document> m_document;
    Member<MediaQueryEvaluator> m_evaluator;

    using MediaQueryListSet = HeapLinkedHashSet<WeakMember<MediaQueryList>>;
    MediaQueryListSet m_mediaLists;

    using ViewportListenerSet = HeapLinkedHashSet<Member<MediaQueryListListener>>;
    ViewportListenerSet m_viewportListeners;
};

} // namespace blink

#endif // MediaQueryMatcher_h
