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

#ifndef QDECLARATIVEGEOMAPTYPE_H
#define QDECLARATIVEGEOMAPTYPE_H

#include <QtCore/QObject>
#include <QtQml/qqml.h>
#include <QtLocation/private/qgeomaptype_p.h>

QT_BEGIN_NAMESPACE

class QDeclarativeGeoMapType : public QObject
{
    Q_OBJECT
    Q_ENUMS(MapStyle)

    Q_PROPERTY(MapStyle style READ style CONSTANT)
    Q_PROPERTY(QString name READ name CONSTANT)
    Q_PROPERTY(QString description READ description CONSTANT)
    Q_PROPERTY(bool mobile READ mobile CONSTANT)
    Q_PROPERTY(bool night READ night CONSTANT REVISION 1)

public:
    enum MapStyle {
        NoMap = QGeoMapType::NoMap,
        StreetMap = QGeoMapType::StreetMap,
        SatelliteMapDay = QGeoMapType::SatelliteMapDay,
        SatelliteMapNight = QGeoMapType::SatelliteMapNight,
        TerrainMap = QGeoMapType::TerrainMap,
        HybridMap = QGeoMapType::HybridMap,
        TransitMap = QGeoMapType::TransitMap,
        GrayStreetMap = QGeoMapType::GrayStreetMap,
        PedestrianMap = QGeoMapType::PedestrianMap,
        CarNavigationMap = QGeoMapType::CarNavigationMap,
        CustomMap = 100
    };

    QDeclarativeGeoMapType(const QGeoMapType mapType, QObject *parent = 0);
    ~QDeclarativeGeoMapType();

    MapStyle style() const;
    QString name() const;
    QString description() const;
    bool mobile() const;
    bool night() const;

    const QGeoMapType mapType() { return mapType_; }

private:
    QGeoMapType mapType_;
};

QT_END_NAMESPACE

QML_DECLARE_TYPE(QDeclarativeGeoMapType)

#endif
