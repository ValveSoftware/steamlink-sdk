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

#include "qxsdidchelper_p.h"

#include "qderivedstring_p.h"
#include "qxsdschemahelper_p.h"

QT_BEGIN_NAMESPACE

using namespace QPatternist;

FieldNode::FieldNode()
{
}

FieldNode::FieldNode(const QXmlItem &item, const QString &data, const SchemaType::Ptr &type)
    : m_item(item)
    , m_data(data)
    , m_type(type)
{
}

bool FieldNode::isEmpty() const
{
    return m_item.isNull();
}

bool FieldNode::isEqualTo(const FieldNode &other, const NamePool::Ptr &namePool, const ReportContext::Ptr &context, const SourceLocationReflection *const reflection) const
{
    if (m_type != other.m_type)
        return false;

    const DerivedString<TypeString>::Ptr string = DerivedString<TypeString>::fromLexical(namePool, m_data);
    const DerivedString<TypeString>::Ptr otherString = DerivedString<TypeString>::fromLexical(namePool, other.m_data);

    return XsdSchemaHelper::constructAndCompare(string, AtomicComparator::OperatorEqual, otherString, m_type, context, reflection);
}

QXmlItem FieldNode::item() const
{
    return m_item;
}

TargetNode::TargetNode(const QXmlItem &item)
    : m_item(item)
{
}

QXmlItem TargetNode::item() const
{
    return m_item;
}

QVector<QXmlItem> TargetNode::fieldItems() const
{
    QVector<QXmlItem> items;

    for (int i = 0; i < m_fields.count(); ++i)
        items.append(m_fields.at(i).item());

    return items;
}

int TargetNode::emptyFieldsCount() const
{
    int counter = 0;
    for (int i = 0; i < m_fields.count(); ++i) {
        if (m_fields.at(i).isEmpty())
            ++counter;
    }

    return counter;
}

bool TargetNode::fieldsAreEqual(const TargetNode &other, const NamePool::Ptr &namePool, const ReportContext::Ptr &context, const SourceLocationReflection *const reflection) const
{
    if (m_fields.count() != other.m_fields.count())
        return false;

    for (int i = 0; i < m_fields.count(); ++i) {
        if (!m_fields.at(i).isEqualTo(other.m_fields.at(i), namePool, context, reflection))
            return false;
    }

    return true;
}

void TargetNode::addField(const QXmlItem &item, const QString &data, const SchemaType::Ptr &type)
{
    m_fields.append(FieldNode(item, data, type));
}

bool TargetNode::operator==(const TargetNode &other) const
{
    return (m_item.toNodeModelIndex() == other.m_item.toNodeModelIndex());
}

QT_END_NAMESPACE
