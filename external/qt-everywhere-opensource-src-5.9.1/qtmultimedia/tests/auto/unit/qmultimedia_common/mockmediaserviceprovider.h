/****************************************************************************
**
** Copyright (C) 2016 The Qt Company Ltd.
** Contact: https://www.qt.io/licensing/
**
** This file is part of the test suite of the Qt Toolkit.
**
** $QT_BEGIN_LICENSE:GPL-EXCEPT$
** Commercial License Usage
** Licensees holding valid commercial Qt licenses may use this file in
** accordance with the commercial license agreement provided with the
** Software or, alternatively, in accordance with the terms contained in
** a written agreement between you and The Qt Company. For licensing terms
** and conditions see https://www.qt.io/terms-conditions. For further
** information use the contact form at https://www.qt.io/contact-us.
**
** GNU General Public License Usage
** Alternatively, this file may be used under the terms of the GNU
** General Public License version 3 as published by the Free Software
** Foundation with exceptions as appearing in the file LICENSE.GPL3-EXCEPT
** included in the packaging of this file. Please review the following
** information to ensure the GNU General Public License requirements will
** be met: https://www.gnu.org/licenses/gpl-3.0.html.
**
** $QT_END_LICENSE$
**
****************************************************************************/

#ifndef MOCKMEDIASERVICEPROVIDER_H
#define MOCKMEDIASERVICEPROVIDER_H

#include "qmediaserviceprovider_p.h"
#include "qmediaservice.h"
#include "mockvideodeviceselectorcontrol.h"
#include "mockcamerainfocontrol.h"

// Simple provider that lets you set the service
class MockMediaServiceProvider : public QMediaServiceProvider
{
public:
    MockMediaServiceProvider(QMediaService* s = 0, bool del=false)
        : service(s), deleteServiceOnRelease(del)
    {
    }

    QMediaService *requestService(const QByteArray &, const QMediaServiceProviderHint &)
    {
        return service;
    }

    void releaseService(QMediaService *service)
    {
        if (deleteServiceOnRelease) {
            delete service;
            this->service = 0;
        }
    }

    QMediaServiceProviderHint::Features supportedFeatures(const QMediaService *) const
    {
        return features;
    }

    void setSupportedFeatures(QMediaServiceProviderHint::Features f)
    {
        features = f;
    }

    QByteArray defaultDevice(const QByteArray &serviceType) const
    {
        if (serviceType == Q_MEDIASERVICE_CAMERA)
            return MockVideoDeviceSelectorControl::defaultCamera();

        return QByteArray();
    }

    QList<QByteArray> devices(const QByteArray &serviceType) const
    {
        if (serviceType == Q_MEDIASERVICE_CAMERA)
            return MockVideoDeviceSelectorControl::availableCameras();

        return QList<QByteArray>();
    }

    QString deviceDescription(const QByteArray &serviceType, const QByteArray &device)
    {
        if (serviceType == Q_MEDIASERVICE_CAMERA)
            return MockVideoDeviceSelectorControl::cameraDescription(device);

        return QString();
    }

    QCamera::Position cameraPosition(const QByteArray &device) const
    {
        return MockCameraInfoControl::position(device);
    }

    int cameraOrientation(const QByteArray &device) const
    {
        return MockCameraInfoControl::orientation(device);
    }

    QMediaService *service;
    bool deleteServiceOnRelease;
    QMediaServiceProviderHint::Features features;
};

#endif // MOCKMEDIASERVICEPROVIDER_H
