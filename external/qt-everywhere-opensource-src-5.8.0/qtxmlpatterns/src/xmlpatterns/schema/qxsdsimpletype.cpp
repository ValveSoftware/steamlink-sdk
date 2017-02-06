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

//
//  W A R N I N G
//  -------------
//
// This file is not part of the Qt API.  It exists purely as an
// implementation detail.  This header file may change from version to
// version without notice, or even be removed.
//
// We mean it.

#include "qxsdsimpletype_p.h"

QT_BEGIN_NAMESPACE

using namespace QPatternist;

QString XsdSimpleType::displayName(const NamePool::Ptr &np) const
{
    return np->displayName(name(np));
}

void XsdSimpleType::setWxsSuperType(const SchemaType::Ptr &type)
{
    m_superType = type;
}

SchemaType::Ptr XsdSimpleType::wxsSuperType() const
{
    return m_superType;
}

void XsdSimpleType::setContext(const NamedSchemaComponent::Ptr &component)
{
    m_context = component;
}

NamedSchemaComponent::Ptr XsdSimpleType::context() const
{
    return m_context;
}

void XsdSimpleType::setPrimitiveType(const AnySimpleType::Ptr &type)
{
    m_primitiveType = type;
}

AnySimpleType::Ptr XsdSimpleType::primitiveType() const
{
    return m_primitiveType;
}

void XsdSimpleType::setItemType(const AnySimpleType::Ptr &type)
{
    m_itemType = type;
}

AnySimpleType::Ptr XsdSimpleType::itemType() const
{
    return m_itemType;
}

void XsdSimpleType::setMemberTypes(const AnySimpleType::List &types)
{
    m_memberTypes = types;
}

AnySimpleType::List XsdSimpleType::memberTypes() const
{
    return m_memberTypes;
}

void XsdSimpleType::setFacets(const XsdFacet::Hash &facets)
{
    m_facets = facets;
}

XsdFacet::Hash XsdSimpleType::facets() const
{
    return m_facets;
}

void XsdSimpleType::setCategory(TypeCategory category)
{
    m_typeCategory = category;
}

XsdSimpleType::TypeCategory XsdSimpleType::category() const
{
    return m_typeCategory;
}

void XsdSimpleType::setDerivationMethod(DerivationMethod method)
{
    m_derivationMethod = method;
}

XsdSimpleType::DerivationMethod XsdSimpleType::derivationMethod() const
{
    return m_derivationMethod;
}

bool XsdSimpleType::isDefinedBySchema() const
{
    return true;
}

QT_END_NAMESPACE
