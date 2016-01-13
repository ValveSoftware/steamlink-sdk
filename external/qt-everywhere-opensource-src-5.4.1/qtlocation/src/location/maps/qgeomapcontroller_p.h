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
#ifndef QGEOMAPCONTROLLER_P_H
#define QGEOMAPCONTROLLER_P_H

//
//  W A R N I N G
//  -------------
//
// This file is not part of the Qt API.  It exists purely as an
// implementation detail.  This header file may change from version to
// version without notice, or even be removed.
//
// We mean it.
//

#include <QObject>

#include "qgeocoordinate.h"
#include "qgeocameradata_p.h"

QT_BEGIN_NAMESPACE

class QGeoMapData;


class Q_LOCATION_EXPORT QGeoMapController : public QObject
{
    Q_OBJECT

    Q_PROPERTY(QGeoCoordinate center READ center WRITE setCenter NOTIFY centerChanged)
    Q_PROPERTY(qreal bearing READ bearing WRITE setBearing NOTIFY bearingChanged)
    Q_PROPERTY(qreal tilt READ tilt WRITE setTilt NOTIFY tiltChanged)
    Q_PROPERTY(qreal roll READ roll WRITE setRoll NOTIFY rollChanged)
    Q_PROPERTY(qreal zoom READ zoom WRITE setZoom NOTIFY zoomChanged)

public:
    QGeoMapController(QGeoMapData *map);
    ~QGeoMapController();

    QGeoCoordinate center() const;
    void setCenter(const QGeoCoordinate &center);

    void setLatitude(qreal latitude);
    void setLongitude(qreal longitude);
    void setAltitude(qreal altitude);

    qreal bearing() const;
    void setBearing(qreal bearing);

    qreal tilt() const;
    void setTilt(qreal tilt);

    qreal roll() const;
    void setRoll(qreal roll);

    qreal zoom() const;
    void setZoom(qreal zoom);

    void pan(qreal dx, qreal dy);

private Q_SLOTS:
    void cameraDataChanged(const QGeoCameraData &cameraData);

Q_SIGNALS:
    void centerChanged(const QGeoCoordinate &center);
    void bearingChanged(qreal bearing);
    void tiltChanged(qreal tilt);
    void rollChanged(qreal roll);
    void zoomChanged(qreal zoom);

private:
    QGeoMapData *map_;
    QGeoCameraData oldCameraData_;
};

QT_END_NAMESPACE

#endif // QGEOMAPCONTROLLER_P_H
