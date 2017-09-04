/****************************************************************************
**
** Copyright (C) 2016 The Qt Company Ltd.
** Contact: https://www.qt.io/licensing/
**
** This file is part of the plugins of the Qt Toolkit.
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

#include "qaudioengine_openal_p.h"
#include "qdebug.h"

#define DEBUG_AUDIOENGINE

QT_USE_NAMESPACE

QSoundSourcePrivate::QSoundSourcePrivate(QObject *parent)
    : QSoundSource(parent)
    , m_alSource(0)
    , m_bindBuffer(0)
    , m_isReady(false)
    , m_state(QSoundSource::StoppedState)
    , m_gain(0)
    , m_pitch(0)
    , m_coneInnerAngle(0)
    , m_coneOuterAngle(0)
    , m_coneOuterGain(1)
{
#ifdef DEBUG_AUDIOENGINE
    qDebug() << "creating new QSoundSourcePrivate";
#endif
    alGenSources(1, &m_alSource);
    QAudioEnginePrivate::checkNoError("create source");
    setGain(1);
    setPitch(1);
    setCone(360, 360, 0);
}

QSoundSourcePrivate::~QSoundSourcePrivate()
{
#ifdef DEBUG_AUDIOENGINE
    qDebug() << "QSoundSourcePrivate::dtor";
#endif
    release();
}

void QSoundSourcePrivate::release()
{
    if (m_alSource) {
#ifdef DEBUG_AUDIOENGINE
    qDebug() << "QSoundSourcePrivate::release";
#endif
        stop();
        unbindBuffer();
        alDeleteSources(1, &m_alSource);
        QAudioEnginePrivate::checkNoError("delete source");
        m_alSource = 0;
    }
}

void QSoundSourcePrivate::bindBuffer(QSoundBuffer* soundBuffer)
{
    unbindBuffer();
    Q_ASSERT(soundBuffer->state() == QSoundBuffer::Ready);
    m_bindBuffer = qobject_cast<QSoundBufferPrivateAL*>(soundBuffer);
    m_bindBuffer->bindToSource(m_alSource);
    m_isReady = true;
}

void QSoundSourcePrivate::unbindBuffer()
{
    if (m_bindBuffer) {
        m_bindBuffer->unbindFromSource(m_alSource);
        m_bindBuffer = 0;
    }
    m_isReady = false;
    if (m_state != QSoundSource::StoppedState) {
        m_state = QSoundSource::StoppedState;
        emit stateChanged(m_state);
    }
}

void QSoundSourcePrivate::play()
{
    if (!m_alSource || !m_isReady)
        return;
    alSourcePlay(m_alSource);
#ifdef DEBUG_AUDIOENGINE
    QAudioEnginePrivate::checkNoError("play");
#endif
    emit activate(this);
}

bool QSoundSourcePrivate::isLooping() const
{
    if (!m_alSource)
        return false;
    ALint looping = 0;
    alGetSourcei(m_alSource, AL_LOOPING, &looping);
    return looping == AL_TRUE ? true : false;
}

void QSoundSourcePrivate::pause()
{
    if (!m_alSource || !m_isReady)
        return;
    alSourcePause(m_alSource);
#ifdef DEBUG_AUDIOENGINE
    QAudioEnginePrivate::checkNoError("pause");
#endif
}

void QSoundSourcePrivate::stop()
{
    if (!m_alSource)
        return;
    alSourceStop(m_alSource);
#ifdef DEBUG_AUDIOENGINE
    QAudioEnginePrivate::checkNoError("stop");
#endif
}

QSoundSource::State QSoundSourcePrivate::state() const
{
    return m_state;
}

void QSoundSourcePrivate::checkState()
{
    QSoundSource::State st;
    st = QSoundSource::StoppedState;
    if (m_alSource && m_isReady) {
        ALint s;
        alGetSourcei(m_alSource, AL_SOURCE_STATE, &s);
        switch (s) {
        case AL_PLAYING:
            st = QSoundSource::PlayingState;
            break;
        case AL_PAUSED:
            st = QSoundSource::PausedState;
            break;
        }
    }
    if (st == m_state)
        return;
    m_state = st;
    emit stateChanged(m_state);
}

void QSoundSourcePrivate::setLooping(bool looping)
{
    if (!m_alSource)
        return;
    alSourcei(m_alSource, AL_LOOPING, looping ? AL_TRUE : AL_FALSE);
}

void QSoundSourcePrivate::setPosition(const QVector3D& position)
{
    if (!m_alSource)
        return;
    alSource3f(m_alSource, AL_POSITION, position.x(), position.y(), position.z());
#ifdef DEBUG_AUDIOENGINE
    QAudioEnginePrivate::checkNoError("source set position");
#endif
}

void QSoundSourcePrivate::setDirection(const QVector3D& direction)
{
    if (!m_alSource)
        return;
    alSource3f(m_alSource, AL_DIRECTION, direction.x(), direction.y(), direction.z());
#ifdef DEBUG_AUDIOENGINE
    QAudioEnginePrivate::checkNoError("source set direction");
#endif
}

void QSoundSourcePrivate::setVelocity(const QVector3D& velocity)
{
    if (!m_alSource)
        return;
    alSource3f(m_alSource, AL_VELOCITY, velocity.x(), velocity.y(), velocity.z());
#ifdef DEBUG_AUDIOENGINE
    QAudioEnginePrivate::checkNoError("source set velocity");
#endif
}

QVector3D QSoundSourcePrivate::velocity() const
{
    if (!m_alSource)
        return QVector3D(0, 0, 0);
    ALfloat x, y, z;
    alGetSource3f(m_alSource, AL_VELOCITY, &x, &y, &z);
    return QVector3D(x, y, z);
}

QVector3D QSoundSourcePrivate::position() const
{
    if (!m_alSource)
        return QVector3D(0, 0, 0);
    ALfloat x, y, z;
    alGetSource3f(m_alSource, AL_POSITION, &x, &y, &z);
    return QVector3D(x, y, z);
}

QVector3D QSoundSourcePrivate::direction() const
{
    if (!m_alSource)
        return QVector3D(0, 1, 0);
    ALfloat x, y, z;
    alGetSource3f(m_alSource, AL_DIRECTION, &x, &y, &z);
    return QVector3D(x, y, z);
}

void QSoundSourcePrivate::setGain(qreal gain)
{
    if (!m_alSource || gain == m_gain)
        return;
    alSourcef(m_alSource, AL_GAIN, gain);
#ifdef DEBUG_AUDIOENGINE
    QAudioEnginePrivate::checkNoError("source set gain");
#endif
    m_gain = gain;
}

void QSoundSourcePrivate::setPitch(qreal pitch)
{
    if (!m_alSource || m_pitch == pitch)
        return;
    alSourcef(m_alSource, AL_PITCH, pitch);
#ifdef DEBUG_AUDIOENGINE
    QAudioEnginePrivate::checkNoError("source set pitch");
#endif
    m_pitch = pitch;
}

void QSoundSourcePrivate::setCone(qreal innerAngle, qreal outerAngle, qreal outerGain)
{
    if (innerAngle > outerAngle)
        outerAngle = innerAngle;
    Q_ASSERT(outerAngle <= 360 && innerAngle >= 0);

    //make sure the setting order will always keep outerAngle >= innerAngle in openAL
    if (outerAngle >= m_coneInnerAngle) {
        if (m_coneOuterAngle != outerAngle) {
            alSourcef(m_alSource, AL_CONE_OUTER_ANGLE, outerAngle);
#ifdef DEBUG_AUDIOENGINE
            QAudioEnginePrivate::checkNoError("source set cone outerAngle");
#endif
            m_coneOuterAngle = outerAngle;
        }
        if (m_coneInnerAngle != innerAngle) {
            alSourcef(m_alSource, AL_CONE_INNER_ANGLE, innerAngle);
#ifdef DEBUG_AUDIOENGINE
            QAudioEnginePrivate::checkNoError("source set cone innerAngle");
#endif
            m_coneInnerAngle = innerAngle;
        }
    } else {
        if (m_coneInnerAngle != innerAngle) {
            alSourcef(m_alSource, AL_CONE_INNER_ANGLE, innerAngle);
#ifdef DEBUG_AUDIOENGINE
            QAudioEnginePrivate::checkNoError("source set cone innerAngle");
#endif
            m_coneInnerAngle = innerAngle;
        }
        if (m_coneOuterAngle != outerAngle) {
            alSourcef(m_alSource, AL_CONE_OUTER_ANGLE, outerAngle);
#ifdef DEBUG_AUDIOENGINE
            QAudioEnginePrivate::checkNoError("source set cone outerAngle");
#endif
            m_coneOuterAngle = outerAngle;
        }
    }

    if (outerGain != m_coneOuterGain) {
        alSourcef(m_alSource, AL_CONE_OUTER_GAIN, outerGain);
#ifdef DEBUG_AUDIOENGINE
        QAudioEnginePrivate::checkNoError("source set cone outerGain");
#endif
        m_coneOuterGain = outerGain;
    }
}
