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

#include "qatomiccomparator_p.h"
#include "qcommonvalues_p.h"
#include "qschemadatetime_p.h"
#include "qdaytimeduration_p.h"
#include "qdecimal_p.h"
#include "qinteger_p.h"
#include "qpatternistlocale_p.h"

#include "qdatetimefn_p.h"

QT_BEGIN_NAMESPACE

using namespace QPatternist;

Item DateTimeFN::evaluateSingleton(const DynamicContext::Ptr &context) const
{
    const Item di(m_operands.first()->evaluateSingleton(context));
    if(!di)
        return Item();

    const Item ti(m_operands.last()->evaluateSingleton(context));
    if(!ti)
        return Item();

    QDateTime date(di.as<AbstractDateTime>()->toDateTime());
    Q_ASSERT(date.isValid());
    QDateTime time(ti.as<AbstractDateTime>()->toDateTime());
    Q_ASSERT(time.isValid());

    if(date.timeSpec() == time.timeSpec() || /* Identical timezone properties. */
       time.timeSpec() == Qt::LocalTime) /* time has no timezone, but date do. */
    {
        date.setTime(time.time());
        Q_ASSERT(date.isValid());
        return DateTime::fromDateTime(date);
    }
    else if(date.timeSpec() == Qt::LocalTime) /* date has no timezone, but time do. */
    {
        time.setDate(date.date());
        Q_ASSERT(time.isValid());
        return DateTime::fromDateTime(time);
    }
    else
    {
        context->error(QtXmlPatterns::tr("If both values have zone offsets, "
                                         "they must have the same zone offset. "
                                         "%1 and %2 are not the same.")
                       .arg(formatData(di.stringValue()),
                            formatData(di.stringValue())),
                       ReportContext::FORG0008, this);
        return Item(); /* Silence GCC warning. */
    }
}

QT_END_NAMESPACE
