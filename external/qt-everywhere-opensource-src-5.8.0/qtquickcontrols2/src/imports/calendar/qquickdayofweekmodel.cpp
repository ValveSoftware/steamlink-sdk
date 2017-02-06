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

#include "qquickdayofweekmodel_p.h"

#include <QtCore/private/qabstractitemmodel_p.h>

QT_BEGIN_NAMESPACE

class QQuickDayOfWeekModelPrivate : public QAbstractItemModelPrivate
{
    Q_DECLARE_PUBLIC(QQuickDayOfWeekModel)

public:
    QLocale locale;
};

QQuickDayOfWeekModel::QQuickDayOfWeekModel(QObject *parent) :
    QAbstractListModel(*(new QQuickDayOfWeekModelPrivate), parent)
{
}

QLocale QQuickDayOfWeekModel::locale() const
{
    Q_D(const QQuickDayOfWeekModel);
    return d->locale;
}

void QQuickDayOfWeekModel::setLocale(const QLocale &locale)
{
    Q_D(QQuickDayOfWeekModel);
    if (d->locale != locale) {
        d->locale = locale;
        emit localeChanged();
        emit dataChanged(index(0, 0), index(6, 0));
    }
}

int QQuickDayOfWeekModel::dayAt(int index) const
{
    Q_D(const QQuickDayOfWeekModel);
    int day = d->locale.firstDayOfWeek() + index;
    if (day > 7)
        day -= 7;
    if (day == 7)
        day = 0; // Qt::Sunday = 7, but Sunday is 0 in JS Date
    return day;
}

QVariant QQuickDayOfWeekModel::data(const QModelIndex &index, int role) const
{
    Q_D(const QQuickDayOfWeekModel);
    if (index.isValid() && index.row() < 7) {
        int day = dayAt(index.row());
        switch (role) {
        case DayRole:
            return day;
        case LongNameRole:
            return d->locale.standaloneDayName(day == 0 ? Qt::Sunday : day, QLocale::LongFormat);
        case ShortNameRole:
            return d->locale.standaloneDayName(day == 0 ? Qt::Sunday : day, QLocale::ShortFormat);
        case NarrowNameRole:
            return d->locale.standaloneDayName(day == 0 ? Qt::Sunday : day, QLocale::NarrowFormat);
        default:
            break;
        }
    }
    return QVariant();
}

int QQuickDayOfWeekModel::rowCount(const QModelIndex &parent) const
{
    if (parent.isValid())
        return 0;
    return 7;
}

QHash<int, QByteArray> QQuickDayOfWeekModel::roleNames() const
{
    QHash<int, QByteArray> roles;
    roles[DayRole] = QByteArrayLiteral("day");
    roles[LongNameRole] = QByteArrayLiteral("longName");
    roles[ShortNameRole] = QByteArrayLiteral("shortName");
    roles[NarrowNameRole] = QByteArrayLiteral("narrowName");
    return roles;
}

QT_END_NAMESPACE
