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

#include "qbuiltintypes_p.h"

#include "qnonetype_p.h"

QT_BEGIN_NAMESPACE

using namespace QPatternist;

NoneType::NoneType()
{
}

bool NoneType::itemMatches(const Item &) const
{
    return false;
}

bool NoneType::xdtTypeMatches(const ItemType::Ptr &t) const
{
    return *this == *t;
}

const ItemType &NoneType::operator|(const ItemType &other) const
{
    return other;
}

QString NoneType::displayName(const NamePool::Ptr &) const
{
    return QLatin1String("none");
}

Cardinality NoneType::cardinality() const
{
    return Cardinality::zeroOrMore();
}

ItemType::Ptr NoneType::itemType() const
{
    return ItemType::Ptr(const_cast<NoneType *>(this));
}

bool NoneType::isAtomicType() const
{
    return false;
}

bool NoneType::isNodeType() const
{
    return false;
}

ItemType::Ptr NoneType::atomizedType() const
{
    return BuiltinTypes::xsAnyAtomicType;
}

ItemType::Ptr NoneType::xdtSuperType() const
{
    return BuiltinTypes::item;
}

QT_END_NAMESPACE
