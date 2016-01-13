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
#include "bbcameramediarecordercontrol.h"

#include "bbcamerasession.h"

#include <QDebug>
#include <QUrl>

#ifndef Q_OS_BLACKBERRY_TABLET
#include <audio/audio_manager_device.h>
#include <audio/audio_manager_volume.h>
#endif

QT_BEGIN_NAMESPACE

#ifndef Q_OS_BLACKBERRY_TABLET
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
#endif

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

#ifndef Q_OS_BLACKBERRY_TABLET
    const int result = audio_manager_get_input_mute(currentAudioInputDevice(), &muted);
    if (result != EOK) {
        emit const_cast<BbCameraMediaRecorderControl*>(this)->error(QMediaRecorder::ResourceError, tr("Unable to retrieve mute status"));
        return false;
    }
#endif
    return muted;
}

qreal BbCameraMediaRecorderControl::volume() const
{
    double level = 0.0;

#ifndef Q_OS_BLACKBERRY_TABLET
    const int result = audio_manager_get_input_level(currentAudioInputDevice(), &level);
    if (result != EOK) {
        emit const_cast<BbCameraMediaRecorderControl*>(this)->error(QMediaRecorder::ResourceError, tr("Unable to retrieve audio input volume"));
        return 0.0;
    }
#endif

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
#ifndef Q_OS_BLACKBERRY_TABLET
    const int result = audio_manager_set_input_mute(currentAudioInputDevice(), muted);
    if (result != EOK) {
        emit error(QMediaRecorder::ResourceError, tr("Unable to set mute status"));
    } else {
        emit mutedChanged(muted);
    }
#endif
}

void BbCameraMediaRecorderControl::setVolume(qreal volume)
{
#ifndef Q_OS_BLACKBERRY_TABLET
    const int result = audio_manager_set_input_level(currentAudioInputDevice(), (volume * 100));
    if (result != EOK) {
        emit error(QMediaRecorder::ResourceError, tr("Unable to set audio input volume"));
    } else {
        emit volumeChanged(volume);
    }
#endif
}

QT_END_NAMESPACE
