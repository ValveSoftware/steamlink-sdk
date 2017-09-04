/****************************************************************************
**
** Copyright (C) 2016 The Qt Company Ltd.
** Contact: https://www.qt.io/licensing/
**
** This file is part of the Qt Toolkit.
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

#include <qmediaserviceproviderplugin.h>
#include <qmediaservice.h>
#include "../mockservice.h"

class MockServicePlugin3 : public QMediaServiceProviderPlugin,
                           public QMediaServiceSupportedDevicesInterface,
                           public QMediaServiceDefaultDeviceInterface,
                           public QMediaServiceCameraInfoInterface
{
    Q_OBJECT
    Q_INTERFACES(QMediaServiceSupportedDevicesInterface)
    Q_INTERFACES(QMediaServiceDefaultDeviceInterface)
    Q_INTERFACES(QMediaServiceCameraInfoInterface)
    Q_PLUGIN_METADATA(IID "org.qt-project.qt.mediaserviceproviderfactory/5.0" FILE "mockserviceplugin3.json")
public:
    QStringList keys() const
    {
        return QStringList() <<
               QLatin1String(Q_MEDIASERVICE_MEDIAPLAYER) <<
               QLatin1String(Q_MEDIASERVICE_AUDIOSOURCE) <<
               QLatin1String(Q_MEDIASERVICE_CAMERA);
    }

    QMediaService* create(QString const& key)
    {
        if (keys().contains(key))
            return new MockMediaService("MockServicePlugin3");
        else
            return 0;
    }

    void release(QMediaService *service)
    {
        delete service;
    }

    QByteArray defaultDevice(const QByteArray &service) const
    {
        if (service == Q_MEDIASERVICE_AUDIOSOURCE)
            return "audiosource1";

        if (service == Q_MEDIASERVICE_CAMERA)
            return "frontcamera";

        return QByteArray();
    }

    QList<QByteArray> devices(const QByteArray &service) const
    {
        QList<QByteArray> res;
        if (service == Q_MEDIASERVICE_AUDIOSOURCE)
            res << "audiosource1" << "audiosource2";

        if (service == Q_MEDIASERVICE_CAMERA)
            res << "frontcamera";

        return res;
    }

    QString deviceDescription(const QByteArray &service, const QByteArray &device)
    {
        if (devices(service).contains(device))
            return QString(device)+" description";
        else
            return QString();
    }

    QCamera::Position cameraPosition(const QByteArray &device) const
    {
        if (device == "frontcamera")
            return QCamera::FrontFace;

        return QCamera::UnspecifiedPosition;
    }

    int cameraOrientation(const QByteArray &device) const
    {
        if (device == "frontcamera")
            return 270;

        return 0;
    }
};

#include "mockserviceplugin3.moc"

