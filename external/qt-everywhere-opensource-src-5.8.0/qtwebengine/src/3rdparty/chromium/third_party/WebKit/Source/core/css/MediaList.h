/*
 * (C) 1999-2003 Lars Knoll (knoll@kde.org)
 * Copyright (C) 2004, 2006, 2008, 2009, 2010, 2012 Apple Inc. All rights reserved.
 *
 * This library is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Library General Public
 * License as published by the Free Software Foundation; either
 * version 2 of the License, or (at your option) any later version.
 *
 * This library is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * Library General Public License for more details.
 *
 * You should have received a copy of the GNU Library General Public License
 * along with this library; see the file COPYING.LIB.  If not, write to
 * the Free Software Foundation, Inc., 51 Franklin Street, Fifth Floor,
 * Boston, MA 02110-1301, USA.
 */

#ifndef MediaList_h
#define MediaList_h

#include "bindings/core/v8/ScriptWrappable.h"
#include "core/CoreExport.h"
#include "core/dom/ExceptionCode.h"
#include "platform/heap/Handle.h"
#include "wtf/Forward.h"
#include "wtf/Vector.h"
#include "wtf/text/WTFString.h"

namespace blink {

class CSSRule;
class CSSStyleSheet;
class ExceptionState;
class MediaList;
class MediaQuery;

class CORE_EXPORT MediaQuerySet : public GarbageCollected<MediaQuerySet> {
public:
    static MediaQuerySet* create()
    {
        return new MediaQuerySet();
    }
    static MediaQuerySet* create(const String& mediaString);
    static MediaQuerySet* createOffMainThread(const String& mediaString);

    bool set(const String&);
    bool add(const String&);
    bool remove(const String&);

    void addMediaQuery(MediaQuery*);

    const HeapVector<Member<MediaQuery>>& queryVector() const { return m_queries; }

    String mediaText() const;

    MediaQuerySet* copy() const { return new MediaQuerySet(*this); }

    DECLARE_TRACE();

private:
    MediaQuerySet();
    MediaQuerySet(const MediaQuerySet&);

    HeapVector<Member<MediaQuery>> m_queries;
};

class MediaList final : public GarbageCollected<MediaList>, public ScriptWrappable {
    DEFINE_WRAPPERTYPEINFO();
public:
    static MediaList* create(MediaQuerySet* mediaQueries, CSSStyleSheet* parentSheet)
    {
        return new MediaList(mediaQueries, parentSheet);
    }

    static MediaList* create(MediaQuerySet* mediaQueries, CSSRule* parentRule)
    {
        return new MediaList(mediaQueries, parentRule);
    }

    unsigned length() const { return m_mediaQueries->queryVector().size(); }
    String item(unsigned index) const;
    void deleteMedium(const String& oldMedium, ExceptionState&);
    void appendMedium(const String& newMedium, ExceptionState&);

    String mediaText() const { return m_mediaQueries->mediaText(); }
    void setMediaText(const String&);

    // Not part of CSSOM.
    CSSRule* parentRule() const { return m_parentRule; }
    CSSStyleSheet* parentStyleSheet() const { return m_parentStyleSheet; }

    const MediaQuerySet* queries() const { return m_mediaQueries.get(); }

    void reattach(MediaQuerySet*);

    DECLARE_TRACE();

private:
    MediaList(MediaQuerySet*, CSSStyleSheet* parentSheet);
    MediaList(MediaQuerySet*, CSSRule* parentRule);

    Member<MediaQuerySet> m_mediaQueries;
    // Cleared in ~CSSStyleSheet destructor when oilpan is not enabled.
    Member<CSSStyleSheet> m_parentStyleSheet;
    // Cleared in the ~CSSMediaRule and ~CSSImportRule destructors when oilpan is not enabled.
    Member<CSSRule> m_parentRule;
};

} // namespace blink

#endif
