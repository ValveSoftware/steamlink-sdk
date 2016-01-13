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
