/*
 * Copyright (C) 2005, 2006, 2009 Apple Inc. All rights reserved.
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

#include "core/dom/QualifiedName.h"

#include "core/HTMLNames.h"
#include "core/MathMLNames.h"
#include "core/SVGNames.h"
#include "core/XLinkNames.h"
#include "core/XMLNSNames.h"
#include "core/XMLNames.h"
#include "wtf/Assertions.h"
#include "wtf/HashSet.h"
#include "wtf/StaticConstructors.h"

namespace blink {

struct SameSizeAsQualifiedNameImpl : public RefCounted<SameSizeAsQualifiedNameImpl> {
    unsigned bitfield;
    void* pointers[4];
};

static_assert(sizeof(QualifiedName::QualifiedNameImpl) == sizeof(SameSizeAsQualifiedNameImpl), "QualifiedNameImpl should stay small");

using QualifiedNameCache = HashSet<QualifiedName::QualifiedNameImpl*, QualifiedNameHash>;

static QualifiedNameCache& qualifiedNameCache()
{
    // This code is lockless and thus assumes it all runs on one thread!
    DCHECK(isMainThread());
    static QualifiedNameCache* gNameCache = new QualifiedNameCache;
    return *gNameCache;
}

struct QNameComponentsTranslator {
    static unsigned hash(const QualifiedNameData& data)
    {
        return hashComponents(data.m_components);
    }
    static bool equal(QualifiedName::QualifiedNameImpl* name, const QualifiedNameData& data)
    {
        return data.m_components.m_prefix == name->m_prefix.impl()
            && data.m_components.m_localName == name->m_localName.impl()
            && data.m_components.m_namespace == name->m_namespace.impl();
    }
    static void translate(QualifiedName::QualifiedNameImpl*& location, const QualifiedNameData& data, unsigned)
    {
        const QualifiedNameComponents& components = data.m_components;
        location = QualifiedName::QualifiedNameImpl::create(AtomicString(components.m_prefix), AtomicString(components.m_localName), AtomicString(components.m_namespace), data.m_isStatic).leakRef();
    }
};

QualifiedName::QualifiedName(const AtomicString& p, const AtomicString& l, const AtomicString& n)
{
    QualifiedNameData data = { { p.impl(), l.impl(), n.isEmpty() ? nullAtom.impl() : n.impl() }, false };
    QualifiedNameCache::AddResult addResult = qualifiedNameCache().addWithTranslator<QNameComponentsTranslator>(data);
    m_impl = addResult.isNewEntry ? adoptRef(*addResult.storedValue) : *addResult.storedValue;
}

QualifiedName::QualifiedName(const AtomicString& p, const AtomicString& l, const AtomicString& n, bool isStatic)
{
    QualifiedNameData data = { { p.impl(), l.impl(), n.impl() }, isStatic };
    QualifiedNameCache::AddResult addResult = qualifiedNameCache().addWithTranslator<QNameComponentsTranslator>(data);
    m_impl = addResult.isNewEntry ? adoptRef(*addResult.storedValue) : *addResult.storedValue;
}

QualifiedName::~QualifiedName()
{
}

QualifiedName::QualifiedNameImpl::~QualifiedNameImpl()
{
    qualifiedNameCache().remove(this);
}

String QualifiedName::toString() const
{
    String local = localName();
    if (hasPrefix())
        return prefix().getString() + ":" + local;
    return local;
}

// Global init routines
DEFINE_GLOBAL(QualifiedName, anyName, nullAtom, starAtom, starAtom)
DEFINE_GLOBAL(QualifiedName, nullName, nullAtom, nullAtom, nullAtom)

void QualifiedName::initAndReserveCapacityForSize(unsigned size)
{
    DCHECK(starAtom.impl());
    qualifiedNameCache().reserveCapacityForSize(size + 2 /*starAtom and nullAtom */);
    new ((void*)&anyName) QualifiedName(nullAtom, starAtom, starAtom, true );
    new ((void*)&nullName) QualifiedName(nullAtom, nullAtom, nullAtom, true );
}

const QualifiedName& QualifiedName::null()
{
    return nullName;
}

const AtomicString& QualifiedName::localNameUpper() const
{
    if (!m_impl->m_localNameUpper)
        m_impl->m_localNameUpper = m_impl->m_localName.upper();
    return m_impl->m_localNameUpper;
}

unsigned QualifiedName::QualifiedNameImpl::computeHash() const
{
    QualifiedNameComponents components = { m_prefix.impl(), m_localName.impl(), m_namespace.impl() };
    return hashComponents(components);
}

void QualifiedName::createStatic(void* targetAddress, StringImpl* name, const AtomicString& nameNamespace)
{
    new (targetAddress) QualifiedName(nullAtom, AtomicString(name), nameNamespace, true);
}

void QualifiedName::createStatic(void* targetAddress, StringImpl* name)
{
    new (targetAddress) QualifiedName(nullAtom, AtomicString(name), nullAtom, true);
}

} // namespace blink
