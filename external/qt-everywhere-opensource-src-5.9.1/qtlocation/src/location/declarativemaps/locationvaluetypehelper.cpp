/****************************************************************************
**
** Copyright (C) 2015 The Qt Company Ltd.
** Contact: http://www.qt.io/licensing/
**
** This file is part of the QtLocation module of the Qt Toolkit.
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

#include "locationvaluetypehelper_p.h"


QGeoCoordinate parseCoordinate(const QJSValue &value, bool *ok)
{
    QGeoCoordinate c;

    if (value.isObject()) {
        if (value.hasProperty(QStringLiteral("latitude")))
            c.setLatitude(value.property(QStringLiteral("latitude")).toNumber());
        if (value.hasProperty(QStringLiteral("longitude")))
            c.setLongitude(value.property(QStringLiteral("longitude")).toNumber());
        if (value.hasProperty(QStringLiteral("altitude")))
            c.setAltitude(value.property(QStringLiteral("altitude")).toNumber());

        if (ok)
            *ok = true;
    }

    return c;
}

QGeoRectangle parseRectangle(const QJSValue &value, bool *ok)
{
    QGeoRectangle r;

    *ok = false;

    if (value.isObject()) {
        if (value.hasProperty(QStringLiteral("bottomLeft"))) {
            QGeoCoordinate c = parseCoordinate(value.property(QStringLiteral("bottomLeft")), ok);
            if (*ok)
                r.setBottomLeft(c);
        }
        if (value.hasProperty(QStringLiteral("bottomRight"))) {
            QGeoCoordinate c = parseCoordinate(value.property(QStringLiteral("bottomRight")), ok);
            if (*ok)
                r.setBottomRight(c);
        }
        if (value.hasProperty(QStringLiteral("topLeft"))) {
            QGeoCoordinate c = parseCoordinate(value.property(QStringLiteral("topLeft")), ok);
            if (*ok)
                r.setTopLeft(c);
        }
        if (value.hasProperty(QStringLiteral("topRight"))) {
            QGeoCoordinate c = parseCoordinate(value.property(QStringLiteral("topRight")), ok);
            if (*ok)
                r.setTopRight(c);
        }
        if (value.hasProperty(QStringLiteral("center"))) {
            QGeoCoordinate c = parseCoordinate(value.property(QStringLiteral("center")), ok);
            if (*ok)
                r.setCenter(c);
        }
        if (value.hasProperty(QStringLiteral("height")))
            r.setHeight(value.property(QStringLiteral("height")).toNumber());
        if (value.hasProperty(QStringLiteral("width")))
            r.setWidth(value.property(QStringLiteral("width")).toNumber());
    }

    return r;
}

QGeoCircle parseCircle(const QJSValue &value, bool *ok)
{
    QGeoCircle c;

    *ok = false;

    if (value.isObject()) {
        if (value.hasProperty(QStringLiteral("center"))) {
            QGeoCoordinate coord = parseCoordinate(value.property(QStringLiteral("center")), ok);
            if (*ok)
                c.setCenter(coord);
        }
        if (value.hasProperty(QStringLiteral("radius")))
            c.setRadius(value.property(QStringLiteral("radius")).toNumber());
    }

    return c;
}
