/****************************************************************************
**
** Copyright (C) 2016 The Qt Company Ltd.
** Contact: https://www.qt.io/licensing/
**
** This file is part of the QtXmlPatterns module of the Qt Toolkit.
**
** $QT_BEGIN_LICENSE:LGPL$
** Commercial License Usage
** Licensees holding valid commercial Qt licenses may use this file in
** accordance with the commercial license agreement provided with the
** Software or, alternatively, in accordance with the terms contained in
** a written agreement between you and The Qt Company. For licensing terms
** and conditions see https://www.qt.io/terms-conditions. For further
** information use the contact form at https://www.qt.io/contact-us.
**
** GNU Lesser General Public License Usage
** Alternatively, this file may be used under the terms of the GNU Lesser
** General Public License version 3 as published by the Free Software
** Foundation and appearing in the file LICENSE.LGPL3 included in the
** packaging of this file. Please review the following information to
** ensure the GNU Lesser General Public License version 3 requirements
** will be met: https://www.gnu.org/licenses/lgpl-3.0.html.
**
** GNU General Public License Usage
** Alternatively, this file may be used under the terms of the GNU
** General Public License version 2.0 or (at your option) the GNU General
** Public license version 3 or any later version approved by the KDE Free
** Qt Foundation. The licenses are as published by the Free Software
** Foundation and appearing in the file LICENSE.GPL2 and LICENSE.GPL3
** included in the packaging of this file. Please review the following
** information to ensure the GNU General Public License requirements will
** be met: https://www.gnu.org/licenses/gpl-2.0.html and
** https://www.gnu.org/licenses/gpl-3.0.html.
**
** $QT_END_LICENSE$
**
****************************************************************************/

#include "qitem_p.h"
#include "qbuiltintypes_p.h"
#include "qitem_p.h"
#include "qschematypefactory_p.h"
#include "qxmlname.h"

#include "qatomictype_p.h"

QT_BEGIN_NAMESPACE

using namespace QPatternist;

AtomicType::AtomicType()
{
}

AtomicType::~AtomicType()
{
}

bool AtomicType::xdtTypeMatches(const ItemType::Ptr &other) const
{
    if(other->isAtomicType())
    {
        if(*other == *this)
            return true;
        else
            return xdtTypeMatches(other->xdtSuperType());
    }
    else
        return false;
}

bool AtomicType::itemMatches(const Item &item) const
{
    Q_ASSERT(item);
    if(item.isNode())
        return false;
    else
    {
        const SchemaType::Ptr t(static_cast<AtomicType *>(item.type().data()));
        return wxsTypeMatches(t);
    }
}

ItemType::Ptr AtomicType::atomizedType() const
{
    return AtomicType::Ptr(const_cast<AtomicType *>(this));
}

QString AtomicType::displayName(const NamePool::Ptr &) const
{
    /* A bit faster than calling name()->displayName() */
    return QLatin1String("xs:anyAtomicType");
}

bool AtomicType::isNodeType() const
{
    return false;
}

bool AtomicType::isAtomicType() const
{
    return true;
}

SchemaType::TypeCategory AtomicType::category() const
{
    return SimpleTypeAtomic;
}

SchemaType::DerivationMethod AtomicType::derivationMethod() const
{
    return DerivationRestriction;
}

QT_END_NAMESPACE
