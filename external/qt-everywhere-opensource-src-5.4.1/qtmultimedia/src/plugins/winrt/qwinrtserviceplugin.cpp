/****************************************************************************
**
** Copyright (C) 2014 Digia Plc and/or its subsidiary(-ies).
** Contact: http://www.qt-project.org/legal
**
** This file is part of the Qt Toolkit.
**
** $QT_BEGIN_LICENSE:LGPL$
** Commercial License Usage
** Licensees holding valid commercial Qt licenses may use this file in
** accordance with the commercial license agreement provided with the
** Software or, alternatively, in accordance with the terms contained in
** a written agreement between you and Digia.  For licensing terms and
** conditions see http://qt.digia.com/licensing.  For further information
** use the contact form at http://qt.digia.com/contact-us.
**
** GNU Lesser General Public License Usage
** Alternatively, this file may be used under the terms of the GNU Lesser
** General Public License version 2.1 as published by the Free Software
** Foundation and appearing in the file LICENSE.LGPL included in the
** packaging of this file.  Please review the following information to
** ensure the GNU Lesser General Public License version 2.1 requirements
** will be met: http://www.gnu.org/licenses/old-licenses/lgpl-2.1.html.
**
** In addition, as a special exception, Digia gives you certain additional
** rights.  These rights are described in the Digia Qt LGPL Exception
** version 1.1, included in the file LGPL_EXCEPTION.txt in this package.
**
** GNU General Public License Usage
** Alternatively, this file may be used under the terms of the GNU
** General Public License version 3.0 as published by the Free Software
** Foundation and appearing in the file LICENSE.GPL included in the
** packaging of this file.  Please review the following information to
** ensure the GNU General Public License version 3.0 requirements will be
** met: http://www.gnu.org/copyleft/gpl.html.
**
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

QT_USE_NAMESPACE

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
