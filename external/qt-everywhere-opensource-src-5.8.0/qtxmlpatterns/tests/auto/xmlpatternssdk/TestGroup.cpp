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

#include <QColor>
#include <QPair>
#include <QVariant>

#include "TestGroup.h"

using namespace QPatternistSDK;

TestGroup::TestGroup(TreeItem *p) : m_parent(p)
{
}

QVariant TestGroup::data(const Qt::ItemDataRole role, int column) const
{
    if(role != Qt::DisplayRole && role != Qt::BackgroundRole && role != Qt::ToolTipRole)
        return QVariant();

    /* In ResultSummary, the first is the amount of passes and the second is the total. */
    const ResultSummary sum(resultSummary());
    const int failures = sum.second - sum.first;

    switch(role)
    {
        case Qt::DisplayRole:
        {

            switch(column)
            {
                case 0:
                    return title();
                case 1:
                    /* Passes. */
                    return QString::number(sum.first);
                case 2:
                    /* Failures. */
                    return QString::number(failures);
                case 3:
                    /* Total. */
                    return QString::number(sum.second);
                default:
                {
                    Q_ASSERT(false);
                    return QString();
                }
            }
        }
        case Qt::BackgroundRole:
        {
            switch(column)
            {
                case 1:
                {
                    if(sum.first)
                    {
                        /* Pass. */
                        return QColor(Qt::green);
                    }
                    else
                        return QVariant();
                }
                case 2:
                {
                    if(failures)
                    {
                        /* Failure. */
                        return QColor(Qt::red);
                    }
                    else
                        return QVariant();
                }
                default:
                    return QVariant();
            }
        }
        case Qt::ToolTipRole:
        {
            return description();
        }
        default:
        {
            Q_ASSERT_X(false, Q_FUNC_INFO, "This shouldn't be reached");
            return QVariant();
        }
    }
}

void TestGroup::setNote(const QString &n)
{
    m_note = n;
}

QString TestGroup::note() const
{
    return m_note;
}

TreeItem *TestGroup::parent() const
{
    return m_parent;
}

// vim: et:ts=4:sw=4:sts=4
