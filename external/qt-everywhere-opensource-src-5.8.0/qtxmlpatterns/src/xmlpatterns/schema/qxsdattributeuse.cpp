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

#include "qxsdattributeuse_p.h"

QT_BEGIN_NAMESPACE

using namespace QPatternist;

void XsdAttributeUse::ValueConstraint::setVariety(Variety variety)
{
    m_variety = variety;
}

XsdAttributeUse::ValueConstraint::Variety XsdAttributeUse::ValueConstraint::variety() const
{
    return m_variety;
}

void XsdAttributeUse::ValueConstraint::setValue(const QString &value)
{
    m_value = value;
}

QString XsdAttributeUse::ValueConstraint::value() const
{
    return m_value;
}

void XsdAttributeUse::ValueConstraint::setLexicalForm(const QString &form)
{
    m_lexicalForm = form;
}

QString XsdAttributeUse::ValueConstraint::lexicalForm() const
{
    return m_lexicalForm;
}

XsdAttributeUse::ValueConstraint::Ptr XsdAttributeUse::ValueConstraint::fromAttributeValueConstraint(const XsdAttribute::ValueConstraint::Ptr &constraint)
{
    XsdAttributeUse::ValueConstraint::Ptr newConstraint(new XsdAttributeUse::ValueConstraint());
    switch (constraint->variety()) {
        case XsdAttribute::ValueConstraint::Fixed: newConstraint->setVariety(Fixed); break;
        case XsdAttribute::ValueConstraint::Default: newConstraint->setVariety(Default); break;
    }
    newConstraint->setValue(constraint->value());
    newConstraint->setLexicalForm(constraint->lexicalForm());

    return newConstraint;
}

XsdAttributeUse::XsdAttributeUse()
    : m_useType(OptionalUse)
{
}

bool XsdAttributeUse::isAttributeUse() const
{
    return true;
}

void XsdAttributeUse::setUseType(UseType type)
{
    m_useType = type;
}

XsdAttributeUse::UseType XsdAttributeUse::useType() const
{
    return m_useType;
}

bool XsdAttributeUse::isRequired() const
{
    return (m_useType == RequiredUse);
}

void XsdAttributeUse::setAttribute(const XsdAttribute::Ptr &attribute)
{
    m_attribute = attribute;
}

XsdAttribute::Ptr XsdAttributeUse::attribute() const
{
    return m_attribute;
}

void XsdAttributeUse::setValueConstraint(const ValueConstraint::Ptr &constraint)
{
    m_valueConstraint = constraint;
}

XsdAttributeUse::ValueConstraint::Ptr XsdAttributeUse::valueConstraint() const
{
    return m_valueConstraint;
}

QT_END_NAMESPACE
