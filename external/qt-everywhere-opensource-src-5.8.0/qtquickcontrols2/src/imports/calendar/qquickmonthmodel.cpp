/****************************************************************************
**
** Copyright (C) 2015 The Qt Company Ltd.
** Contact: http://www.qt.io/licensing/
**
** This file is part of the Qt Labs Calendar module of the Qt Toolkit.
**
** $QT_BEGIN_LICENSE:LGPL3$
** Commercial License Usage
** Licensees holding valid commercial Qt licenses may use this file in
** accordance with the commercial license agreement provided with the
** Software or, alternatively, in accordance with the terms contained in
** a written agreement between you and The Qt Company. For licensing terms
** and conditions see http://www.qt.io/terms-conditions. For further
** information use the contact form at http://www.qt.io/contact-us.
**
** GNU Lesser General Public License Usage
** Alternatively, this file may be used under the terms of the GNU Lesser
** General Public License version 3 as published by the Free Software
** Foundation and appearing in the file LICENSE.LGPLv3 included in the
** packaging of this file. Please review the following information to
** ensure the GNU Lesser General Public License version 3 requirements
** will be met: https://www.gnu.org/licenses/lgpl.html.
**
** GNU General Public License Usage
** Alternatively, this file may be used under the terms of the GNU
** General Public License version 2.0 or later as published by the Free
** Software Foundation and appearing in the file LICENSE.GPL included in
** the packaging of this file. Please review the following information to
** ensure the GNU General Public License version 2.0 requirements will be
** met: http://www.gnu.org/licenses/gpl-2.0.html.
**
** $QT_END_LICENSE$
**
****************************************************************************/

#include "qquickmonthmodel_p.h"

#include <QtCore/private/qabstractitemmodel_p.h>

namespace {
    static const int daysInAWeek = 7;
    static const int weeksOnACalendarMonth = 6;
    static const int daysOnACalendarMonth = daysInAWeek * weeksOnACalendarMonth;
}

QT_BEGIN_NAMESPACE

class QQuickMonthModelPrivate : public QAbstractItemModelPrivate
{
    Q_DECLARE_PUBLIC(QQuickMonthModel)

public:
    QQuickMonthModelPrivate() : dates(daysOnACalendarMonth)
    {
        today = QDate::currentDate();
        month = today.month();
        year = today.year();
    }

    bool populate(int month, int year, const QLocale &locale, bool force = false);

    int month;
    int year;
    QString title;
    QLocale locale;
    QVector<QDate> dates;
    QDate today;
};

bool QQuickMonthModelPrivate::populate(int m, int y, const QLocale &l, bool force)
{
    Q_Q(QQuickMonthModel);
    if (!force && m == month && y == year && l.firstDayOfWeek() == locale.firstDayOfWeek())
        return false;

    // The actual first (1st) day of the month.
    QDate firstDayOfMonthDate(y, m, 1);
    int difference = ((firstDayOfMonthDate.dayOfWeek() - l.firstDayOfWeek()) + 7) % 7;
    // The first day to display should never be the 1st of the month, as we want some days from
    // the previous month to be visible.
    if (difference == 0)
        difference += 7;
    QDate firstDateToDisplay = firstDayOfMonthDate.addDays(-difference);

    today = QDate::currentDate();
    for (int i = 0; i < daysOnACalendarMonth; ++i)
        dates[i] = firstDateToDisplay.addDays(i);

    q->setTitle(l.standaloneMonthName(m) + QStringLiteral(" ") + QString::number(y));

    return true;
}

QQuickMonthModel::QQuickMonthModel(QObject *parent) :
    QAbstractListModel(*(new QQuickMonthModelPrivate), parent)
{
    Q_D(QQuickMonthModel);
    d->populate(d->month, d->year, d->locale, true);
}

int QQuickMonthModel::month() const
{
    Q_D(const QQuickMonthModel);
    return d->month;
}

void QQuickMonthModel::setMonth(int month)
{
    Q_D(QQuickMonthModel);
    if (d->month != month) {
        if (d->populate(month, d->year, d->locale))
            emit dataChanged(index(0, 0), index(daysOnACalendarMonth - 1, 0));
        d->month = month;
        emit monthChanged();
    }
}

int QQuickMonthModel::year() const
{
    Q_D(const QQuickMonthModel);
    return d->year;
}

void QQuickMonthModel::setYear(int year)
{
    Q_D(QQuickMonthModel);
    if (d->year != year) {
        if (d->populate(d->month, year, d->locale))
            emit dataChanged(index(0, 0), index(daysOnACalendarMonth - 1, 0));
        d->year = year;
        emit yearChanged();
    }
}

QLocale QQuickMonthModel::locale() const
{
    Q_D(const QQuickMonthModel);
    return d->locale;
}

void QQuickMonthModel::setLocale(const QLocale &locale)
{
    Q_D(QQuickMonthModel);
    if (d->locale != locale) {
        if (d->populate(d->month, d->year, locale))
            emit dataChanged(index(0, 0), index(daysOnACalendarMonth - 1, 0));
        d->locale = locale;
        emit localeChanged();
    }
}

QString QQuickMonthModel::title() const
{
    Q_D(const QQuickMonthModel);
    return d->title;
}

void QQuickMonthModel::setTitle(const QString &title)
{
    Q_D(QQuickMonthModel);
    if (d->title != title) {
        d->title = title;
        emit titleChanged();
    }
}

QDate QQuickMonthModel::dateAt(int index) const
{
    Q_D(const QQuickMonthModel);
    return d->dates.value(index);
}

int QQuickMonthModel::indexOf(const QDate &date) const
{
    Q_D(const QQuickMonthModel);
    if (date < d->dates.first() || date > d->dates.last())
        return -1;
    return qMax(qint64(0), d->dates.first().daysTo(date));
}

QVariant QQuickMonthModel::data(const QModelIndex &index, int role) const
{
    Q_D(const QQuickMonthModel);
    if (index.isValid() && index.row() < daysOnACalendarMonth) {
        const QDate date = d->dates.at(index.row());
        switch (role) {
        case DateRole:
            return date;
        case DayRole:
            return date.day();
        case TodayRole:
            return date == d->today;
        case WeekNumberRole:
            return date.weekNumber();
        case MonthRole:
            return date.month() - 1;
        case YearRole:
            return date.year();
        default:
            break;
        }
    }
    return QVariant();
}

int QQuickMonthModel::rowCount(const QModelIndex &parent) const
{
    if (parent.isValid())
        return 0;
    return daysOnACalendarMonth;
}

QHash<int, QByteArray> QQuickMonthModel::roleNames() const
{
    QHash<int, QByteArray> roles;
    roles[DateRole] = QByteArrayLiteral("date");
    roles[DayRole] = QByteArrayLiteral("day");
    roles[TodayRole] = QByteArrayLiteral("today");
    roles[WeekNumberRole] = QByteArrayLiteral("weekNumber");
    roles[MonthRole] = QByteArrayLiteral("month");
    roles[YearRole] = QByteArrayLiteral("year");
    return roles;
}

QT_END_NAMESPACE
