/****************************************************************************
**
** Copyright (C) 2016 Research In Motion
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
#include "bbcameramediarecordercontrol.h"

#include "bbcamerasession.h"

#include <QDebug>
#include <QUrl>

#include <audio/audio_manager_device.h>
#include <audio/audio_manager_volume.h>

QT_BEGIN_NAMESPACE

static audio_manager_device_t currentAudioInputDevice()
{
    audio_manager_device_t device = AUDIO_DEVICE_HEADSET;

    const int result = audio_manager_get_default_input_device(&device);
    if (result != EOK) {
        qWarning() << "Unable to retrieve default audio input device:" << result;
        return AUDIO_DEVICE_HEADSET;
    }

    return device;
}

BbCameraMediaRecorderControl::BbCameraMediaRecorderControl(BbCameraSession *session, QObject *parent)
    : QMediaRecorderControl(parent)
    , m_session(session)
{
    connect(m_session, SIGNAL(videoStateChanged(QMediaRecorder::State)), this, SIGNAL(stateChanged(QMediaRecorder::State)));
    connect(m_session, SIGNAL(videoStatusChanged(QMediaRecorder::Status)), this, SIGNAL(statusChanged(QMediaRecorder::Status)));
    connect(m_session, SIGNAL(durationChanged(qint64)), this, SIGNAL(durationChanged(qint64)));
    connect(m_session, SIGNAL(actualLocationChanged(QUrl)), this, SIGNAL(actualLocationChanged(QUrl)));
    connect(m_session, SIGNAL(videoError(int,QString)), this, SIGNAL(error(int,QString)));
}

QUrl BbCameraMediaRecorderControl::outputLocation() const
{
    return m_session->outputLocation();
}

bool BbCameraMediaRecorderControl::setOutputLocation(const QUrl &location)
{
    return m_session->setOutputLocation(location);
}

QMediaRecorder::State BbCameraMediaRecorderControl::state() const
{
    return m_session->videoState();
}

QMediaRecorder::Status BbCameraMediaRecorderControl::status() const
{
    return m_session->videoStatus();
}

qint64 BbCameraMediaRecorderControl::duration() const
{
    return m_session->duration();
}

bool BbCameraMediaRecorderControl::isMuted() const
{
    bool muted = false;

    const int result = audio_manager_get_input_mute(currentAudioInputDevice(), &muted);
    if (result != EOK) {
        emit const_cast<BbCameraMediaRecorderControl*>(this)->error(QMediaRecorder::ResourceError, tr("Unable to retrieve mute status"));
        return false;
    }

    return muted;
}

qreal BbCameraMediaRecorderControl::volume() const
{
    double level = 0.0;

    const int result = audio_manager_get_input_level(currentAudioInputDevice(), &level);
    if (result != EOK) {
        emit const_cast<BbCameraMediaRecorderControl*>(this)->error(QMediaRecorder::ResourceError, tr("Unable to retrieve audio input volume"));
        return 0.0;
    }

    return (level / 100);
}

void BbCameraMediaRecorderControl::applySettings()
{
    m_session->applyVideoSettings();
}

void BbCameraMediaRecorderControl::setState(QMediaRecorder::State state)
{
    m_session->setVideoState(state);
}

void BbCameraMediaRecorderControl::setMuted(bool muted)
{
    const int result = audio_manager_set_input_mute(currentAudioInputDevice(), muted);
    if (result != EOK) {
        emit error(QMediaRecorder::ResourceError, tr("Unable to set mute status"));
    } else {
        emit mutedChanged(muted);
    }
}

void BbCameraMediaRecorderControl::setVolume(qreal volume)
{
    const int result = audio_manager_set_input_level(currentAudioInputDevice(), (volume * 100));
    if (result != EOK) {
        emit error(QMediaRecorder::ResourceError, tr("Unable to set audio input volume"));
    } else {
        emit volumeChanged(volume);
    }
}

QT_END_NAMESPACE
