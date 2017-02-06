/*
 * Copyright (C) 1999 Lars Knoll (knoll@kde.org)
 * Copyright (C) 2003, 2004, 2005, 2006, 2007, 2008, 2009, 2010, 2011 Apple Inc. All rights reserved.
 * Copyright (C) 2013 Google Inc. All rights reserved.
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
 *
 */

#ifndef ElementStyleResources_h
#define ElementStyleResources_h

#include "core/CSSPropertyNames.h"
#include "core/css/CSSPropertyIDTemplates.h"
#include "platform/CrossOriginAttributeValue.h"
#include "platform/graphics/Color.h"
#include "platform/heap/Handle.h"
#include "wtf/HashMap.h"
#include "wtf/Noncopyable.h"

namespace blink {

class CSSCursorImageValue;
class CSSImageGeneratorValue;
class CSSImageSetValue;
class CSSImageValue;
class CSSSVGDocumentValue;
class CSSValue;
class ComputedStyle;
class Document;
class FilterOperation;
class StyleImage;
class StylePendingImage;

// Holds information about resources, requested by stylesheets.
// Lifetime: per-element style resolve.
class ElementStyleResources {
    STACK_ALLOCATED();
    WTF_MAKE_NONCOPYABLE(ElementStyleResources);
public:
    ElementStyleResources(Document&, float deviceScaleFactor);

    StyleImage* styleImage(CSSPropertyID, const CSSValue&);
    StyleImage* cachedOrPendingFromValue(CSSPropertyID, const CSSImageValue&);
    StyleImage* setOrPendingFromValue(CSSPropertyID, const CSSImageSetValue&);

    void loadPendingResources(ComputedStyle*);

    void addPendingSVGDocument(FilterOperation*, const CSSSVGDocumentValue*);

private:
    StyleImage* cursorOrPendingFromValue(CSSPropertyID, const CSSCursorImageValue&);
    StyleImage* generatedOrPendingFromValue(CSSPropertyID, const CSSImageGeneratorValue&);

    void loadPendingSVGDocuments(ComputedStyle*);
    void loadPendingImages(ComputedStyle*);

    StyleImage* loadPendingImage(ComputedStyle*, StylePendingImage*, CrossOriginAttributeValue = CrossOriginAttributeNotSet);

    Member<Document> m_document;
    HashSet<CSSPropertyID> m_pendingImageProperties;
    HeapHashMap<Member<FilterOperation>, Member<const CSSSVGDocumentValue>> m_pendingSVGDocuments;
    float m_deviceScaleFactor;
};

} // namespace blink

#endif // ElementStyleResources_h
