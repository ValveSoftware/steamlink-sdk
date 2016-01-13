/*
 * Copyright (C) 1999 Lars Knoll (knoll@kde.org)
 *           (C) 1999 Antti Koivisto (koivisto@kde.org)
 * Copyright (C) 2003, 2004, 2005, 2006, 2007, 2008 Apple Inc. All rights reserved.
 * Copyright (C) 2014 Samsung Electronics. All rights reserved.
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

#ifndef HTMLFormControlsCollection_h
#define HTMLFormControlsCollection_h

#include "core/html/FormAssociatedElement.h"
#include "core/html/HTMLCollection.h"
#include "core/html/RadioNodeList.h"

namespace WebCore {

class HTMLElement;
class HTMLImageElement;
class QualifiedName;

// This class is just a big hack to find form elements even in malformed HTML elements.
// The famous <table><tr><form><td> problem.

class HTMLFormControlsCollection FINAL : public HTMLCollection {
public:
    static PassRefPtrWillBeRawPtr<HTMLFormControlsCollection> create(ContainerNode&, CollectionType);

    virtual ~HTMLFormControlsCollection();

    virtual Element* namedItem(const AtomicString& name) const OVERRIDE;
    void namedGetter(const AtomicString& name, bool& radioNodeListEnabled, RefPtrWillBeRawPtr<RadioNodeList>&, bool& elementEnabled, RefPtrWillBeRawPtr<Element>&);

    virtual void trace(Visitor*) OVERRIDE;

private:
    explicit HTMLFormControlsCollection(ContainerNode&);

    virtual void updateIdNameCache() const OVERRIDE;
    virtual void supportedPropertyNames(Vector<String>& names) OVERRIDE;

    const FormAssociatedElement::List& formControlElements() const;
    const WillBeHeapVector<RawPtrWillBeMember<HTMLImageElement> >& formImageElements() const;
    virtual Element* virtualItemAfter(Element*) const OVERRIDE;
    virtual void invalidateCache(Document* oldDocument = 0) const OVERRIDE;

    mutable RawPtrWillBeMember<Element> m_cachedElement;
    mutable unsigned m_cachedElementOffsetInArray;
};
DEFINE_TYPE_CASTS(HTMLFormControlsCollection, LiveNodeListBase, collection, collection->type() == FormControls, collection.type() == FormControls);

} // namespace

#endif
