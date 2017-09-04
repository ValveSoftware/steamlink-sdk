/****************************************************************************
**
** Copyright (C) 2016 The Qt Company Ltd.
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

#ifndef QANDROIDMEDIASERVICEPLUGIN_H
#define QANDROIDMEDIASERVICEPLUGIN_H

#include <QMediaServiceProviderPlugin>

QT_BEGIN_NAMESPACE

class QAndroidMediaServicePlugin
        : public QMediaServiceProviderPlugin
        , public QMediaServiceSupportedDevicesInterface
        , public QMediaServiceDefaultDeviceInterface
        , public QMediaServiceCameraInfoInterface
        , public QMediaServiceFeaturesInterface
{
    Q_OBJECT
    Q_INTERFACES(QMediaServiceSupportedDevicesInterface)
    Q_INTERFACES(QMediaServiceDefaultDeviceInterface)
    Q_INTERFACES(QMediaServiceCameraInfoInterface)
    Q_INTERFACES(QMediaServiceFeaturesInterface)
    Q_PLUGIN_METADATA(IID "org.qt-project.qt.mediaserviceproviderfactory/5.0"
                      FILE "android_mediaservice.json")

public:
    QAndroidMediaServicePlugin();
    ~QAndroidMediaServicePlugin();

    QMediaService* create(QString const& key) Q_DECL_OVERRIDE;
    void release(QMediaService *service) Q_DECL_OVERRIDE;

    QMediaServiceProviderHint::Features supportedFeatures(const QByteArray &service) const Q_DECL_OVERRIDE;

    QByteArray defaultDevice(const QByteArray &service) const Q_DECL_OVERRIDE;
    QList<QByteArray> devices(const QByteArray &service) const Q_DECL_OVERRIDE;
    QString deviceDescription(const QByteArray &service, const QByteArray &device) Q_DECL_OVERRIDE;

    QCamera::Position cameraPosition(const QByteArray &device) const Q_DECL_OVERRIDE;
    int cameraOrientation(const QByteArray &device) const Q_DECL_OVERRIDE;
};

QT_END_NAMESPACE

#endif // QANDROIDMEDIASERVICEPLUGIN_H
