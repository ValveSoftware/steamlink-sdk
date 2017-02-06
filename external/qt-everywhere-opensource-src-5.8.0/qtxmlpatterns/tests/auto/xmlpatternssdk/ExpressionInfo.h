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

#ifndef PatternistSDK_ExpressionInfo_H
#define PatternistSDK_ExpressionInfo_H

#include "Global.h"
#include <private/qexpressiondispatch_p.h>

#include <QPair>
#include <QString>

QT_BEGIN_NAMESPACE

namespace QPatternistSDK
{
    /**
     * @short Houses debug information about an QPatternist::Expression instance.
     *
     * An Expression's name, typically its class name, can be retrieved
     * via the member variable first, and additional data, for example its string
     * value or operator, can be retrieved via the member variable second.
     *
     * @ingroup PatternistSDK
     * @author Frans Englich <frans.englich@nokia.com>
     */
    class Q_PATTERNISTSDK_EXPORT ExpressionInfo : public QPatternist::ExpressionVisitorResult
    {
    public:
        ExpressionInfo(const QString &name, const QString &details);

        QString name() const { return m_name; }
        QString details() const { return m_details; }

    private:
        QString m_name;
        QString m_details;
    };
}

QT_END_NAMESPACE

#endif
// vim: et:ts=4:sw=4:sts=4
