/*
 * Copyright (C) 1999 Lars Knoll (knoll@kde.org)
 *           (C) 1999 Antti Koivisto (koivisto@kde.org)
 *           (C) 2000 Dirk Mueller (mueller@kde.org)
 * Copyright (C) 2004, 2005, 2006, 2007, 2008 Apple Inc. All rights reserved.
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

#ifndef HTMLOptionsCollection_h
#define HTMLOptionsCollection_h

#include "core/html/HTMLCollection.h"

namespace WebCore {

class ExceptionState;
class HTMLOptionElement;
class HTMLSelectElement;

class HTMLOptionsCollection FINAL : public HTMLCollection {
public:
    static PassRefPtrWillBeRawPtr<HTMLOptionsCollection> create(ContainerNode&, CollectionType);

    void add(PassRefPtrWillBeRawPtr<HTMLOptionElement>, ExceptionState&);
    void add(PassRefPtrWillBeRawPtr<HTMLOptionElement>, int index, ExceptionState&);
    void remove(int index);
    void remove(HTMLOptionElement*);

    int selectedIndex() const;
    void setSelectedIndex(int);

    void setLength(unsigned, ExceptionState&);
    void namedGetter(const AtomicString& name, bool&, RefPtrWillBeRawPtr<NodeList>&, bool&, RefPtrWillBeRawPtr<Element>&);
    bool anonymousIndexedSetter(unsigned, PassRefPtrWillBeRawPtr<HTMLOptionElement>, ExceptionState&);

private:
    explicit HTMLOptionsCollection(ContainerNode&);

    virtual void supportedPropertyNames(Vector<String>& names) OVERRIDE;
};

DEFINE_TYPE_CASTS(HTMLOptionsCollection, LiveNodeListBase, collection, collection->type() == SelectOptions, collection.type() == SelectOptions);

} //namespace

#endif
