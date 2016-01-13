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

#include "camerabinrecorder.h"
#include "camerabincontrol.h"
#include "camerabinresourcepolicy.h"
#include "camerabinaudioencoder.h"
#include "camerabinvideoencoder.h"
#include "camerabincontainer.h"
#include <QtCore/QDebug>

#include <gst/pbutils/encoding-profile.h>

QT_BEGIN_NAMESPACE

CameraBinRecorder::CameraBinRecorder(CameraBinSession *session)
    :QMediaRecorderControl(session),
     m_session(session),
     m_state(QMediaRecorder::StoppedState),
     m_status(QMediaRecorder::UnloadedStatus)
{
    connect(m_session, SIGNAL(stateChanged(QCamera::State)), SLOT(updateStatus()));
    connect(m_session, SIGNAL(pendingStateChanged(QCamera::State)), SLOT(updateStatus()));
    connect(m_session, SIGNAL(busyChanged(bool)), SLOT(updateStatus()));

    connect(m_session, SIGNAL(durationChanged(qint64)), SIGNAL(durationChanged(qint64)));
    connect(m_session, SIGNAL(mutedChanged(bool)), this, SIGNAL(mutedChanged(bool)));
    connect(m_session->cameraControl()->resourcePolicy(), SIGNAL(canCaptureChanged()),
            this, SLOT(updateStatus()));
}

CameraBinRecorder::~CameraBinRecorder()
{
}

QUrl CameraBinRecorder::outputLocation() const
{
    return m_session->outputLocation();
}

bool CameraBinRecorder::setOutputLocation(const QUrl &sink)
{
    m_session->setOutputLocation(sink);
    return true;
}

QMediaRecorder::State CameraBinRecorder::state() const
{
    return m_state;
}

QMediaRecorder::Status CameraBinRecorder::status() const
{
    return m_status;
}

void CameraBinRecorder::updateStatus()
{
    QCamera::State sessionState = m_session->state();

    QMediaRecorder::State oldState = m_state;
    QMediaRecorder::Status oldStatus = m_status;

    if (sessionState == QCamera::ActiveState &&
            m_session->captureMode().testFlag(QCamera::CaptureVideo)) {

        if (!m_session->cameraControl()->resourcePolicy()->canCapture()) {
            m_status = QMediaRecorder::UnavailableStatus;
            m_state = QMediaRecorder::StoppedState;
            m_session->stopVideoRecording();
        } else  if (m_state == QMediaRecorder::RecordingState) {
            m_status = QMediaRecorder::RecordingStatus;
        } else {
            m_status = m_session->isBusy() ?
                        QMediaRecorder::FinalizingStatus :
                        QMediaRecorder::LoadedStatus;
        }
    } else {
        if (m_state == QMediaRecorder::RecordingState) {
            m_state = QMediaRecorder::StoppedState;
            m_session->stopVideoRecording();
        }
        m_status = m_session->pendingState() == QCamera::ActiveState ?
                    QMediaRecorder::LoadingStatus :
                    QMediaRecorder::UnloadedStatus;
    }

    if (m_state != oldState)
        emit stateChanged(m_state);

    if (m_status != oldStatus)
        emit statusChanged(m_status);
}

qint64 CameraBinRecorder::duration() const
{
    return m_session->duration();
}


