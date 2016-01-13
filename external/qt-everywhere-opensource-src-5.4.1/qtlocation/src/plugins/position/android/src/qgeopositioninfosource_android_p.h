/****************************************************************************
**
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

#ifndef QGEOPOSITIONINFOSOURCE_ANDROID_P_H
#define QGEOPOSITIONINFOSOURCE_ANDROID_P_H

#include <QGeoPositionInfoSource>
#include <QTimer>

class QGeoPositionInfoSourceAndroid : public QGeoPositionInfoSource
{
    Q_OBJECT
public:
    QGeoPositionInfoSourceAndroid(QObject *parent = 0);
    ~QGeoPositionInfoSourceAndroid();

    // From QGeoPositionInfoSource
    void setUpdateInterval(int msec);
    QGeoPositionInfo lastKnownPosition(bool fromSatellitePositioningMethodsOnly = false) const;
    PositioningMethods supportedPositioningMethods() const;
    void setPreferredPositioningMethods(PositioningMethods methods);
    int minimumUpdateInterval() const;
    Error error() const;

public Q_SLOTS:
    virtual void startUpdates();
    virtual void stopUpdates();

    virtual void requestUpdate(int timeout = 0);

    void processPositionUpdate(const QGeoPositionInfo& pInfo);
    void processSinglePositionUpdate(const QGeoPositionInfo& pInfo);

    void locationProviderDisabled();
private Q_SLOTS:
    void requestTimeout();

private:
    void reconfigureRunningSystem();

    bool updatesRunning;
    int androidClassKeyForUpdate;
    int androidClassKeyForSingleRequest;
    QList<QGeoPositionInfo> queuedSingleUpdates;
    Error m_error;
    QTimer m_requestTimer;
};

#endif // QGEOPOSITIONINFOSOURCE_ANDROID_P_H
