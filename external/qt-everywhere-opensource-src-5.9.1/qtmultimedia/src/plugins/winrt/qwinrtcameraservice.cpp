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

#include "qwinrtcameraservice.h"
#include "qwinrtcameracontrol.h"
#include "qwinrtcamerainfocontrol.h"
#include "qwinrtvideoprobecontrol.h"
#include "qwinrtcameravideorenderercontrol.h"

#include <QtCore/QCoreApplication>
#include <QtCore/qfunctions_winrt.h>
#include <QtCore/QPointer>
#include <QtMultimedia/QCameraImageCaptureControl>
#include <QtMultimedia/QVideoRendererControl>
#include <QtMultimedia/QVideoDeviceSelectorControl>
#include <QtMultimedia/QImageEncoderControl>
#include <QtMultimedia/QCameraFlashControl>
#include <QtMultimedia/QCameraFocusControl>
#include <QtMultimedia/QCameraLocksControl>
#include <QtMultimedia/QMediaVideoProbeControl>

QT_BEGIN_NAMESPACE

class QWinRTCameraServicePrivate
{
public:
    QPointer<QWinRTCameraControl> cameraControl;
    QPointer<QWinRTCameraInfoControl> cameraInfoControl;
};

QWinRTCameraService::QWinRTCameraService(QObject *parent)
    : QMediaService(parent), d_ptr(new QWinRTCameraServicePrivate)
{
}

QMediaControl *QWinRTCameraService::requestControl(const char *name)
{
    Q_D(QWinRTCameraService);

    if (qstrcmp(name, QCameraControl_iid) == 0) {
        if (!d->cameraControl)
            d->cameraControl = new QWinRTCameraControl(this);
        return d->cameraControl;
    }
    if (qstrcmp(name, QCameraInfoControl_iid) == 0) {
        if (!d->cameraInfoControl)
            d->cameraInfoControl = new QWinRTCameraInfoControl(this);
        return d->cameraInfoControl;
    }

    if (!d->cameraControl)
        return nullptr;

    if (qstrcmp(name, QVideoRendererControl_iid) == 0)
        return d->cameraControl->videoRenderer();

    if (qstrcmp(name, QVideoDeviceSelectorControl_iid) == 0)
       return d->cameraControl->videoDeviceSelector();

    if (qstrcmp(name, QCameraImageCaptureControl_iid) == 0)
        return d->cameraControl->imageCaptureControl();

    if (qstrcmp(name, QImageEncoderControl_iid) == 0)
        return d->cameraControl->imageEncoderControl();

    if (qstrcmp(name, QCameraFlashControl_iid) == 0)
        return d->cameraControl->cameraFlashControl();

    if (qstrcmp(name, QCameraFocusControl_iid) == 0)
        return d->cameraControl->cameraFocusControl();

    if (qstrcmp(name, QCameraLocksControl_iid) == 0)
        return d->cameraControl->cameraLocksControl();

    if (qstrcmp(name, QMediaVideoProbeControl_iid) == 0)
        return new QWinRTVideoProbeControl(qobject_cast<QWinRTCameraVideoRendererControl *>(d->cameraControl->videoRenderer()));

    return nullptr;
}

void QWinRTCameraService::releaseControl(QMediaControl *control)
{
    Q_ASSERT(control);
    if (QWinRTVideoProbeControl *videoProbe = qobject_cast<QWinRTVideoProbeControl *>(control))
        videoProbe->deleteLater();
}

QT_END_NAMESPACE
