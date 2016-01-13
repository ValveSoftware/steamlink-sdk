/****************************************************************************
**
** Copyright (C) 2014 Digia Plc and/or its subsidiary(-ies).
** Contact: http://www.qt-project.org/legal
**
** This file is part of the QtXmlPatterns module of the Qt Toolkit.
**
** $QT_BEGIN_LICENSE:LGPL21$
** Commercial License Usage
** Licensees holding valid commercial Qt licenses may use this file in
** accordance with the commercial license agreement provided with the
** Software or, alternatively, in accordance with the terms contained in
** a written agreement between you and Digia. For licensing terms and
** conditions see http://qt.digia.com/licensing. For further information
** use the contact form at http://qt.digia.com/contact-us.
**
** GNU Lesser General Public License Usage
** Alternatively, this file may be used under the terms of the GNU Lesser
** General Public License version 2.1 or version 3 as published by the Free
** Software Foundation and appearing in the file LICENSE.LGPLv21 and
** LICENSE.LGPLv3 included in the packaging of this file. Please review the
** following information to ensure the GNU Lesser General Public License
** requirements will be met: https://www.gnu.org/licenses/lgpl.html and
** http://www.gnu.org/licenses/old-licenses/lgpl-2.1.html.
**
** In addition, as a special exception, Digia gives you certain additional
** rights. These rights are described in the Digia Qt LGPL Exception
** version 1.1, included in the file LGPL_EXCEPTION.txt in this package.
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
