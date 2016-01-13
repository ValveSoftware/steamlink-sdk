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

#ifndef QQUICKCALENDARMODEL_H
#define QQUICKCALENDARMODEL_H

#include <QObject>
#include <QAbstractListModel>
#include <QLocale>
#include <QVariant>
#include <QDate>

QT_BEGIN_NAMESPACE

class QQuickCalendarModel : public QAbstractListModel
{
    Q_OBJECT
    Q_PROPERTY(QDate visibleDate READ visibleDate WRITE setVisibleDate NOTIFY visibleDateChanged)
    Q_PROPERTY(QLocale locale READ locale WRITE setLocale NOTIFY localeChanged)
    Q_PROPERTY(int count READ rowCount NOTIFY countChanged)

public:
    explicit QQuickCalendarModel(QObject *parent = 0);

    enum {
        // If this class is made public, this will have to be changed.
        DateRole = Qt::UserRole + 1
    };

    QDate visibleDate() const;
    void setVisibleDate(const QDate &visibleDate);

    QLocale locale() const;
    void setLocale(const QLocale &locale);

    QVariant data(const QModelIndex &index, int role) const Q_DECL_OVERRIDE;

    int rowCount(const QModelIndex &parent = QModelIndex()) const Q_DECL_OVERRIDE;

    QHash<int, QByteArray> roleNames() const Q_DECL_OVERRIDE;

    Q_INVOKABLE QDate dateAt(int index) const;
    Q_INVOKABLE int indexAt(const QDate &visibleDate);
    Q_INVOKABLE int weekNumberAt(int row) const;

Q_SIGNALS:
    void visibleDateChanged(const QDate &visibleDate);
    void localeChanged(const QLocale &locale);
    void countChanged(int count);

protected:
    void populateFromVisibleDate(const QDate &previousDate, bool force = false);

    QDate mVisibleDate;
    QDate mFirstVisibleDate;
    QDate mLastVisibleDate;
    QVector<QDate> mVisibleDates;
    QLocale mLocale;
};

QT_END_NAMESPACE

#endif // QQUICKCALENDARMODEL_H
