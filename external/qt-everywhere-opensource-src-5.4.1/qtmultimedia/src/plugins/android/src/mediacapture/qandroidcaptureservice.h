/****************************************************************************
**
** Copyright (C) 2014 Digia Plc and/or its subsidiary(-ies).
** Contact: http://www.qt-project.org/legal
**
** This file is part of the Qt Toolkit.
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

#ifndef QANDROIDCAPTURESERVICE_H
#define QANDROIDCAPTURESERVICE_H

#include <qmediaservice.h>
#include <qmediacontrol.h>

QT_BEGIN_NAMESPACE

class QAndroidMediaRecorderControl;
class QAndroidCaptureSession;
class QAndroidCameraControl;
class QAndroidCameraInfoControl;
class QAndroidVideoDeviceSelectorControl;
class QAndroidAudioInputSelectorControl;
class QAndroidCameraSession;
class QAndroidVideoRendererControl;
class QAndroidCameraZoomControl;
class QAndroidCameraExposureControl;
class QAndroidCameraFlashControl;
class QAndroidCameraFocusControl;
class QAndroidCameraLocksControl;
class QAndroidCameraImageProcessingControl;
class QAndroidImageEncoderControl;
class QAndroidCameraImageCaptureControl;
class QAndroidCameraCaptureDestinationControl;
class QAndroidCameraCaptureBufferFormatControl;
class QAndroidAudioEncoderSettingsControl;
class QAndroidVideoEncoderSettingsControl;
class QAndroidMediaContainerControl;

class QAndroidCaptureService : public QMediaService
{
    Q_OBJECT

public:
    explicit QAndroidCaptureService(const QString &service, QObject *parent = 0);
    virtual ~QAndroidCaptureService();

    QMediaControl *requestControl(const char *name);
    void releaseControl(QMediaControl *);

private:
    QString m_service;

    QAndroidMediaRecorderControl *m_recorderControl;
    QAndroidCaptureSession *m_captureSession;
    QAndroidCameraControl *m_cameraControl;
    QAndroidCameraInfoControl *m_cameraInfoControl;
    QAndroidVideoDeviceSelectorControl *m_videoInputControl;
    QAndroidAudioInputSelectorControl *m_audioInputControl;
    QAndroidCameraSession *m_cameraSession;
    QMediaControl *m_videoRendererControl;
    QAndroidCameraZoomControl *m_cameraZoomControl;
    QAndroidCameraExposureControl *m_cameraExposureControl;
    QAndroidCameraFlashControl *m_cameraFlashControl;
    QAndroidCameraFocusControl *m_cameraFocusControl;
    QAndroidCameraLocksControl *m_cameraLocksControl;
    QAndroidCameraImageProcessingControl *m_cameraImageProcessingControl;
    QAndroidImageEncoderControl *m_imageEncoderControl;
    QAndroidCameraImageCaptureControl *m_imageCaptureControl;
    QAndroidCameraCaptureDestinationControl *m_captureDestinationControl;
    QAndroidCameraCaptureBufferFormatControl *m_captureBufferFormatControl;
    QAndroidAudioEncoderSettingsControl *m_audioEncoderSettingsControl;
    QAndroidVideoEncoderSettingsControl *m_videoEncoderSettingsControl;
    QAndroidMediaContainerControl *m_mediaContainerControl;
};

QT_END_NAMESPACE

#endif // QANDROIDCAPTURESERVICE_H
