/****************************************************************************
**
** Copyright (C) 2016 The Qt Company Ltd.
** Contact: https://www.qt.io/licensing/
**
** This file is part of the QtPositioning module of the Qt Toolkit.
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

#ifndef LOCATIONSINGLETON_H
#define LOCATIONSINGLETON_H

#include <QtCore/QObject>
#include <QtCore/qnumeric.h>
#include <QtPositioning/QGeoCoordinate>
#include <QtPositioning/QGeoShape>
#include <QtPositioning/QGeoRectangle>
#include <QtPositioning/QGeoCircle>
#include <QVariant>

class LocationSingleton : public QObject
{
    Q_OBJECT

public:
    explicit LocationSingleton(QObject *parent = 0);

    Q_INVOKABLE QGeoCoordinate coordinate() const;
    Q_INVOKABLE QGeoCoordinate coordinate(double latitude, double longitude,
                                          double altitude = qQNaN()) const;

    Q_INVOKABLE QGeoShape shape() const;

    Q_INVOKABLE QGeoRectangle rectangle() const;
    Q_INVOKABLE QGeoRectangle rectangle(const QGeoCoordinate &center,
                                        double width, double height) const;
    Q_INVOKABLE QGeoRectangle rectangle(const QGeoCoordinate &topLeft,
                                        const QGeoCoordinate &bottomRight) const;
    Q_INVOKABLE QGeoRectangle rectangle(const QVariantList &coordinates) const;

    Q_INVOKABLE QGeoCircle circle() const;
    Q_INVOKABLE QGeoCircle circle(const QGeoCoordinate &center, qreal radius = -1.0) const;

    Q_INVOKABLE QGeoCircle shapeToCircle(const QGeoShape &shape) const;
    Q_INVOKABLE QGeoRectangle shapeToRectangle(const QGeoShape &shape) const;
};

#endif // LOCATIONSINGLETON_H
