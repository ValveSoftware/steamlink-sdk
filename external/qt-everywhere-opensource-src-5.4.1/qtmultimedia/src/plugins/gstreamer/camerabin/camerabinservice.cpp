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

#include "camerabinservice.h"
#include "camerabinsession.h"
#include "camerabinrecorder.h"
#include "camerabincontainer.h"
#include "camerabinaudioencoder.h"
#include "camerabinvideoencoder.h"
#include "camerabinimageencoder.h"
#include "camerabincontrol.h"
#include "camerabinmetadata.h"
#include "camerabininfocontrol.h"

#ifdef HAVE_GST_PHOTOGRAPHY
#include "camerabinexposure.h"
#include "camerabinflash.h"
#include "camerabinfocus.h"
#include "camerabinlocks.h"
#include "camerabinzoom.h"
#endif

#include "camerabinimagecapture.h"
#include "camerabinimageprocessing.h"
#include "camerabincapturebufferformat.h"
#include "camerabincapturedestination.h"
#include "camerabinviewfindersettings.h"
#include <private/qgstreamerbushelper_p.h>

#include <private/qgstreameraudioinputselector_p.h>
#include <private/qgstreamervideoinputdevicecontrol_p.h>


#if defined(HAVE_WIDGETS)
#include <private/qgstreamervideowidget_p.h>
#endif
#include <private/qgstreamervideowindow_p.h>
#include <private/qgstreamervideorenderer_p.h>

#if defined(Q_WS_MAEMO_6) && defined(__arm__)
#include "qgstreamergltexturerenderer.h"
#endif

#include <private/qmediaserviceprovider_p.h>

#include <QtCore/qdebug.h>
#include <QtCore/qprocess.h>

#if defined(Q_WS_MAEMO_6)
#include "camerabuttonlistener_meego.h"
#endif

QT_BEGIN_NAMESPACE

CameraBinService::CameraBinService(GstElementFactory *sourceFactory, QObject *parent):
    QMediaService(parent),
    m_cameraInfoControl(0)
{
    m_captureSession = 0;
    m_metaDataControl = 0;

    m_audioInputSelector = 0;
    m_videoInputDevice = 0;

    m_videoOutput = 0;
    m_videoRenderer = 0;
    m_videoWindow = 0;
#if defined(HAVE_WIDGETS)
    m_videoWidgetControl = 0;
#endif
    m_imageCaptureControl = 0;

    m_captureSession = new CameraBinSession(sourceFactory, this);
    m_videoInputDevice = new QGstreamerVideoInputDeviceControl(sourceFactory, m_captureSession);
    m_imageCaptureControl = new CameraBinImageCapture(m_captureSession);

    connect(m_videoInputDevice, SIGNAL(selectedDeviceChanged(QString)),
            m_captureSession, SLOT(setDevice(QString)));

    if (m_videoInputDevice->deviceCount())
        m_captureSession->setDevice(m_videoInputDevice->deviceName(m_videoInputDevice->selectedDevice()));

#if defined(Q_WS_MAEMO_6) && defined(__arm__) && defined(HAVE_WIDGETS)
    m_videoRenderer = new QGstreamerGLTextureRenderer(this);
#else
    m_videoRenderer = new QGstreamerVideoRenderer(this);
#endif

#ifdef Q_WS_MAEMO_6
    m_videoWindow = new QGstreamerVideoWindow(this, "omapxvsink");
#else
    m_videoWindow = new QGstreamerVideoWindow(this);
#endif

#if defined(HAVE_WIDGETS)
    m_videoWidgetControl = new QGstreamerVideoWidgetControl(this);
#endif

    m_audioInputSelector = new QGstreamerAudioInputSelector(this);
    connect(m_audioInputSelector, SIGNAL(activeInputChanged(QString)), m_captureSession, SLOT(setCaptureDevice(QString)));

    if (m_captureSession && m_audioInputSelector->availableInputs().size() > 0)
        m_captureSession->setCaptureDevice(m_audioInputSelector->defaultInput());

    m_metaDataControl = new CameraBinMetaData(this);
    connect(m_metaDataControl, SIGNAL(metaDataChanged(QMap<QByteArray,QVariant>)),
            m_captureSession, SLOT(setMetaData(QMap<QByteArray,QVariant>)));

#if defined(Q_WS_MAEMO_6)
    new CameraButtonListener(this);
#endif
}

CameraBinService::~CameraBinService()
{
}

QMediaControl *CameraBinService::requestControl(const char *name)
{
    if (!m_captureSession)
        return 0;

    //qDebug() << "Request control" << name;

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
            m_captureSession->setViewfinder(m_videoOutput);
            return m_videoOutput;
        }
    }

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
        return m_captureSession->cameraControl();

    if (qstrcmp(name,QMetaDataWriterControl_iid) == 0)
        return m_metaDataControl;

    if (qstrcmp(name, QCameraImageCaptureControl_iid) == 0)
        return m_imageCaptureControl;

#ifdef HAVE_GST_PHOTOGRAPHY
    if (qstrcmp(name, QCameraExposureControl_iid) == 0)
        return m_captureSession->cameraExposureControl();

    if (qstrcmp(name, QCameraFlashControl_iid) == 0)
        return m_captureSession->cameraFlashControl();

    if (qstrcmp(name, QCameraFocusControl_iid) == 0)
        return m_captureSession->cameraFocusControl();

    if (qstrcmp(name, QCameraLocksControl_iid) == 0)
        return m_captureSession->cameraLocksControl();

    if (qstrcmp(name, QCameraZoomControl_iid) == 0)
        return m_captureSession->cameraZoomControl();
#endif

    if (qstrcmp(name, QCameraImageProcessingControl_iid) == 0)
        return m_captureSession->imageProcessingControl();

    if (qstrcmp(name, QCameraCaptureDestinationControl_iid) == 0)
        return m_captureSession->captureDestinationControl();

    if (qstrcmp(name, QCameraCaptureBufferFormatControl_iid) == 0)
        return m_captureSession->captureBufferFormatControl();

    if (qstrcmp(name, QCameraViewfinderSettingsControl_iid) == 0)
        return m_captureSession->viewfinderSettingsControl();

    if (qstrcmp(name, QCameraInfoControl_iid) == 0) {
        if (!m_cameraInfoControl)
            m_cameraInfoControl = new CameraBinInfoControl(m_captureSession->sourceFactory(), this);
        return m_cameraInfoControl;
    }

    return 0;
}

void CameraBinService::releaseControl(QMediaControl *control)
{
    if (control && control == m_videoOutput) {
        m_videoOutput = 0;
        m_captureSession->setViewfinder(0);
    }
}

bool CameraBinService::isCameraBinAvailable()
{
    GstElementFactory *factory = gst_element_factory_find("camerabin2");
    if (factory) {
        gst_object_unref(GST_OBJECT(factory));
        return true;
    }

    return false;
}

QT_END_NAMESPACE
