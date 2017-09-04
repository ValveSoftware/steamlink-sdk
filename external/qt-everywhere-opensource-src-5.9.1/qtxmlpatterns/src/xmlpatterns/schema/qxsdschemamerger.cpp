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

#include "qxsdschemamerger_p.h"

QT_BEGIN_NAMESPACE

using namespace QPatternist;

XsdSchemaMerger::XsdSchemaMerger(const XsdSchema::Ptr &schema, const XsdSchema::Ptr &otherSchema)
{
    merge(schema, otherSchema);
}

XsdSchema::Ptr XsdSchemaMerger::mergedSchema() const
{
    return m_mergedSchema;
}

void XsdSchemaMerger::merge(const XsdSchema::Ptr &schema, const XsdSchema::Ptr &otherSchema)
{
    m_mergedSchema = XsdSchema::Ptr(new XsdSchema(otherSchema->namePool()));

    // first fill the merged schema with the values from schema
    if (schema) {
        const XsdElement::List elements = schema->elements();
        for (int i = 0; i < elements.count(); ++i) {
            m_mergedSchema->addElement(elements.at(i));
        }

        const XsdAttribute::List attributes = schema->attributes();
        for (int i = 0; i < attributes.count(); ++i) {
            m_mergedSchema->addAttribute(attributes.at(i));
        }

        const SchemaType::List types = schema->types();
        for (int i = 0; i < types.count(); ++i) {
            m_mergedSchema->addType(types.at(i));
        }

        const SchemaType::List anonymousTypes = schema->anonymousTypes();
        for (int i = 0; i < anonymousTypes.count(); ++i) {
            m_mergedSchema->addAnonymousType(anonymousTypes.at(i));
        }

        const XsdModelGroup::List elementGroups = schema->elementGroups();
        for (int i = 0; i < elementGroups.count(); ++i) {
            m_mergedSchema->addElementGroup(elementGroups.at(i));
        }

        const XsdAttributeGroup::List attributeGroups = schema->attributeGroups();
        for (int i = 0; i < attributeGroups.count(); ++i) {
            m_mergedSchema->addAttributeGroup(attributeGroups.at(i));
        }

        const XsdNotation::List notations = schema->notations();
        for (int i = 0; i < notations.count(); ++i) {
            m_mergedSchema->addNotation(notations.at(i));
        }

        const XsdIdentityConstraint::List identityConstraints = schema->identityConstraints();
        for (int i = 0; i < identityConstraints.count(); ++i) {
            m_mergedSchema->addIdentityConstraint(identityConstraints.at(i));
        }
    }

    // then merge in the values from the otherSchema
    {
        const XsdElement::List elements = otherSchema->elements();
        for (int i = 0; i < elements.count(); ++i) {
            if (!m_mergedSchema->element(elements.at(i)->name(otherSchema->namePool())))
                m_mergedSchema->addElement(elements.at(i));
        }

        const XsdAttribute::List attributes = otherSchema->attributes();
        for (int i = 0; i < attributes.count(); ++i) {
            if (!m_mergedSchema->attribute(attributes.at(i)->name(otherSchema->namePool())))
                m_mergedSchema->addAttribute(attributes.at(i));
        }

        const SchemaType::List types = otherSchema->types();
        for (int i = 0; i < types.count(); ++i) {
            if (!m_mergedSchema->type(types.at(i)->name(otherSchema->namePool())))
                m_mergedSchema->addType(types.at(i));
        }

        const SchemaType::List anonymousTypes = otherSchema->anonymousTypes();
        for (int i = 0; i < anonymousTypes.count(); ++i) {
            // add anonymous type as they are
            m_mergedSchema->addAnonymousType(anonymousTypes.at(i));
        }

        const XsdModelGroup::List elementGroups = otherSchema->elementGroups();
        for (int i = 0; i < elementGroups.count(); ++i) {
            if (!m_mergedSchema->elementGroup(elementGroups.at(i)->name(otherSchema->namePool())))
                m_mergedSchema->addElementGroup(elementGroups.at(i));
        }

        const XsdAttributeGroup::List attributeGroups = otherSchema->attributeGroups();
        for (int i = 0; i < attributeGroups.count(); ++i) {
            if (!m_mergedSchema->attributeGroup(attributeGroups.at(i)->name(otherSchema->namePool())))
                m_mergedSchema->addAttributeGroup(attributeGroups.at(i));
        }

        const XsdNotation::List notations = otherSchema->notations();
        for (int i = 0; i < notations.count(); ++i) {
            if (!m_mergedSchema->notation(notations.at(i)->name(otherSchema->namePool())))
                m_mergedSchema->addNotation(notations.at(i));
        }

        const XsdIdentityConstraint::List identityConstraints = otherSchema->identityConstraints();
        for (int i = 0; i < identityConstraints.count(); ++i) {
            if (!m_mergedSchema->identityConstraint(identityConstraints.at(i)->name(otherSchema->namePool())))
                m_mergedSchema->addIdentityConstraint(identityConstraints.at(i));
        }
    }
}

QT_END_NAMESPACE
