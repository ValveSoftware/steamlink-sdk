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
                                                , public QPair<QString, QString>
    {
    public:
        ExpressionInfo(const QString &name, const QString &details);
    };
}

QT_END_NAMESPACE

#endif
// vim: et:ts=4:sw=4:sts=4
