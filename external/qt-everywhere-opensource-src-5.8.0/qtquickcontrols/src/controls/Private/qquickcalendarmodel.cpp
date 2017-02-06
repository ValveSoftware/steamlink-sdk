/****************************************************************************
**
** Copyright (C) 2016 The Qt Company Ltd.
** Contact: https://www.qt.io/licensing/
**
** This file is part of the Qt Quick Controls module of the Qt Toolkit.
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

#include "qquickcalendarmodel_p.h"

namespace {
    static const int daysInAWeek = 7;

    /*
        Not the number of weeks per month, but the number of weeks that are
        shown on a typical calendar.
    */
    static const int weeksOnACalendarMonth = 6;

    /*
        The amount of days to populate the calendar with.
    */
    static const int daysOnACalendarMonth = daysInAWeek * weeksOnACalendarMonth;
}

QT_BEGIN_NAMESPACE

/*!
    QQuickCalendarModel provides a model for the Calendar control.
    It is responsible for populating itself with dates based on a given month
    and year.

    The model stores a list of dates whose indices map directly to the Calendar.
    For example, the model would store the following dates when any day in
    January 2015 is selected on the Calendar:

    [30/12/2014][31/12/2014][01/01/2015]...[31/01/2015][01/02/2015]...[09/02/2015]

    The Calendar would then display the dates in the same order within a grid:

            January 2015

    [30][31][01][02][03][04][05]
    [06][07][08][09][10][11][12]
    [13][14][15][16][17][18][19]
    [20][21][22][23][24][25][26]
    [27][28][29][30][31][01][02]
    [03][04][05][06][07][08][09]
*/

QQuickCalendarModel1::QQuickCalendarModel1(QObject *parent) :
    QAbstractListModel(parent)
{
}

/*!
    The date that determines which dates are stored.

    We store all of the days in the month of visibleDate, as well as any days
    in the previous or following month if there is enough space.
*/
QDate QQuickCalendarModel1::visibleDate() const
{
    return mVisibleDate;
}

/*!
    Sets the visible date to \a visibleDate.

    If \a visibleDate is a valid date and is different than the previously
    visible date, the visible date is changed and
    populateFromVisibleDate() called.
*/
void QQuickCalendarModel1::setVisibleDate(const QDate &date)
{
    if (date != mVisibleDate && date.isValid()) {
        const QDate previousDate = mVisibleDate;
        mVisibleDate = date;
        populateFromVisibleDate(previousDate);
        emit visibleDateChanged(date);
    }
}

/*!
    The locale set on the Calendar.

    This affects which dates are stored for visibleDate(). For example, if
    the locale is en_US, the first day of the week is Sunday. Therefore, if
    visibleDate() is some day in January 2014, there will be three days
    displayed before the 1st of January:


            January 2014

    [SO][MO][TU][WE][TH][FR][SA]
    [29][30][31][01][02][03][04]
    ...

    If the locale is then changed to en_GB (with the same visibleDate()),
    there will be 2 days before the 1st of January, because Monday is the
    first day of the week for that locale:

            January 2014

    [MO][TU][WE][TH][FR][SA][SO]
    [30][31][01][02][03][04][05]
    ...
*/
QLocale QQuickCalendarModel1::locale() const
{
    return mLocale;
}

/*!
    Sets the locale to \a locale.
*/
void QQuickCalendarModel1::setLocale(const QLocale &locale)
{
    if (locale != mLocale) {
        Qt::DayOfWeek previousFirstDayOfWeek = mLocale.firstDayOfWeek();
        mLocale = locale;
        emit localeChanged(mLocale);
        if (mLocale.firstDayOfWeek() != previousFirstDayOfWeek) {
            // We don't have a previousDate, so just use our current one...
            // it's ignored anyway, since we're forcing the repopulation.
            populateFromVisibleDate(mVisibleDate, true);
        }
    }
}

QVariant QQuickCalendarModel1::data(const QModelIndex &index, int role) const
{
    if (role == DateRole)
        return mVisibleDates.at(index.row());
    return QVariant();
}

int QQuickCalendarModel1::rowCount(const QModelIndex &) const
{
    return mVisibleDates.isEmpty() ? 0 : weeksOnACalendarMonth * daysInAWeek;
}

QHash<int, QByteArray> QQuickCalendarModel1::roleNames() const
{
    QHash<int, QByteArray> names;
    names[DateRole] = QByteArrayLiteral("date");
    return names;
}

/*!
    Returns the date at \a index, or an invalid date if \a index is invalid.
*/
QDate QQuickCalendarModel1::dateAt(int index) const
{
    return index >= 0 && index < mVisibleDates.size() ? mVisibleDates.at(index) : QDate();
}

/*!
    Returns the index for \a date, or -1 if \a date is outside of our range.
*/
int QQuickCalendarModel1::indexAt(const QDate &date)
{
    if (mVisibleDates.size() == 0 || date < mFirstVisibleDate || date > mLastVisibleDate)
        return -1;

    // The index of the visible date will be the days from the
    // previous month that we had to display before it, plus the
    // day of the visible date itself.
    return qMax(qint64(0), mFirstVisibleDate.daysTo(date));
}

/*!
    Returns the week number for the first day of the week corresponding to \a row,
    or -1 if \a row is outside of our range.
*/
int QQuickCalendarModel1::weekNumberAt(int row) const
{
    const int index = row * daysInAWeek;
    const QDate date = dateAt(index);
    if (date.isValid())
        return date.weekNumber();
    return -1;
}

/*!
    Called before visibleDateChanged() is emitted.

    This function is called even when just the day has changed, in which case
    it does nothing.

    The \a force parameter is used when the locale has changed; the month
    shown doesn't change, but the days displayed do.
    The \a previousDate parameter is ignored when \a force is true.
*/
void QQuickCalendarModel1::populateFromVisibleDate(const QDate &previousDate, bool force)
{
    // We don't need to populate if the year and month haven't changed.
    if (!force && mVisibleDate.year() == previousDate.year() && mVisibleDate.month() == previousDate.month())
        return;

    // Since our model is of a fixed size, we fill it once and assign values each time
    // the month changes, instead of clearing and appending each time.
    bool isEmpty = mVisibleDates.isEmpty();
    if (isEmpty) {
        beginResetModel();
        mVisibleDates.fill(QDate(), daysOnACalendarMonth);
    }

    // The actual first (1st) day of the month.
    QDate firstDayOfMonthDate(mVisibleDate.year(), mVisibleDate.month(), 1);
    int difference = ((firstDayOfMonthDate.dayOfWeek() - mLocale.firstDayOfWeek()) + 7) % 7;
    // The first day to display should never be the 1st of the month, as we want some days from
    // the previous month to be visible.
    if (difference == 0)
        difference += daysInAWeek;
    QDate firstDateToDisplay = firstDayOfMonthDate.addDays(-difference);
    for (int i = 0; i < daysOnACalendarMonth; ++i)
        mVisibleDates[i] = firstDateToDisplay.addDays(i);

    mFirstVisibleDate = mVisibleDates.at(0);
    mLastVisibleDate = mVisibleDates.at(mVisibleDates.size() - 1);

    if (!isEmpty) {
        emit dataChanged(index(0, 0), index(rowCount() - 1, 0));
    } else {
        endResetModel();
        emit countChanged(rowCount());
    }
}

QT_END_NAMESPACE
