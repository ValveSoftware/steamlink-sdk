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

#include "qxsdattribute_p.h"
#include "qxsdcomplextype_p.h"

QT_BEGIN_NAMESPACE

using namespace QPatternist;


void XsdAttribute::Scope::setVariety(Variety variety)
{
    m_variety = variety;
}

XsdAttribute::Scope::Variety XsdAttribute::Scope::variety() const
{
    return m_variety;
}

void XsdAttribute::Scope::setParent(const NamedSchemaComponent::Ptr &parent)
{
    m_parent = parent;
}

NamedSchemaComponent::Ptr XsdAttribute::Scope::parent() const
{
    return m_parent;
}

void XsdAttribute::ValueConstraint::setVariety(Variety variety)
{
    m_variety = variety;
}

XsdAttribute::ValueConstraint::Variety XsdAttribute::ValueConstraint::variety() const
{
    return m_variety;
}

void XsdAttribute::ValueConstraint::setValue(const QString &value)
{
    m_value = value;
}

QString XsdAttribute::ValueConstraint::value() const
{
    return m_value;
}

void XsdAttribute::ValueConstraint::setLexicalForm(const QString &form)
{
    m_lexicalForm = form;
}

QString XsdAttribute::ValueConstraint::lexicalForm() const
{
    return m_lexicalForm;
}

void XsdAttribute::setType(const AnySimpleType::Ptr &type)
{
    m_type = type;
}

AnySimpleType::Ptr XsdAttribute::type() const
{
    return m_type;
}

void XsdAttribute::setScope(const Scope::Ptr &scope)
{
    m_scope = scope;
}

XsdAttribute::Scope::Ptr XsdAttribute::scope() const
{
    return m_scope;
}

void XsdAttribute::setValueConstraint(const ValueConstraint::Ptr &constraint)
{
    m_valueConstraint = constraint;
}

XsdAttribute::ValueConstraint::Ptr XsdAttribute::valueConstraint() const
{
    return m_valueConstraint;
}

QT_END_NAMESPACE
