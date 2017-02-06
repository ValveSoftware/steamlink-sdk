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

#include "qatomictype_p.h"

#include "qanytype_p.h"

QT_BEGIN_NAMESPACE

using namespace QPatternist;

AnyType::~AnyType()
{
}

bool AnyType::wxsTypeMatches(const SchemaType::Ptr &other) const
{
    if(other)
        return this == other.data() ? true : wxsTypeMatches(other->wxsSuperType());
    else
        return false;
}

bool AnyType::isAbstract() const
{
    return false;
}

QXmlName AnyType::name(const NamePool::Ptr &np) const
{
    return np->allocateQName(StandardNamespaces::xs, QLatin1String("anyType"));
}

QString AnyType::displayName(const NamePool::Ptr &) const
{
    /* A bit faster than calling name()->displayName() */
    return QLatin1String("xs:anyType");
}

SchemaType::Ptr AnyType::wxsSuperType() const
{
    return SchemaType::Ptr();
}

SchemaType::TypeCategory AnyType::category() const
{
    return None;
}

bool AnyType::isComplexType() const
{
    return true;
}

SchemaType::DerivationMethod AnyType::derivationMethod() const
{
    return NoDerivation;
}

SchemaType::DerivationConstraints AnyType::derivationConstraints() const
{
    return SchemaType::DerivationConstraints();
}

QT_END_NAMESPACE
