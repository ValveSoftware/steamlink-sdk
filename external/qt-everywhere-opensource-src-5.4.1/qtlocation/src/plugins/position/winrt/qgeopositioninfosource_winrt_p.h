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

#ifndef QGEOPOSITIONINFOSOURCEWINRT_H
#define QGEOPOSITIONINFOSOURCEWINRT_H

#include "qgeopositioninfosource.h"
#include "qgeopositioninfo.h"

#include <QTimer>

#include <EventToken.h>
#include <wrl.h>

namespace ABI {
    namespace Windows {
        namespace Devices {
            namespace Geolocation{
                struct IGeolocator;
                struct IPositionChangedEventArgs;
                struct IStatusChangedEventArgs;
            }
        }
    }
}

QT_BEGIN_NAMESPACE

class QGeoPositionInfoSourceWinrt : public QGeoPositionInfoSource
{
    Q_OBJECT
public:
    QGeoPositionInfoSourceWinrt(QObject *parent = 0);
    ~QGeoPositionInfoSourceWinrt();

    QGeoPositionInfo lastKnownPosition(bool fromSatellitePositioningMethodsOnly = false) const;
    PositioningMethods supportedPositioningMethods() const;

    void setPreferredPositioningMethods(PositioningMethods methods);

    void setUpdateInterval(int msec);
    int minimumUpdateInterval() const;
    Error error() const;

    HRESULT onPositionChanged(ABI::Windows::Devices::Geolocation::IGeolocator *locator,
                              ABI::Windows::Devices::Geolocation::IPositionChangedEventArgs *args);

public slots:
    void startUpdates();
    void stopUpdates();

    void requestUpdate(int timeout = 0);

private slots:
    void stopHandler();
    void virtualPositionUpdate();
    void singleUpdateTimeOut();
private:
    bool startHandler();

    Q_DISABLE_COPY(QGeoPositionInfoSourceWinrt)
    void setError(QGeoPositionInfoSource::Error positionError);
    bool checkNativeState();

    Microsoft::WRL::ComPtr<ABI::Windows::Devices::Geolocation::IGeolocator> m_locator;
    EventRegistrationToken m_positionToken;

    QGeoPositionInfo m_lastPosition;
    QGeoPositionInfoSource::Error m_positionError;

    //EventRegistrationToken m_StatusToken;
    QTimer m_periodicTimer;
    QTimer m_singleUpdateTimer;
    bool m_updatesOngoing;
};

QT_END_NAMESPACE

#endif // QGEOPOSITIONINFOSOURCEWINRT_H
