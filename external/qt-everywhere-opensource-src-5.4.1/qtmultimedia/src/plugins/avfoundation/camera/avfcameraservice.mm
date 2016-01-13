/****************************************************************************
**
** Copyright (C) 2013 Digia Plc and/or its subsidiary(-ies).
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

#include <QtCore/qvariant.h>
#include <QtCore/qdebug.h>

#include "avfcameraservice.h"
#include "avfcameracontrol.h"
#include "avfcamerainfocontrol.h"
#include "avfcamerasession.h"
#include "avfvideodevicecontrol.h"
#include "avfaudioinputselectorcontrol.h"
#include "avfcamerametadatacontrol.h"
#include "avfmediarecordercontrol.h"
#include "avfimagecapturecontrol.h"
#include "avfvideorenderercontrol.h"
#include "avfmediarecordercontrol.h"
#include "avfimagecapturecontrol.h"

#include <private/qmediaplaylistnavigator_p.h>
#include <qmediaplaylist.h>

QT_USE_NAMESPACE

AVFCameraService::AVFCameraService(QObject *parent):
    QMediaService(parent),
    m_videoOutput(0)
{
    m_session = new AVFCameraSession(this);
    m_cameraControl = new AVFCameraControl(this);
    m_cameraInfoControl = new AVFCameraInfoControl(this);
    m_videoDeviceControl = new AVFVideoDeviceControl(this);
    m_audioInputSelectorControl = new AVFAudioInputSelectorControl(this);

    m_metaDataControl = new AVFCameraMetaDataControl(this);
    m_recorderControl = new AVFMediaRecorderControl(this);
    m_imageCaptureControl = new AVFImageCaptureControl(this);
}

AVFCameraService::~AVFCameraService()
{
    m_cameraControl->setState(QCamera::UnloadedState);

    if (m_videoOutput) {
        m_session->setVideoOutput(0);
        delete m_videoOutput;
        m_videoOutput = 0;
    }

    //delete controls before session,
    //so they have a chance to do deinitialization
    delete m_imageCaptureControl;
    //delete m_recorderControl;
    delete m_metaDataControl;
    delete m_cameraControl;

    delete m_session;
}

QMediaControl *AVFCameraService::requestControl(const char *name)
{
    if (qstrcmp(name, QCameraControl_iid) == 0)
        return m_cameraControl;

    if (qstrcmp(name, QCameraInfoControl_iid) == 0)
        return m_cameraInfoControl;

    if (qstrcmp(name, QVideoDeviceSelectorControl_iid) == 0)
        return m_videoDeviceControl;

    if (qstrcmp(name, QAudioInputSelectorControl_iid) == 0)
        return m_audioInputSelectorControl;

    //metadata support is not implemented yet
    //if (qstrcmp(name, QMetaDataWriterControl_iid) == 0)
    //    return m_metaDataControl;

    if (qstrcmp(name, QMediaRecorderControl_iid) == 0)
        return m_recorderControl;

    if (qstrcmp(name, QCameraImageCaptureControl_iid) == 0)
        return m_imageCaptureControl;

    if (!m_videoOutput) {
        if (qstrcmp(name, QVideoRendererControl_iid) == 0)
            m_videoOutput = new AVFVideoRendererControl(this);

        if (m_videoOutput) {
            m_session->setVideoOutput(m_videoOutput);
            return m_videoOutput;
        }
    }

    return 0;
}

void AVFCameraService::releaseControl(QMediaControl *control)
{
    if (m_videoOutput == control) {
        m_session->setVideoOutput(0);
        delete m_videoOutput;
        m_videoOutput = 0;
    }
}

#include "moc_avfcameraservice.cpp"
