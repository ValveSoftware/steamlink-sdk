/****************************************************************************
**
** Copyright (C) 2014 Digia Plc and/or its subsidiary(-ies).
** Contact: http://www.qt-project.org/legal
**
** This file is part of the Qt Quick Controls module of the Qt Toolkit.
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

#include "qquickrangeddate_p.h"

QT_BEGIN_NAMESPACE

// JavaScript Date > QDate conversion is not correct for large negative dates.
Q_GLOBAL_STATIC_WITH_ARGS(const QDate, jsMinimumDate, (QDate(1, 1, 1)))
Q_GLOBAL_STATIC_WITH_ARGS(const QDate, jsMaximumDate, (QDate(275759, 10, 25)))

QQuickRangedDate::QQuickRangedDate() :
    QObject(0),
    mDate(QDate::currentDate()),
    mMinimumDate(*jsMinimumDate),
    mMaximumDate(*jsMaximumDate)
{
}

/*! \internal
    \qmlproperty date QQuickRangedDate::date
*/
void QQuickRangedDate::setDate(const QDate &date)
{
    if (date == mDate)
        return;

    if (date < mMinimumDate) {
        mDate = mMinimumDate;
    } else if (date > mMaximumDate) {
        mDate = mMaximumDate;
    } else {
        mDate = date;
    }

    emit dateChanged();
}

/*! \internal
    \qmlproperty date QQuickRangedDate::minimumDate
*/
void QQuickRangedDate::setMinimumDate(const QDate &minimumDate)
{
    if (minimumDate == mMinimumDate)
        return;

    mMinimumDate = qMax(minimumDate, *jsMinimumDate);
    emit minimumDateChanged();

    // If the new minimumDate makes date invalid, clamp date to it.
    if (mDate < mMinimumDate) {
        mDate = mMinimumDate;
        emit dateChanged();
    }
}

/*! \internal
    \qmlproperty date QQuickRangedDate::maximumDate
*/
void QQuickRangedDate::setMaximumDate(const QDate &maximumDate)
{
    if (maximumDate == mMaximumDate)
        return;

    // If the new maximumDate is smaller than minimumDate, clamp maximumDate to it.
    // If the new maximumDate is larger than jsMaximumDate, also clamp it.
    mMaximumDate = maximumDate < mMinimumDate ? mMinimumDate : qMin(maximumDate, *jsMaximumDate);
    emit maximumDateChanged();

    // If the new maximumDate makes the date invalid, clamp it.
    if (mDate > mMaximumDate) {
        mDate = mMaximumDate;
        emit dateChanged();
    }
}

QT_END_NAMESPACE
