/****************************************************************************
**
** Copyright (C) 2013 Jolla Ltd.
** Contact: Aaron McCarthy <aaron.mccarthy@jollamobile.com>
** Copyright (C) 2014 Digia Plc and/or its subsidiary(-ies).
** Contact: http://www.qt-project.org/legal
**
** This file is part of the QtPositioning module of the Qt Toolkit.
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

#ifndef QGEOPOSITIONINFOSOURCE_GEOCLUEMASTER_H
#define QGEOPOSITIONINFOSOURCE_GEOCLUEMASTER_H

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

#include "qgeocluemaster.h"

#include <qgeopositioninfosource.h>
#include <geoclue/geoclue-velocity.h>
#include <QTimer>

//#define Q_LOCATION_GEOCLUE_DEBUG

QT_BEGIN_NAMESPACE

class QGeoPositionInfoSourceGeoclueMaster : public QGeoPositionInfoSource, public QGeoclueMaster
{
    Q_OBJECT

public:
    QGeoPositionInfoSourceGeoclueMaster(QObject *parent = 0);
    ~QGeoPositionInfoSourceGeoclueMaster();

    // From QGeoPositionInfoSource
    void setUpdateInterval(int msec);
    QGeoPositionInfo lastKnownPosition(bool fromSatellitePositioningMethodsOnly = false) const;
    PositioningMethods supportedPositioningMethods() const;
    void setPreferredPositioningMethods(PositioningMethods methods);
    int minimumUpdateInterval() const;

    void updatePosition(GeocluePositionFields fields, int timestamp, double latitude,
                        double longitude, double altitude, GeoclueAccuracy *accuracy);

    void regularUpdateFailed();

    void velocityUpdateFailed();
    void velocityUpdateSucceeded(GeoclueVelocityFields fields, int timestamp, double speed,
                                 double direction, double climb);

    Error error() const;

public slots:
    virtual void startUpdates();
    virtual void stopUpdates();
    virtual void requestUpdate(int timeout = 5000);

private slots:
    void requestUpdateTimeout();
    void positionProviderChanged(const QByteArray &service, const QByteArray &path);

private:
    void configurePositionSource();
    void cleanupPositionSource();
    void setOptions();

private:
    GeocluePosition *m_pos;
    GeoclueVelocity *m_vel;
    QTimer m_requestTimer;
    bool m_lastVelocityIsFresh;
    bool m_regularUpdateTimedOut;
    double m_lastVelocity;
    double m_lastDirection;
    double m_lastClimb;
    bool m_lastPositionFromSatellite;
    QGeoPositionInfo m_lastPosition;
    PositioningMethods m_methods;
    bool m_running;
    QGeoPositionInfoSource::Error m_error;
};

QT_END_NAMESPACE

#endif // QGEOPOSITIONINFOSOURCE_GEOCLUEMASTER_H
