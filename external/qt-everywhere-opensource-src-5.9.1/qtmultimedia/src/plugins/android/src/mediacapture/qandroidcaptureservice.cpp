/****************************************************************************
**
** Copyright (C) 2016 The Qt Company Ltd.
** Copyright (C) 2016 Ruslan Baratov
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

#include "qandroidcaptureservice.h"

#include "qandroidmediarecordercontrol.h"
#include "qandroidcapturesession.h"
#include "qandroidcameracontrol.h"
#include "qandroidcamerainfocontrol.h"
#include "qandroidvideodeviceselectorcontrol.h"
#include "qandroidaudioinputselectorcontrol.h"
#include "qandroidcamerasession.h"
#include "qandroidcameravideorenderercontrol.h"
#include "qandroidcamerazoomcontrol.h"
#include "qandroidcameraexposurecontrol.h"
#include "qandroidcameraflashcontrol.h"
#include "qandroidcamerafocuscontrol.h"
#include "qandroidviewfindersettingscontrol.h"
#include "qandroidcameralockscontrol.h"
#include "qandroidcameraimageprocessingcontrol.h"
#include "qandroidimageencodercontrol.h"
#include "qandroidcameraimagecapturecontrol.h"
#include "qandroidcameracapturedestinationcontrol.h"
#include "qandroidcameracapturebufferformatcontrol.h"
#include "qandroidaudioencodersettingscontrol.h"
#include "qandroidvideoencodersettingscontrol.h"
#include "qandroidmediacontainercontrol.h"
#include "qandroidmediavideoprobecontrol.h"

#include <qmediaserviceproviderplugin.h>

QT_BEGIN_NAMESPACE

QAndroidCaptureService::QAndroidCaptureService(const QString &service, QObject *parent)
    : QMediaService(parent)
    , m_service(service)
    , m_videoRendererControl(0)
{
    if (m_service == QLatin1String(Q_MEDIASERVICE_CAMERA)) {
        m_cameraSession = new QAndroidCameraSession;
        m_cameraControl = new QAndroidCameraControl(m_cameraSession);
        m_cameraInfoControl = new QAndroidCameraInfoControl;
        m_videoInputControl = new QAndroidVideoDeviceSelectorControl(m_cameraSession);
        m_cameraZoomControl = new QAndroidCameraZoomControl(m_cameraSession);
        m_cameraExposureControl = new QAndroidCameraExposureControl(m_cameraSession);
        m_cameraFlashControl = new QAndroidCameraFlashControl(m_cameraSession);
        m_cameraFocusControl = new QAndroidCameraFocusControl(m_cameraSession);
        m_viewfinderSettingsControl2 = new QAndroidViewfinderSettingsControl2(m_cameraSession);
        m_cameraLocksControl = new QAndroidCameraLocksControl(m_cameraSession);
        m_cameraImageProcessingControl = new QAndroidCameraImageProcessingControl(m_cameraSession);
        m_imageEncoderControl = new QAndroidImageEncoderControl(m_cameraSession);
        m_imageCaptureControl = new QAndroidCameraImageCaptureControl(m_cameraSession);
        m_captureDestinationControl = new QAndroidCameraCaptureDestinationControl(m_cameraSession);
        m_captureBufferFormatControl = new QAndroidCameraCaptureBufferFormatControl;
        m_audioInputControl = 0;
    } else {
        m_cameraSession = 0;
        m_cameraControl = 0;
        m_cameraInfoControl = 0;
        m_videoInputControl = 0;
        m_cameraZoomControl = 0;
        m_cameraExposureControl = 0;
        m_cameraFlashControl = 0;
        m_cameraFocusControl = 0;
        m_viewfinderSettingsControl2 = 0;
        m_cameraLocksControl = 0;
        m_cameraImageProcessingControl = 0;
        m_imageEncoderControl = 0;
        m_imageCaptureControl = 0;
        m_captureDestinationControl = 0;
        m_captureBufferFormatControl = 0;
        m_videoEncoderSettingsControl = 0;
    }

    m_captureSession = new QAndroidCaptureSession(m_cameraSession);
    m_recorderControl = new QAndroidMediaRecorderControl(m_captureSession);
    m_audioEncoderSettingsControl = new QAndroidAudioEncoderSettingsControl(m_captureSession);
    m_mediaContainerControl = new QAndroidMediaContainerControl(m_captureSession);

    if (m_service == QLatin1String(Q_MEDIASERVICE_CAMERA)) {
        m_videoEncoderSettingsControl = new QAndroidVideoEncoderSettingsControl(m_captureSession);
    } else {
        m_audioInputControl = new QAndroidAudioInputSelectorControl(m_captureSession);
        m_captureSession->setAudioInput(m_audioInputControl->defaultInput());
    }
}

QAndroidCaptureService::~QAndroidCaptureService()
{
    delete m_audioEncoderSettingsControl;
    delete m_videoEncoderSettingsControl;
    delete m_mediaContainerControl;
    delete m_recorderControl;
    delete m_captureSession;
    delete m_cameraControl;
    delete m_cameraInfoControl;
    delete m_audioInputControl;
    delete m_videoInputControl;
    delete m_videoRendererControl;
    delete m_cameraZoomControl;
    delete m_cameraExposureControl;
    delete m_cameraFlashControl;
    delete m_cameraFocusControl;
    delete m_viewfinderSettingsControl2;
    delete m_cameraLocksControl;
    delete m_cameraImageProcessingControl;
    delete m_imageEncoderControl;
    delete m_imageCaptureControl;
    delete m_captureDestinationControl;
    delete m_captureBufferFormatControl;
    delete m_cameraSession;
}

QMediaControl *QAndroidCaptureService::requestControl(const char *name)
{
    if (qstrcmp(name, QMediaRecorderControl_iid) == 0)
        return m_recorderControl;

    if (qstrcmp(name, QMediaContainerControl_iid) == 0)
        return m_mediaContainerControl;

    if (qstrcmp(name, QAudioEncoderSettingsControl_iid) == 0)
        return m_audioEncoderSettingsControl;

    if (qstrcmp(name, QVideoEncoderSettingsControl_iid) == 0)
        return m_videoEncoderSettingsControl;

    if (qstrcmp(name, QCameraControl_iid) == 0)
        return m_cameraControl;

    if (qstrcmp(name, QCameraInfoControl_iid) == 0)
        return m_cameraInfoControl;

    if (qstrcmp(name, QAudioInputSelectorControl_iid) == 0)
        return m_audioInputControl;

    if (qstrcmp(name, QVideoDeviceSelectorControl_iid) == 0)
        return m_videoInputControl;

    if (qstrcmp(name, QCameraZoomControl_iid) == 0)
        return m_cameraZoomControl;

    if (qstrcmp(name, QCameraExposureControl_iid) == 0)
        return m_cameraExposureControl;

    if (qstrcmp(name, QCameraFlashControl_iid) == 0)
        return m_cameraFlashControl;

    if (qstrcmp(name, QCameraFocusControl_iid) == 0)
        return m_cameraFocusControl;

    if (qstrcmp(name, QCameraViewfinderSettingsControl2_iid) == 0)
        return m_viewfinderSettingsControl2;

    if (qstrcmp(name, QCameraLocksControl_iid) == 0)
        return m_cameraLocksControl;

    if (qstrcmp(name, QCameraImageProcessingControl_iid) == 0)
        return m_cameraImageProcessingControl;

    if (qstrcmp(name, QImageEncoderControl_iid) == 0)
        return m_imageEncoderControl;

    if (qstrcmp(name, QCameraImageCaptureControl_iid) == 0)
        return m_imageCaptureControl;

    if (qstrcmp(name, QCameraCaptureDestinationControl_iid) == 0)
        return m_captureDestinationControl;

    if (qstrcmp(name, QCameraCaptureBufferFormatControl_iid) == 0)
        return m_captureBufferFormatControl;

    if (qstrcmp(name, QVideoRendererControl_iid) == 0
            && m_service == QLatin1String(Q_MEDIASERVICE_CAMERA)
            && !m_videoRendererControl) {
        m_videoRendererControl = new QAndroidCameraVideoRendererControl(m_cameraSession);
        return m_videoRendererControl;
    }

    if (qstrcmp(name,QMediaVideoProbeControl_iid) == 0) {
        QAndroidMediaVideoProbeControl *videoProbe = 0;
        if (m_cameraSession) {
            videoProbe = new QAndroidMediaVideoProbeControl(this);
            m_cameraSession->addProbe(videoProbe);
        }
        return videoProbe;
    }

    return 0;
}

void QAndroidCaptureService::releaseControl(QMediaControl *control)
{
    if (control) {
        if (control == m_videoRendererControl) {
            delete m_videoRendererControl;
            m_videoRendererControl = 0;
            return;
        }

        QAndroidMediaVideoProbeControl *videoProbe = qobject_cast<QAndroidMediaVideoProbeControl *>(control);
        if (videoProbe) {
            if (m_cameraSession)
                m_cameraSession->removeProbe(videoProbe);
            delete videoProbe;
            return;
        }
    }

}

QT_END_NAMESPACE
