/****************************************************************************
**
** Copyright (C) 2016 The Qt Company Ltd.
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

#include "audiocapturesession.h"
#include "audiomediarecordercontrol.h"

#include <QtCore/qdebug.h>

QT_BEGIN_NAMESPACE

AudioMediaRecorderControl::AudioMediaRecorderControl(QObject *parent)
    : QMediaRecorderControl(parent)
{
    m_session = qobject_cast<AudioCaptureSession*>(parent);
    connect(m_session, SIGNAL(positionChanged(qint64)),
            this, SIGNAL(durationChanged(qint64)));
    connect(m_session, SIGNAL(stateChanged(QMediaRecorder::State)),
            this, SIGNAL(stateChanged(QMediaRecorder::State)));
    connect(m_session, SIGNAL(statusChanged(QMediaRecorder::Status)),
            this, SIGNAL(statusChanged(QMediaRecorder::Status)));
    connect(m_session, SIGNAL(actualLocationChanged(QUrl)),
            this, SIGNAL(actualLocationChanged(QUrl)));
    connect(m_session, &AudioCaptureSession::volumeChanged,
            this, &AudioMediaRecorderControl::volumeChanged);
    connect(m_session, &AudioCaptureSession::mutedChanged,
            this, &AudioMediaRecorderControl::mutedChanged);
    connect(m_session, SIGNAL(error(int,QString)),
            this, SIGNAL(error(int,QString)));
}

AudioMediaRecorderControl::~AudioMediaRecorderControl()
{
}

QUrl AudioMediaRecorderControl::outputLocation() const
{
    return m_session->outputLocation();
}

bool AudioMediaRecorderControl::setOutputLocation(const QUrl& sink)
{
    return m_session->setOutputLocation(sink);
}

QMediaRecorder::State AudioMediaRecorderControl::state() const
{
    return m_session->state();
}

QMediaRecorder::Status AudioMediaRecorderControl::status() const
{
    return m_session->status();
}

qint64 AudioMediaRecorderControl::duration() const
{
    return m_session->position();
}

bool AudioMediaRecorderControl::isMuted() const
{
    return m_session->isMuted();
}

qreal AudioMediaRecorderControl::volume() const
{
    return m_session->volume();
}

void AudioMediaRecorderControl::setState(QMediaRecorder::State state)
{
    m_session->setState(state);
}

void AudioMediaRecorderControl::setMuted(bool muted)
{
    m_session->setMuted(muted);
}

void AudioMediaRecorderControl::setVolume(qreal volume)
{
    m_session->setVolume(volume);
}

QT_END_NAMESPACE
