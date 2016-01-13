/****************************************************************************
**
** Copyright (C) 2013 Research In Motion
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
#ifndef BBCAMERASERVICE_H
#define BBCAMERASERVICE_H

#include <QObject>

#include <qmediaservice.h>

QT_BEGIN_NAMESPACE

class BbCameraAudioEncoderSettingsControl;
class BbCameraCaptureBufferFormatControl;
class BbCameraCaptureDestinationControl;
class BbCameraControl;
class BbCameraExposureControl;
class BbCameraFlashControl;
class BbCameraFocusControl;
class BbCameraImageCaptureControl;
class BbCameraImageProcessingControl;
class BbCameraInfoControl;
class BbCameraLocksControl;
class BbCameraMediaRecorderControl;
class BbCameraSession;
class BbCameraVideoEncoderSettingsControl;
class BbCameraViewfinderSettingsControl;
class BbCameraZoomControl;
class BbImageEncoderControl;
class BbVideoDeviceSelectorControl;
class BbVideoRendererControl;

class BbCameraService : public QMediaService
{
    Q_OBJECT

public:
    explicit BbCameraService(QObject *parent = 0);
    ~BbCameraService();

    virtual QMediaControl* requestControl(const char *name);
    virtual void releaseControl(QMediaControl *control);

private:
    BbCameraSession* m_cameraSession;

    BbCameraAudioEncoderSettingsControl* m_cameraAudioEncoderSettingsControl;
    BbCameraCaptureBufferFormatControl* m_cameraCaptureBufferFormatControl;
    BbCameraCaptureDestinationControl* m_cameraCaptureDestinationControl;
    BbCameraControl* m_cameraControl;
    BbCameraExposureControl* m_cameraExposureControl;
    BbCameraFlashControl* m_cameraFlashControl;
    BbCameraFocusControl* m_cameraFocusControl;
    BbCameraImageCaptureControl* m_cameraImageCaptureControl;
    BbCameraImageProcessingControl* m_cameraImageProcessingControl;
    BbCameraInfoControl* m_cameraInfoControl;
    BbCameraLocksControl* m_cameraLocksControl;
    BbCameraMediaRecorderControl* m_cameraMediaRecorderControl;
    BbCameraVideoEncoderSettingsControl* m_cameraVideoEncoderSettingsControl;
    BbCameraViewfinderSettingsControl* m_cameraViewfinderSettingsControl;
    BbCameraZoomControl* m_cameraZoomControl;
    BbImageEncoderControl* m_imageEncoderControl;
    BbVideoDeviceSelectorControl* m_videoDeviceSelectorControl;
    BbVideoRendererControl* m_videoRendererControl;
};

QT_END_NAMESPACE

#endif
