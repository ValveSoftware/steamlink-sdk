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

#include "qxsdcomplextype_p.h"

QT_BEGIN_NAMESPACE

using namespace QPatternist;

void XsdComplexType::OpenContent::setMode(Mode mode)
{
    m_mode = mode;
}

XsdComplexType::OpenContent::Mode XsdComplexType::OpenContent::mode() const
{
    return m_mode;
}

void XsdComplexType::OpenContent::setWildcard(const XsdWildcard::Ptr &wildcard)
{
    m_wildcard = wildcard;
}

XsdWildcard::Ptr XsdComplexType::OpenContent::wildcard() const
{
    return m_wildcard;
}

void XsdComplexType::ContentType::setVariety(Variety variety)
{
    m_variety = variety;
}

XsdComplexType::ContentType::Variety XsdComplexType::ContentType::variety() const
{
    return m_variety;
}

void XsdComplexType::ContentType::setParticle(const XsdParticle::Ptr &particle)
{
    m_particle = particle;
}

XsdParticle::Ptr XsdComplexType::ContentType::particle() const
{
    return m_particle;
}

void XsdComplexType::ContentType::setOpenContent(const OpenContent::Ptr &content)
{
    m_openContent = content;
}

XsdComplexType::OpenContent::Ptr XsdComplexType::ContentType::openContent() const
{
    return m_openContent;
}

void XsdComplexType::ContentType::setSimpleType(const AnySimpleType::Ptr &type)
{
    m_simpleType = type;
}

AnySimpleType::Ptr XsdComplexType::ContentType::simpleType() const
{
    return m_simpleType;
}


XsdComplexType::XsdComplexType()
    : m_isAbstract(false)
    , m_contentType(new ContentType())
{
    m_contentType->setVariety(ContentType::Empty);
}

void XsdComplexType::setIsAbstract(bool abstract)
{
    m_isAbstract = abstract;
}

bool XsdComplexType::isAbstract() const
{
    return m_isAbstract;
}

QString XsdComplexType::displayName(const NamePool::Ptr &np) const
{
    return np->displayName(name(np));
}

void XsdComplexType::setWxsSuperType(const SchemaType::Ptr &type)
{
    m_superType = type;
}

SchemaType::Ptr XsdComplexType::wxsSuperType() const
{
    return m_superType;
}

void XsdComplexType::setContext(const NamedSchemaComponent::Ptr &component)
{
    m_context = component;
}

NamedSchemaComponent::Ptr XsdComplexType::context() const
{
    return m_context;
}

void XsdComplexType::setContentType(const ContentType::Ptr &type)
{
    m_contentType = type;
}

XsdComplexType::ContentType::Ptr XsdComplexType::contentType() const
{
    return m_contentType;
}

void XsdComplexType::setAttributeUses(const XsdAttributeUse::List &attributeUses)
{
    m_attributeUses = attributeUses;
}

void XsdComplexType::addAttributeUse(const XsdAttributeUse::Ptr &attributeUse)
{
    m_attributeUses.append(attributeUse);
}

XsdAttributeUse::List XsdComplexType::attributeUses() const
{
    return m_attributeUses;
}

void XsdComplexType::setAttributeWildcard(const XsdWildcard::Ptr &wildcard)
{
    m_attributeWildcard = wildcard;
}

XsdWildcard::Ptr XsdComplexType::attributeWildcard() const
{
    return m_attributeWildcard;
}

XsdComplexType::TypeCategory XsdComplexType::category() const
{
    return ComplexType;
}

void XsdComplexType::setDerivationMethod(DerivationMethod method)
{
    m_derivationMethod = method;
}

XsdComplexType::DerivationMethod XsdComplexType::derivationMethod() const
{
    return m_derivationMethod;
}

void XsdComplexType::setProhibitedSubstitutions(const BlockingConstraints &substitutions)
{
    m_prohibitedSubstitutions = substitutions;
}

XsdComplexType::BlockingConstraints XsdComplexType::prohibitedSubstitutions() const
{
    return m_prohibitedSubstitutions;
}

void XsdComplexType::setAssertions(const XsdAssertion::List &assertions)
{
    m_assertions = assertions;
}

void XsdComplexType::addAssertion(const XsdAssertion::Ptr &assertion)
{
    m_assertions.append(assertion);
}

XsdAssertion::List XsdComplexType::assertions() const
{
    return m_assertions;
}

bool XsdComplexType::isDefinedBySchema() const
{
    return true;
}

QT_END_NAMESPACE
