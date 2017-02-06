/****************************************************************************
**
** Copyright (C) 2016 The Qt Company Ltd and/or its subsidiary(-ies).
** Contact: https://www.qt.io/licensing/
**
** This file is part of the Qt Toolkit.
**
** $QT_BEGIN_LICENSE:LGPL$
** Commercial License Usage
** Licensees holding valid commercial Qt licenses may use this file in
** accordance with the commercial license agreement provided with the
** Software or, alternatively, in accordance with the terms contained in
** a written agreement between you and The Qt Company. For licensing terms
** and conditions see https://www.qt.io/terms-conditions. For further
** information use the contact form at https://www.qt.io/contact-us.
**
** GNU Lesser General Public License Usage
** Alternatively, this file may be used under the terms of the GNU Lesser
** General Public License version 3 as published by the Free Software
** Foundation and appearing in the file LICENSE.LGPL3 included in the
** packaging of this file. Please review the following information to
** ensure the GNU Lesser General Public License version 3 requirements
** will be met: https://www.gnu.org/licenses/lgpl-3.0.html.
**
** GNU General Public License Usage
** Alternatively, this file may be used under the terms of the GNU
** General Public License version 2.0 or (at your option) the GNU General
** Public license version 3 or any later version approved by the KDE Free
** Qt Foundation. The licenses are as published by the Free Software
** Foundation and appearing in the file LICENSE.GPL2 and LICENSE.GPL3
** included in the packaging of this file. Please review the following
** information to ensure the GNU General Public License requirements will
** be met: https://www.gnu.org/licenses/gpl-2.0.html and
** https://www.gnu.org/licenses/gpl-3.0.html.
**
** $QT_END_LICENSE$
**
****************************************************************************/

#include <QtCore/QString>
#include <QtCore/QFile>

#include "qwinrtserviceplugin.h"
#include "qwinrtmediaplayerservice.h"
#include "qwinrtcameraservice.h"
#include "qwinrtvideodeviceselectorcontrol.h"

QT_BEGIN_NAMESPACE

QMediaService *QWinRTServicePlugin::create(QString const &key)
{
    if (key == QLatin1String(Q_MEDIASERVICE_MEDIAPLAYER))
        return new QWinRTMediaPlayerService(this);

    if (key == QLatin1String(Q_MEDIASERVICE_CAMERA))
        return new QWinRTCameraService(this);

    return Q_NULLPTR;
}

void QWinRTServicePlugin::release(QMediaService *service)
{
    delete service;
}

QMediaServiceProviderHint::Features QWinRTServicePlugin::supportedFeatures(
        const QByteArray &service) const
{
    if (service == Q_MEDIASERVICE_MEDIAPLAYER)
       return QMediaServiceProviderHint::StreamPlayback | QMediaServiceProviderHint::VideoSurface;

    return QMediaServiceProviderHint::Features();
}

QCamera::Position QWinRTServicePlugin::cameraPosition(const QByteArray &device) const
{
    return QWinRTVideoDeviceSelectorControl::cameraPosition(device);
}

int QWinRTServicePlugin::cameraOrientation(const QByteArray &device) const
{
    return QWinRTVideoDeviceSelectorControl::cameraOrientation(device);
}

QList<QByteArray> QWinRTServicePlugin::devices(const QByteArray &service) const
{
    if (service == Q_MEDIASERVICE_CAMERA)
        return QWinRTVideoDeviceSelectorControl::deviceNames();

    return QList<QByteArray>();
}

QString QWinRTServicePlugin::deviceDescription(const QByteArray &service, const QByteArray &device)
{
    if (service == Q_MEDIASERVICE_CAMERA)
        return QWinRTVideoDeviceSelectorControl::deviceDescription(device);

    return QString();
}

QByteArray QWinRTServicePlugin::defaultDevice(const QByteArray &service) const
{
    if (service == Q_MEDIASERVICE_CAMERA)
        return QWinRTVideoDeviceSelectorControl::defaultDeviceName();

    return QByteArray();
}

QT_END_NAMESPACE
