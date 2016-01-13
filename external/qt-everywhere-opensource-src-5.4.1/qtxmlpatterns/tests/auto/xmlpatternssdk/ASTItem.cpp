/****************************************************************************
**
** Copyright (C) 2014 Digia Plc and/or its subsidiary(-ies).
** Contact: http://www.qt-project.org/legal
**
** This file is part of the test suite of the Qt Toolkit.
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

#include <QList>
#include <QPointer>
#include <QVariant>

#include "ASTItem.h"

using namespace QPatternistSDK;

/**
 * This is what the AST rendering is indented with.
 */
static const QLatin1String astIndent("  ");
// STATIC DATA

ASTItem::ASTItem(ASTItem *p,
                 const QString &name,
                 const QString &details,
                 const QString &staticType,
                 const QString &reqType) : m_name(name),
                                              m_details(details),
                                              m_reqType(reqType),
                                              m_staticType(staticType),
                                              m_parent(p)
{
}

ASTItem::~ASTItem()
{
    qDeleteAll(m_children);
}

QString ASTItem::toString() const
{
    /* The first ASTItem* is a virtual root node, so skip "this". */
    Q_ASSERT(m_children.count() == 1);
    TreeItem *treeChild = m_children.first();
    Q_ASSERT(treeChild);

    ASTItem *astChild = static_cast<ASTItem *>(treeChild);

    return astChild->toString(QString());
}

QString ASTItem::toString(const QString &indent) const
{
    QString retval;

    retval += indent;
    retval += m_name;
    retval += QLatin1Char('(');
    retval += m_details;
    retval += QLatin1String(")\n");

    const TreeItem::List::const_iterator end(m_children.constEnd());

    for(TreeItem::List::const_iterator it(m_children.constBegin()); it != end; ++it)
    {
        TreeItem *treeChild = *it; /* Cast away the QPointer with its casting operator. */
        ASTItem *astChild = static_cast<ASTItem *>(treeChild);

        retval += astChild->toString(indent + astIndent);
    }

    return retval;
}

QVariant ASTItem::data(const Qt::ItemDataRole role, int column) const
{
    if(role != Qt::DisplayRole)
        return QVariant();

    switch(column)
    {
        case 0:
            return m_name;
        case 1:
            return m_details;
        case 2:
            return m_staticType;
        case 3:
            return m_reqType;
        default:
        {
            Q_ASSERT(false);
            return QVariant();
        }
    }
}

int ASTItem::columnCount() const
{
    return 4;
}

TreeItem::List ASTItem::children() const
{
    return m_children;
}

void ASTItem::appendChild(TreeItem *item)
{
    m_children.append(item);
}

TreeItem *ASTItem::child(const unsigned int rowP) const
{
    return m_children.value(rowP);
}

unsigned int ASTItem::childCount() const
{
    return m_children.count();
}

TreeItem *ASTItem::parent() const
{
    return m_parent;
}

// vim: et:ts=4:sw=4:sts=4
