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

#include "qdeclarativegeomapitemgroup_p.h"

QT_BEGIN_NAMESPACE

/*!
    \qmltype MapItemGroup
    \instantiates QDeclarativeGeoMapItemGroup
    \inqmlmodule QtLocation
    \ingroup qml-QtLocation5-maps
    \since Qt Location 5.9

    \brief The MapItemGroup type is a container for map items.

    Its purpose is to enable code modularization by allowing the usage
    of qml files containing map elements related to each other, and
    the associated bindings.

    \note The release of this API with Qt 5.9 is a Technology Preview.

    \section2 Example Usage

    The following snippet shows how to use a MapItemGroup to create a MapCircle, centered at
    the coordinate (63, -18) with a radius of 100km, filled in red, surrounded by an ondulated green border,
    both contained in a semitransparent blue circle with a MouseArea that moves the whole group.
    This group is defined in a separate file named PolygonGroup.qml:

    \code
    import QtQuick 2.4
    import QtPositioning 5.6
    import QtLocation 5.9

    MapItemGroup {
        id: itemGroup
        property alias position: mainCircle.center
        property var radius: 100 * 1000
        property var borderHeightPct : 0.3

        MapCircle {
            id: mainCircle
            center : QtPositioning.coordinate(40, 0)
            radius: itemGroup.radius * (1.0 + borderHeightPct)
            opacity: 0.05
            visible: true
            color: 'blue'

            MouseArea{
                anchors.fill: parent
                drag.target: parent
                id: maItemGroup
            }
        }

        MapCircle {
            id: groupCircle
            center: itemGroup.position
            radius: itemGroup.radius
            color: 'crimson'

            onCenterChanged: {
                groupPolyline.populateBorder();
            }
        }

        MapPolyline {
            id: groupPolyline
            line.color: 'green'
            line.width: 3

            function populateBorder() {
                groupPolyline.path = [] // clearing the path
                var waveLength = 8.0;
                var waveAmplitude = groupCircle.radius * borderHeightPct;
                for (var i=0; i <= 360; i++) {
                    var wavePhase = (i/360.0 * 2.0 * Math.PI )* waveLength
                    var waveHeight = (Math.cos(wavePhase) + 1.0) / 2.0
                    groupPolyline.addCoordinate(groupCircle.center.atDistanceAndAzimuth(groupCircle.radius + waveAmplitude * waveHeight , i))
                }
            }

            Component.onCompleted: {
                populateBorder()
            }
        }
    }
    \endcode

    PolygonGroup.qml is now a reusable component that can then be used in a Map as:

    \code
    Map {
        id: map
        PolygonGroup {
            id: polygonGroup
            position: QtPositioning.coordinate(63,-18)
        }
    }
    \endcode

    \image api-mapitemgroup.png
*/

QDeclarativeGeoMapItemGroup::QDeclarativeGeoMapItemGroup(QQuickItem *parent): QQuickItem(parent), m_quickMap(nullptr)
{

}

QDeclarativeGeoMapItemGroup::~QDeclarativeGeoMapItemGroup()
{

}

void QDeclarativeGeoMapItemGroup::setQuickMap(QDeclarativeGeoMap *quickMap)
{
    m_quickMap = quickMap;
}

QDeclarativeGeoMap *QDeclarativeGeoMapItemGroup::quickMap() const
{
    return m_quickMap;
}

QT_END_NAMESPACE
