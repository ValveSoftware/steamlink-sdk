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

#include "qxsdschematypesfactory_p.h"

#include "qbasictypesfactory_p.h"
#include "qbuiltintypes_p.h"
#include "qderivedinteger_p.h"
#include "qderivedstring_p.h"
#include "qcommonnamespaces_p.h"
#include "qxsdschematoken_p.h"
#include "qxsdsimpletype_p.h"

QT_BEGIN_NAMESPACE

using namespace QPatternist;

XsdSchemaTypesFactory::XsdSchemaTypesFactory(const NamePool::Ptr &namePool)
    : m_namePool(namePool)
{
    m_types.reserve(3);

    const XsdFacet::Ptr whiteSpaceFacet(new XsdFacet());
    whiteSpaceFacet->setType(XsdFacet::WhiteSpace);
    whiteSpaceFacet->setFixed(true);
    whiteSpaceFacet->setValue(DerivedString<TypeString>::fromLexical(m_namePool, XsdSchemaToken::toString(XsdSchemaToken::Collapse)));

    const XsdFacet::Ptr minLengthFacet(new XsdFacet());
    minLengthFacet->setType(XsdFacet::MinimumLength);
    minLengthFacet->setValue(DerivedInteger<TypeNonNegativeInteger>::fromLexical(namePool, QLatin1String("1")));

    XsdFacet::Hash facets;
    facets.insert(whiteSpaceFacet->type(), whiteSpaceFacet);
    facets.insert(minLengthFacet->type(), minLengthFacet);

    {
        const QXmlName typeName = m_namePool->allocateQName(CommonNamespaces::WXS, QLatin1String("NMTOKENS"));
        const XsdSimpleType::Ptr type(new XsdSimpleType());
        type->setName(typeName);
        type->setWxsSuperType(BuiltinTypes::xsAnySimpleType);
        type->setCategory(XsdSimpleType::SimpleTypeList);
        type->setItemType(BuiltinTypes::xsNMTOKEN);
        type->setDerivationMethod(XsdSimpleType::DerivationRestriction);
        type->setFacets(facets);
        m_types.insert(typeName, type);
    }
    {
        const QXmlName typeName = m_namePool->allocateQName(CommonNamespaces::WXS, QLatin1String("IDREFS"));
        const XsdSimpleType::Ptr type(new XsdSimpleType());
        type->setName(typeName);
        type->setWxsSuperType(BuiltinTypes::xsAnySimpleType);
        type->setCategory(XsdSimpleType::SimpleTypeList);
        type->setItemType(BuiltinTypes::xsIDREF);
        type->setDerivationMethod(XsdSimpleType::DerivationRestriction);
        type->setFacets(facets);
        m_types.insert(typeName, type);
    }
    {
        const QXmlName typeName = m_namePool->allocateQName(CommonNamespaces::WXS, QLatin1String("ENTITIES"));
        const XsdSimpleType::Ptr type(new XsdSimpleType());
        type->setName(typeName);
        type->setWxsSuperType(BuiltinTypes::xsAnySimpleType);
        type->setCategory(XsdSimpleType::SimpleTypeList);
        type->setItemType(BuiltinTypes::xsENTITY);
        type->setDerivationMethod(XsdSimpleType::DerivationRestriction);
        type->setFacets(facets);
        m_types.insert(typeName, type);
    }
}

SchemaType::Ptr XsdSchemaTypesFactory::createSchemaType(const QXmlName name) const
{
    if (m_types.contains(name)) {
        return m_types.value(name);
    } else {
        if (!m_basicTypesFactory)
            m_basicTypesFactory = BasicTypesFactory::self(m_namePool);

        return m_basicTypesFactory->createSchemaType(name);
    }
}

SchemaType::Hash XsdSchemaTypesFactory::types() const
{
    return m_types;
}

QT_END_NAMESPACE
