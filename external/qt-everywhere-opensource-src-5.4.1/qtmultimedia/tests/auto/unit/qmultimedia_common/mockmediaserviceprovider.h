/****************************************************************************
**
** Copyright (C) 2014 Digia Plc and/or its subsidiary(-ies).
** Contact: http://www.qt-project.org/legal
**
** This file is part of the test suite of the Qt Toolkit.
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
};

#endif // MOCKMEDIASERVICEPROVIDER_H
