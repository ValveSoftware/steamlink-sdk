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

#include "qgstreamercaptureservice.h"
#include "qgstreamercapturesession.h"
#include "qgstreamerrecordercontrol.h"
#include "qgstreamermediacontainercontrol.h"
#include "qgstreameraudioencode.h"
#include "qgstreamervideoencode.h"
#include "qgstreamerimageencode.h"
#include "qgstreamercameracontrol.h"
#include <private/qgstreamerbushelper_p.h>
#include "qgstreamercapturemetadatacontrol.h"

#if defined(USE_GSTREAMER_CAMERA)
#include "qgstreamerv4l2input.h"
#endif

#include "qgstreamerimagecapturecontrol.h"
#include <private/qgstreameraudioinputselector_p.h>
#include <private/qgstreamervideoinputdevicecontrol_p.h>
#include <private/qgstreameraudioprobecontrol_p.h>

#include <private/qgstreamervideorenderer_p.h>
#include <private/qgstreamervideowindow_p.h>

#if defined(HAVE_WIDGETS)
#include <private/qgstreamervideowidget_p.h>
#endif

#include <qmediaserviceproviderplugin.h>

QT_BEGIN_NAMESPACE

QGstreamerCaptureService::QGstreamerCaptureService(const QString &service, QObject *parent):
    QMediaService(parent)
{
    m_captureSession = 0;
    m_cameraControl = 0;
    m_metaDataControl = 0;

#if defined(USE_GSTREAMER_CAMERA)
    m_videoInput = 0;
#endif
    m_audioInputSelector = 0;
    m_videoInputDevice = 0;

    m_videoOutput = 0;
    m_videoRenderer = 0;
    m_videoWindow = 0;
#if defined(HAVE_WIDGETS)
    m_videoWidgetControl = 0;
#endif
    m_imageCaptureControl = 0;

    if (service == Q_MEDIASERVICE_AUDIOSOURCE) {
        m_captureSession = new QGstreamerCaptureSession(QGstreamerCaptureSession::Audio, this);
    }

#if defined(USE_GSTREAMER_CAMERA)
   if (service == Q_MEDIASERVICE_CAMERA) {
        m_captureSession = new QGstreamerCaptureSession(QGstreamerCaptureSession::AudioAndVideo, this);
        m_cameraControl = new QGstreamerCameraControl(m_captureSession);
        m_videoInput = new QGstreamerV4L2Input(this);
        m_captureSession->setVideoInput(m_videoInput);
        m_videoInputDevice = new QGstreamerVideoInputDeviceControl(this);

        connect(m_videoInputDevice, SIGNAL(selectedDeviceChanged(QString)),
                m_videoInput, SLOT(setDevice(QString)));

        if (m_videoInputDevice->deviceCount())
            m_videoInput->setDevice(m_videoInputDevice->deviceName(m_videoInputDevice->selectedDevice()));

        m_videoRenderer = new QGstreamerVideoRenderer(this);
        m_videoWindow = new QGstreamerVideoWindow(this);

#if defined(HAVE_WIDGETS)
        m_videoWidgetControl = new QGstreamerVideoWidgetControl(this);
#endif
        m_imageCaptureControl = new QGstreamerImageCaptureControl(m_captureSession);
    }
#endif

    m_audioInputSelector = new QGstreamerAudioInputSelector(this);
    connect(m_audioInputSelector, SIGNAL(activeInputChanged(QString)), m_captureSession, SLOT(setCaptureDevice(QString)));

    if (m_captureSession && m_audioInputSelector->availableInputs().size() > 0)
        m_captureSession->setCaptureDevice(m_audioInputSelector->defaultInput());

    m_metaDataControl = new QGstreamerCaptureMetaDataControl(this);
    connect(m_metaDataControl, SIGNAL(metaDataChanged(QMap<QByteArray,QVariant>)),
            m_captureSession, SLOT(setMetaData(QMap<QByteArray,QVariant>)));
}

QGstreamerCaptureService::~QGstreamerCaptureService()
{
}

QMediaControl *QGstreamerCaptureService::requestControl(const char *name)
{
    if (!m_captureSession)
        return 0;

    if (qstrcmp(name,QAudioInputSelectorControl_iid) == 0)
        return m_audioInputSelector;

    if (qstrcmp(name,QVideoDeviceSelectorControl_iid) == 0)
        return m_videoInputDevice;

    if (qstrcmp(name,QMediaRecorderControl_iid) == 0)
        return m_captureSession->recorderControl();

    if (qstrcmp(name,QAudioEncoderSettingsControl_iid) == 0)
        return m_captureSession->audioEncodeControl();

    if (qstrcmp(name,QVideoEncoderSettingsControl_iid) == 0)
        return m_captureSession->videoEncodeControl();

    if (qstrcmp(name,QImageEncoderControl_iid) == 0)
        return m_captureSession->imageEncodeControl();


    if (qstrcmp(name,QMediaContainerControl_iid) == 0)
        return m_captureSession->mediaContainerControl();

    if (qstrcmp(name,QCameraControl_iid) == 0)
        return m_cameraControl;

    if (qstrcmp(name,QMetaDataWriterControl_iid) == 0)
        return m_metaDataControl;

    if (qstrcmp(name, QCameraImageCaptureControl_iid) == 0)
        return m_imageCaptureControl;

    if (qstrcmp(name,QMediaAudioProbeControl_iid) == 0) {
        if (m_captureSession) {
            QGstreamerAudioProbeControl *probe = new QGstreamerAudioProbeControl(this);
            m_captureSession->addProbe(probe);
            return probe;
        }
        return 0;
    }

    if (!m_videoOutput) {
        if (qstrcmp(name, QVideoRendererControl_iid) == 0) {
            m_videoOutput = m_videoRenderer;
        } else if (qstrcmp(name, QVideoWindowControl_iid) == 0) {
            m_videoOutput = m_videoWindow;
        }
#if defined(HAVE_WIDGETS)
        else if (qstrcmp(name, QVideoWidgetControl_iid) == 0) {
            m_videoOutput = m_videoWidgetControl;
        }
#endif

        if (m_videoOutput) {
            m_captureSession->setVideoPreview(m_videoOutput);
            return m_videoOutput;
        }
    }

    return 0;
}

void QGstreamerCaptureService::releaseControl(QMediaControl *control)
{
    if (control && control == m_videoOutput) {
        m_videoOutput = 0;
        m_captureSession->setVideoPreview(0);
    }

    QGstreamerAudioProbeControl* audioProbe = qobject_cast<QGstreamerAudioProbeControl*>(control);
    if (audioProbe) {
        if (m_captureSession)
            m_captureSession->removeProbe(audioProbe);
        delete audioProbe;
        return;
    }
}

QT_END_NAMESPACE
