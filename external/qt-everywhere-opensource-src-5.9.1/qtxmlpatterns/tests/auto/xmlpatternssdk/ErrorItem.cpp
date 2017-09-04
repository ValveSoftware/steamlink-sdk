/****************************************************************************
**
** Copyright (C) 2016 The Qt Company Ltd.
** Contact: https://www.qt.io/licensing/
**
** This file is part of the test suite of the Qt Toolkit.
**
** $QT_BEGIN_LICENSE:GPL-EXCEPT$
** Commercial License Usage
** Licensees holding valid commercial Qt licenses may use this file in
** accordance with the commercial license agreement provided with the
** Software or, alternatively, in accordance with the terms contained in
** a written agreement between you and The Qt Company. For licensing terms
** and conditions see https://www.qt.io/terms-conditions. For further
** information use the contact form at https://www.qt.io/contact-us.
**
** GNU General Public License Usage
** Alternatively, this file may be used under the terms of the GNU
** General Public License version 3 as published by the Free Software
** Foundation with exceptions as appearing in the file LICENSE.GPL3-EXCEPT
** included in the packaging of this file. Please review the following
** information to ensure the GNU General Public License requirements will
** be met: https://www.gnu.org/licenses/gpl-3.0.html.
**
** $QT_END_LICENSE$
**
****************************************************************************/

#include <QList>
#include <QPointer>
#include <QVariant>

#include <private/qreportcontext_p.h>
#include <private/qcommonnamespaces_p.h>

#include "ErrorItem.h"

using namespace QPatternistSDK;

QString ErrorItem::toString(const QtMsgType type)
{
    switch(type)
    {
        case QtWarningMsg:
            return QLatin1String("Warning");
        case QtFatalMsg:
            return QLatin1String("Error");
        default:
        {
            Q_ASSERT(false);
            return QString();
        }
    }
}

ErrorItem::ErrorItem(const ErrorHandler::Message &error,
                     ErrorItem *p) : m_message(error),
                                     m_parent(p)
{
}

ErrorItem::~ErrorItem()
{
    qDeleteAll(m_children);
}

int ErrorItem::columnCount() const
{
    return 3;
}

QVariant ErrorItem::data(const Qt::ItemDataRole role, int column) const
{
    if(role != Qt::DisplayRole)
        return QVariant();

    switch(column)
    {
        case 0:
            return toString(m_message.type());
        case 1:
        {
            if(!m_message.type()) /* It's a warning, likely. */
                return QString();

            QString ns;
            const QString code(QPatternist::ReportContext::codeFromURI(m_message.identifier().toString(), ns));

            if(ns == QPatternist::CommonNamespaces::XPERR)
                return code;
            else /* Do the full version. */
                return m_message.type();
        }
        case 2:
            return m_message.description();
        default:
        {
            Q_ASSERT(false);
            return QVariant();
        }
    }
}

TreeItem::List ErrorItem::children() const
{
    return m_children;
}

void ErrorItem::appendChild(TreeItem *item)
{
    m_children.append(item);
}

TreeItem *ErrorItem::child(const unsigned int rowP) const
{
    return m_children.value(rowP);
}

unsigned int ErrorItem::childCount() const
{
    return m_children.count();
}

TreeItem *ErrorItem::parent() const
{
    return m_parent;
}

// vim: et:ts=4:sw=4:sts=4
