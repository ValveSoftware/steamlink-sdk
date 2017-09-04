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

#include "qxsdelement_p.h"

QT_BEGIN_NAMESPACE

using namespace QPatternist;

void XsdElement::Scope::setVariety(Variety variety)
{
    m_variety = variety;
}

XsdElement::Scope::Variety XsdElement::Scope::variety() const
{
    return m_variety;
}

void XsdElement::Scope::setParent(const NamedSchemaComponent::Ptr &parent)
{
    m_parent = parent;
}

NamedSchemaComponent::Ptr XsdElement::Scope::parent() const
{
    return m_parent;
}

void XsdElement::ValueConstraint::setVariety(Variety variety)
{
    m_variety = variety;
}

XsdElement::ValueConstraint::Variety XsdElement::ValueConstraint::variety() const
{
    return m_variety;
}

void XsdElement::ValueConstraint::setValue(const QString &value)
{
    m_value = value;
}

QString XsdElement::ValueConstraint::value() const
{
    return m_value;
}

void XsdElement::ValueConstraint::setLexicalForm(const QString &form)
{
    m_lexicalForm = form;
}

QString XsdElement::ValueConstraint::lexicalForm() const
{
    return m_lexicalForm;
}

void XsdElement::TypeTable::addAlternative(const XsdAlternative::Ptr &alternative)
{
    m_alternatives.append(alternative);
}

XsdAlternative::List XsdElement::TypeTable::alternatives() const
{
    return m_alternatives;
}

void XsdElement::TypeTable::setDefaultTypeDefinition(const XsdAlternative::Ptr &type)
{
    m_defaultTypeDefinition = type;
}

XsdAlternative::Ptr XsdElement::TypeTable::defaultTypeDefinition() const
{
    return m_defaultTypeDefinition;
}


XsdElement::XsdElement()
    : m_isAbstract(false), m_isNillable(false)
{
}

bool XsdElement::isElement() const
{
    return true;
}

void XsdElement::setType(const SchemaType::Ptr &type)
{
    m_type = type;
}

SchemaType::Ptr XsdElement::type() const
{
    return m_type;
}

void XsdElement::setScope(const Scope::Ptr &scope)
{
    m_scope = scope;
}

XsdElement::Scope::Ptr XsdElement::scope() const
{
    return m_scope;
}

void XsdElement::setValueConstraint(const ValueConstraint::Ptr &constraint)
{
    m_valueConstraint = constraint;
}

XsdElement::ValueConstraint::Ptr XsdElement::valueConstraint() const
{
    return m_valueConstraint;
}

void XsdElement::setTypeTable(const TypeTable::Ptr &table)
{
    m_typeTable = table;
}

XsdElement::TypeTable::Ptr XsdElement::typeTable() const
{
    return m_typeTable;
}

void XsdElement::setIsAbstract(bool abstract)
{
    m_isAbstract = abstract;
}

bool XsdElement::isAbstract() const
{
    return m_isAbstract;
}

void XsdElement::setIsNillable(bool nillable)
{
    m_isNillable = nillable;
}

bool XsdElement::isNillable() const
{
    return m_isNillable;
}

void XsdElement::setDisallowedSubstitutions(const BlockingConstraints &substitutions)
{
    m_disallowedSubstitutions = substitutions;
}

XsdElement::BlockingConstraints XsdElement::disallowedSubstitutions() const
{
    return m_disallowedSubstitutions;
}

void XsdElement::setSubstitutionGroupExclusions(const SchemaType::DerivationConstraints &exclusions)
{
    m_substitutionGroupExclusions = exclusions;
}

SchemaType::DerivationConstraints XsdElement::substitutionGroupExclusions() const
{
    return m_substitutionGroupExclusions;
}

void XsdElement::setIdentityConstraints(const XsdIdentityConstraint::List &constraints)
{
    m_identityConstraints = constraints;
}

void XsdElement::addIdentityConstraint(const XsdIdentityConstraint::Ptr &constraint)
{
    m_identityConstraints.append(constraint);
}

XsdIdentityConstraint::List XsdElement::identityConstraints() const
{
    return m_identityConstraints;
}

void XsdElement::setSubstitutionGroupAffiliations(const XsdElement::List &affiliations)
{
    m_substitutionGroupAffiliations = affiliations;
}

XsdElement::List XsdElement::substitutionGroupAffiliations() const
{
    return m_substitutionGroupAffiliations;
}

void XsdElement::addSubstitutionGroup(const XsdElement::Ptr &element)
{
    m_substitutionGroups.insert(element);
}

XsdElement::List XsdElement::substitutionGroups() const
{
    return m_substitutionGroups.toList();
}

QT_END_NAMESPACE
