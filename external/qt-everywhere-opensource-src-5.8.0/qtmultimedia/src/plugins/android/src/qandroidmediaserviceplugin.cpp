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

#include "qandroidmediaserviceplugin.h"

#include "qandroidmediaservice.h"
#include "qandroidcaptureservice.h"
#include "qandroidaudioinputselectorcontrol.h"
#include "qandroidcamerainfocontrol.h"
#include "qandroidcamerasession.h"
#include "androidmediaplayer.h"
#include "androidsurfacetexture.h"
#include "androidcamera.h"
#include "androidmultimediautils.h"
#include "androidmediarecorder.h"
#include "androidsurfaceview.h"
#include <qdebug.h>

QT_BEGIN_NAMESPACE

QAndroidMediaServicePlugin::QAndroidMediaServicePlugin()
{
}

QAndroidMediaServicePlugin::~QAndroidMediaServicePlugin()
{
}

QMediaService *QAndroidMediaServicePlugin::create(const QString &key)
{
    if (key == QLatin1String(Q_MEDIASERVICE_MEDIAPLAYER))
        return new QAndroidMediaService;

    if (key == QLatin1String(Q_MEDIASERVICE_CAMERA)
            || key == QLatin1String(Q_MEDIASERVICE_AUDIOSOURCE)) {
        return new QAndroidCaptureService(key);
    }

    qWarning() << "Android service plugin: unsupported key:" << key;
    return 0;
}

void QAndroidMediaServicePlugin::release(QMediaService *service)
{
    delete service;
}

QMediaServiceProviderHint::Features QAndroidMediaServicePlugin::supportedFeatures(const QByteArray &service) const
{
    if (service == Q_MEDIASERVICE_MEDIAPLAYER)
        return QMediaServiceProviderHint::VideoSurface;

    if (service == Q_MEDIASERVICE_CAMERA)
        return QMediaServiceProviderHint::VideoSurface | QMediaServiceProviderHint::RecordingSupport;

    if (service == Q_MEDIASERVICE_AUDIOSOURCE)
        return QMediaServiceProviderHint::RecordingSupport;

    return QMediaServiceProviderHint::Features();
}

QByteArray QAndroidMediaServicePlugin::defaultDevice(const QByteArray &service) const
{
    if (service == Q_MEDIASERVICE_CAMERA && !QAndroidCameraSession::availableCameras().isEmpty())
        return QAndroidCameraSession::availableCameras().first().name;

    return QByteArray();
}

QList<QByteArray> QAndroidMediaServicePlugin::devices(const QByteArray &service) const
{
    if (service == Q_MEDIASERVICE_CAMERA) {
        QList<QByteArray> devices;
        const QList<AndroidCameraInfo> &cameras = QAndroidCameraSession::availableCameras();
        for (int i = 0; i < cameras.count(); ++i)
            devices.append(cameras.at(i).name);
        return devices;
    }

    if (service == Q_MEDIASERVICE_AUDIOSOURCE)
        return QAndroidAudioInputSelectorControl::availableDevices();

    return QList<QByteArray>();
}

QString QAndroidMediaServicePlugin::deviceDescription(const QByteArray &service, const QByteArray &device)
{
    if (service == Q_MEDIASERVICE_CAMERA) {
        const QList<AndroidCameraInfo> &cameras = QAndroidCameraSession::availableCameras();
        for (int i = 0; i < cameras.count(); ++i) {
            const AndroidCameraInfo &info = cameras.at(i);
            if (info.name == device)
                return info.description;
        }
    }

    if (service == Q_MEDIASERVICE_AUDIOSOURCE)
        return QAndroidAudioInputSelectorControl::availableDeviceDescription(device);

    return QString();
}

QCamera::Position QAndroidMediaServicePlugin::cameraPosition(const QByteArray &device) const
{
    return QAndroidCameraInfoControl::position(device);
}

int QAndroidMediaServicePlugin::cameraOrientation(const QByteArray &device) const
{
    return QAndroidCameraInfoControl::orientation(device);
}

QT_END_NAMESPACE

Q_DECL_EXPORT jint JNICALL JNI_OnLoad(JavaVM *vm, void * /*reserved*/)
{
    static bool initialized = false;
    if (initialized)
        return JNI_VERSION_1_6;
    initialized = true;

    QT_USE_NAMESPACE
    typedef union {
        JNIEnv *nativeEnvironment;
        void *venv;
    } UnionJNIEnvToVoid;

    UnionJNIEnvToVoid uenv;
    uenv.venv = NULL;

    if (vm->GetEnv(&uenv.venv, JNI_VERSION_1_4) != JNI_OK)
        return JNI_ERR;

    JNIEnv *jniEnv = uenv.nativeEnvironment;

    if (!AndroidMediaPlayer::initJNI(jniEnv) ||
        !AndroidCamera::initJNI(jniEnv) ||
        !AndroidMediaRecorder::initJNI(jniEnv) ||
        !AndroidSurfaceHolder::initJNI(jniEnv)) {
        return JNI_ERR;
    }

    AndroidSurfaceTexture::initJNI(jniEnv);

    return JNI_VERSION_1_4;
}