void CameraBinRecorder::applySettings()
{
    CameraBinContainer *containerControl = m_session->mediaContainerControl();
    CameraBinAudioEncoder *audioEncoderControl = m_session->audioEncodeControl();
    CameraBinVideoEncoder *videoEncoderControl = m_session->videoEncodeControl();

    containerControl->resetActualContainerFormat();
    audioEncoderControl->resetActualSettings();
    videoEncoderControl->resetActualSettings();

    //encodebin doesn't like the encoding profile with ANY caps,
    //if container and codecs are not specified,
    //try to find a commonly used supported combination
    if (containerControl->containerFormat().isEmpty() &&
            audioEncoderControl->audioSettings().codec().isEmpty() &&
            videoEncoderControl->videoSettings().codec().isEmpty()) {

        QList<QStringList> candidates;
        candidates.append(QStringList() << "video/x-matroska" << "video/x-h264" << "audio/mpeg, mpegversion=(int)4");
        candidates.append(QStringList() << "video/webm" << "video/x-vp8" << "audio/x-vorbis");
        candidates.append(QStringList() << "application/ogg" << "video/x-theora" << "audio/x-vorbis");
        candidates.append(QStringList() << "video/quicktime" << "video/x-h264" << "audio/mpeg, mpegversion=(int)4");
        candidates.append(QStringList() << "video/quicktime" << "video/x-h264" << "audio/mpeg");
        candidates.append(QStringList() << "video/x-msvideo" << "video/x-divx" << "audio/mpeg");

        foreach (const QStringList &candidate, candidates) {
            if (containerControl->supportedContainers().contains(candidate[0]) &&
                    videoEncoderControl->supportedVideoCodecs().contains(candidate[1]) &&
                    audioEncoderControl->supportedAudioCodecs().contains(candidate[2])) {
                containerControl->setActualContainerFormat(candidate[0]);

                QVideoEncoderSettings videoSettings = videoEncoderControl->videoSettings();
                videoSettings.setCodec(candidate[1]);
                if (videoSettings.resolution().isEmpty())
                    videoSettings.setResolution(640, 480);
                videoEncoderControl->setActualVideoSettings(videoSettings);

                QAudioEncoderSettings audioSettings = audioEncoderControl->audioSettings();
                audioSettings.setCodec(candidate[2]);
                audioEncoderControl->setActualAudioSettings(audioSettings);

                break;
            }
        }
    }
}

GstEncodingContainerProfile *CameraBinRecorder::videoProfile()
{
    GstEncodingContainerProfile *containerProfile = m_session->mediaContainerControl()->createProfile();

    if (containerProfile) {
        GstEncodingProfile *audioProfile = m_session->audioEncodeControl()->createProfile();
        GstEncodingProfile *videoProfile = m_session->videoEncodeControl()->createProfile();

        if (audioProfile) {
            if (!gst_encoding_container_profile_add_profile(containerProfile, audioProfile))
                gst_encoding_profile_unref(audioProfile);
        }
        if (videoProfile) {
            if (!gst_encoding_container_profile_add_profile(containerProfile, videoProfile))
                gst_encoding_profile_unref(videoProfile);
        }
    }

    return containerProfile;
}

void CameraBinRecorder::setState(QMediaRecorder::State state)
{
    if (m_state == state)
        return;

    QMediaRecorder::State oldState = m_state;
    QMediaRecorder::Status oldStatus = m_status;

    switch (state) {
    case QMediaRecorder::StoppedState:
        m_state = state;
        m_status = QMediaRecorder::FinalizingStatus;
        m_session->stopVideoRecording();
        break;
    case QMediaRecorder::PausedState:
        emit error(QMediaRecorder::ResourceError, tr("QMediaRecorder::pause() is not supported by camerabin2."));
        break;
    case QMediaRecorder::RecordingState:

        if (m_session->state() != QCamera::ActiveState) {
            emit error(QMediaRecorder::ResourceError, tr("Service has not been started"));
        } else if (!m_session->cameraControl()->resourcePolicy()->canCapture()) {
            emit error(QMediaRecorder::ResourceError, tr("Recording permissions are not available"));
        } else {
            m_session->recordVideo();
            m_state = state;
            m_status = QMediaRecorder::RecordingStatus;
            emit actualLocationChanged(m_session->outputLocation());
        }
    }

    if (m_state != oldState)
        emit stateChanged(m_state);

    if (m_status != oldStatus)
        emit statusChanged(m_status);
}

bool CameraBinRecorder::isMuted() const
{
    return m_session->isMuted();
}

qreal CameraBinRecorder::volume() const
{
    return 1.0;
}

void CameraBinRecorder::setMuted(bool muted)
{
    m_session->setMuted(muted);
}

void CameraBinRecorder::setVolume(qreal volume)
{
    if (!qFuzzyCompare(volume, qreal(1.0)))
        qWarning() << "Media service doesn't support recorder audio gain.";
}

QT_END_NAMESPACE

