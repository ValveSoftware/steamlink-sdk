/****************************************************************************
**
** Copyright (C) 2014 Digia Plc and/or its subsidiary(-ies).
** Contact: http://www.qt-project.org/legal
**
** This file is part of the QtLocation module of the Qt Toolkit.
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

#ifndef QDECLARATIVERATINGS_P_H
#define QDECLARATIVERATINGS_P_H

#include <qplaceratings.h>
#include <QtQml/qqml.h>

#include <QObject>

QT_BEGIN_NAMESPACE

class QDeclarativeRatings : public QObject
{
    Q_OBJECT

    Q_PROPERTY(QPlaceRatings ratings READ ratings WRITE setRatings)
    Q_PROPERTY(qreal average READ average WRITE setAverage NOTIFY averageChanged)
    Q_PROPERTY(qreal maximum READ maximum WRITE setMaximum NOTIFY maximumChanged)
    Q_PROPERTY(int count READ count WRITE setCount NOTIFY countChanged)

public:
    explicit QDeclarativeRatings(QObject *parent = 0);
    explicit QDeclarativeRatings(const QPlaceRatings &src, QObject *parent = 0);
    ~QDeclarativeRatings();

    QPlaceRatings ratings() const;
    void setRatings(const QPlaceRatings &src);

    qreal average() const;
    void setAverage(qreal average);

    qreal maximum() const;
    void setMaximum(qreal max);

    int count() const;
    void setCount(int count);

Q_SIGNALS:
    void averageChanged();
    void maximumChanged();
    void countChanged();

private:
    QPlaceRatings m_ratings;
};

QT_END_NAMESPACE

QML_DECLARE_TYPE(QDeclarativeRatings)

#endif // QDECLARATIVERATING_P_H
