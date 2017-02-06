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

#include "qquickweeknumbermodel_p.h"

#include <QtCore/private/qabstractitemmodel_p.h>
#include <QtCore/qdatetime.h>

QT_BEGIN_NAMESPACE

class QQuickWeekNumberModelPrivate : public QAbstractItemModelPrivate
{
    Q_DECLARE_PUBLIC(QQuickWeekNumberModel)

public:
    QQuickWeekNumberModelPrivate() : month(-1), year(-1)
    {
        QDate date = QDate::currentDate();
        init(date.month(), date.year(), locale);
        month = date.month();
        year = date.year();
    }

    void init(int month, int year, const QLocale &locale = QLocale());
    static QDate calculateFirst(int month, int year, const QLocale &locale);

    int month;
    int year;
    QLocale locale;
    int weekNumbers[6];
};

void QQuickWeekNumberModelPrivate::init(int m, int y, const QLocale &l)
{
    Q_Q(QQuickWeekNumberModel);
    if (m == month && y == year && l.firstDayOfWeek() == locale.firstDayOfWeek())
        return;

    // The actual first (1st) day of the month.
    QDate firstDayOfMonthDate(y, m, 1);
    int difference = ((firstDayOfMonthDate.dayOfWeek() - l.firstDayOfWeek()) + 7) % 7;
    // The first day to display should never be the 1st of the month, as we want some days from
    // the previous month to be visible.
    if (difference == 0)
        difference += 7;

    for (int i = 0; i < 6; ++i)
        weekNumbers[i] = firstDayOfMonthDate.addDays(i * 7 - difference).weekNumber();

    if (q) // null at construction
        emit q->dataChanged(q->index(0, 0), q->index(5, 0));
}

QQuickWeekNumberModel::QQuickWeekNumberModel(QObject *parent) :
    QAbstractListModel(*(new QQuickWeekNumberModelPrivate), parent)
{
}

int QQuickWeekNumberModel::month() const
{
    Q_D(const QQuickWeekNumberModel);
    return d->month;
}

void QQuickWeekNumberModel::setMonth(int month)
{
    Q_D(QQuickWeekNumberModel);
    if (d->month != month) {
        d->init(month, d->year, d->locale);
        d->month = month;
        emit monthChanged();
    }
}

int QQuickWeekNumberModel::year() const
{
    Q_D(const QQuickWeekNumberModel);
    return d->year;
}

void QQuickWeekNumberModel::setYear(int year)
{
    Q_D(QQuickWeekNumberModel);
    if (d->year != year) {
        d->init(d->month, year, d->locale);
        d->year = year;
        emit yearChanged();
    }
}

QLocale QQuickWeekNumberModel::locale() const
{
    Q_D(const QQuickWeekNumberModel);
    return d->locale;
}

void QQuickWeekNumberModel::setLocale(const QLocale &locale)
{
    Q_D(QQuickWeekNumberModel);
    if (d->locale != locale) {
        d->init(d->month, d->year, locale);
        d->locale = locale;
        emit localeChanged();
    }
}

int QQuickWeekNumberModel::weekNumberAt(int index) const
{
    Q_D(const QQuickWeekNumberModel);
    if (index < 0 || index > 5)
        return -1;
    return d->weekNumbers[index];
}

int QQuickWeekNumberModel::indexOf(int weekNumber) const
{
    Q_D(const QQuickWeekNumberModel);
    if (weekNumber < d->weekNumbers[0] || weekNumber > d->weekNumbers[5])
        return -1;
    return weekNumber - d->weekNumbers[0];
}

QVariant QQuickWeekNumberModel::data(const QModelIndex &index, int role) const
{
    if (role == WeekNumberRole) {
        int weekNumber = weekNumberAt(index.row());
        if (weekNumber != -1)
            return weekNumber;
    }
    return QVariant();
}

int QQuickWeekNumberModel::rowCount(const QModelIndex &parent) const
{
    if (parent.isValid())
        return 0;
    return 6;
}

QHash<int, QByteArray> QQuickWeekNumberModel::roleNames() const
{
    QHash<int, QByteArray> roles;
    roles[WeekNumberRole] = QByteArrayLiteral("weekNumber");
    return roles;
}

QT_END_NAMESPACE
