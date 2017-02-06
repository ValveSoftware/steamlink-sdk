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

#include "qbuiltintypes_p.h"
#include "qitem_p.h"

#include "qdate_p.h"

QT_BEGIN_NAMESPACE

using namespace QPatternist;

Date::Date(const QDateTime &dateTime) : AbstractDateTime(dateTime)
{
}

Date::Ptr Date::fromLexical(const QString &lexical)
{
    static const CaptureTable captureTable( // STATIC DATA
        /* The extra paranthesis is a build fix for GCC 3.3. */
        (QRegExp(QLatin1String(
                "^\\s*"                             /* Any preceding whitespace. */
                "(-?)"                              /* Any preceding minus. */
                "(\\d{4,})"                         /* The year part. */
                "-"                                 /* Delimiter. */
                "(\\d{2})"                          /* The month part. */
                "-"                                 /* Delimiter. */
                "(\\d{2})"                          /* The day part. */
                "(?:(\\+|-)(\\d{2}):(\\d{2})|(Z))?" /* The zone offset, "+08:24". */
                "\\s*$"                             /* Any terminating whitespace. */))),
        /*zoneOffsetSignP*/         5,
        /*zoneOffsetHourP*/         6,
        /*zoneOffsetMinuteP*/       7,
        /*zoneOffsetUTCSymbolP*/    8,
        /*yearP*/                   2,
        /*monthP*/                  3,
        /*dayP*/                    4,
        /*hourP*/                   -1,
        /*minutesP*/                -1,
        /*secondsP*/                -1,
        /*msecondsP*/               -1,
        /*yearSign*/                1);

    AtomicValue::Ptr err;
    const QDateTime retval(create(err, lexical, captureTable));

    return err ? err : Date::Ptr(new Date(retval));
}

Date::Ptr Date::fromDateTime(const QDateTime &date)
{
    /* Don't include the QTime; "reset" the time. */
    QDateTime result;
    copyTimeSpec(date, result);
    result.setDate(date.date());
    Q_ASSERT(date.isValid());

    return Date::Ptr(new Date(result));
}

Item Date::fromValue(const QDateTime &dt) const
{
    Q_ASSERT(dt.isValid());
    return fromDateTime(dt);
}

QString Date::stringValue() const
{
    return dateToString() + zoneOffsetToString();
}

ItemType::Ptr Date::type() const
{
    return BuiltinTypes::xsDate;
}

QT_END_NAMESPACE
