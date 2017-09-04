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

#include "qsoundinstance_p.h"
#include "qsoundsource_p.h"
#include "qsoundbuffer_p.h"
#include "qdeclarative_sound_p.h"
#include "qdeclarative_audiocategory_p.h"
#include "qdeclarative_audiosample_p.h"
#include "qdeclarative_attenuationmodel_p.h"
#include "qdeclarative_playvariation_p.h"
#include "qdeclarative_audioengine_p.h"
#include "qdeclarative_audiolistener_p.h"

#include "qdebug.h"

#define DEBUG_AUDIOENGINE

QT_BEGIN_NAMESPACE

QSoundInstance::QSoundInstance(QObject *parent)
    : QObject(parent)
    , m_soundSource(0)
    , m_bindBuffer(0)
    , m_sound(0)
    , m_variationIndex(-1)
    , m_isReady(false)
    , m_gain(1)
    , m_attenuationGain(1)
    , m_varGain(1)
    , m_pitch(1)
    , m_varPitch(1)
    , m_state(QSoundInstance::StoppedState)
    , m_coneOuterGain(0)
    , m_engine(0)
{
#ifdef DEBUG_AUDIOENGINE
    qDebug() << "creating new QSoundInstance";
#endif
    m_engine = qobject_cast<QDeclarativeAudioEngine*>(parent);
}

void QSoundInstance::bindSoundDescription(QDeclarativeSound *sound)
{
#ifdef DEBUG_AUDIOENGINE
    qDebug() << "QSoundInstance::bindSoundDescription" << sound;
#endif
    if (m_sound == sound)
        return;

    if (m_sound && m_sound->categoryObject()) {
        disconnect(m_sound->categoryObject(), SIGNAL(volumeChanged(qreal)), this, SLOT(categoryVolumeChanged()));
        disconnect(m_sound->categoryObject(), SIGNAL(paused()), this, SLOT(pause()));
        disconnect(m_sound->categoryObject(), SIGNAL(stopped()), this, SLOT(stop()));
        disconnect(m_sound->categoryObject(), SIGNAL(resumed()), this, SLOT(resume()));
    }
    m_attenuationGain = 1;
    m_gain = 1;

    m_sound = sound;

    if (sound) {
        if (!m_soundSource) {
            m_soundSource = m_engine->engine()->createSoundSource();
            connect(m_soundSource, SIGNAL(stateChanged(QSoundSource::State)),
                    this, SLOT(handleSourceStateChanged(QSoundSource::State)));
        }
    } else {
        if (m_soundSource) {
            detach();
            m_engine->engine()->releaseSoundSource(m_soundSource);
            m_soundSource = 0;
        }
    }

    if (m_sound) {
        if (m_sound->categoryObject()) {
            connect(m_sound->categoryObject(), SIGNAL(volumeChanged(qreal)), this, SLOT(categoryVolumeChanged()));
            connect(m_sound->categoryObject(), SIGNAL(paused()), this, SLOT(pause()));
            connect(m_sound->categoryObject(), SIGNAL(stopped()), this, SLOT(stop()));
            connect(m_sound->categoryObject(), SIGNAL(resumed()), this, SLOT(resume()));
        }
        prepareNewVariation();
    } else {
        m_variationIndex = -1;
    }
}

QSoundInstance::~QSoundInstance()
{
#ifdef DEBUG_AUDIOENGINE
    qDebug() << "QSoundInstance::dtor()";
#endif
    if (m_soundSource) {
        detach();
        m_engine->engine()->releaseSoundSource(m_soundSource);
    }
}

void QSoundInstance::prepareNewVariation()
{
#ifdef DEBUG_AUDIOENGINE
    qDebug() << "QSoundInstance::prepareNewVariation()";
#endif
    int newVariationIndex = m_sound->genVariationIndex(m_variationIndex);
    if (newVariationIndex == m_variationIndex)
        return;
    QDeclarativePlayVariation *playVar = m_sound->getVariation(newVariationIndex);
#ifdef DEBUG_AUDIOENGINE
    qDebug() << "QSoundInstance: generate new play variation [old:" << m_variationIndex << ", new:" << newVariationIndex << "-" << playVar->sample() << "]";
#endif
    m_variationIndex = newVariationIndex;
    playVar->applyParameters(this);
    detach();

    m_bindBuffer = playVar->sampleObject()->soundBuffer();
    if (m_bindBuffer->state() == QSoundBuffer::Ready) {
        Q_ASSERT(m_soundSource);
        m_soundSource->bindBuffer(m_bindBuffer);
        m_isReady = true;
    } else {
        m_bindBuffer->load();
        connect(m_bindBuffer, SIGNAL(ready()), this, SLOT(bufferReady()));
    }
}

void QSoundInstance::bufferReady()
{
#ifdef DEBUG_AUDIOENGINE
    qDebug() << "QSoundInstance::bufferReady()";
#endif
    if (!m_soundSource)
        return;
    m_soundSource->bindBuffer(m_bindBuffer);
    disconnect(m_bindBuffer, SIGNAL(ready()), this, SLOT(bufferReady()));
    m_isReady = true;

    if (m_state == QSoundInstance::PlayingState) {
        sourcePlay();
    } else if (m_state == QSoundInstance::PausedState) {
        sourcePause();
    }
}

void QSoundInstance::categoryVolumeChanged()
{
    updateGain();
}

void QSoundInstance::handleSourceStateChanged(QSoundSource::State newState)
{
    State ns = State(newState);
    if (ns == m_state)
        return;
    if (ns == QSoundInstance::StoppedState) {
        prepareNewVariation();
    }
    setState(ns);
}

void QSoundInstance::setState(State state)
{
    if (state == m_state)
        return;
    m_state = state;
    emit stateChanged(m_state);
}

qreal QSoundInstance::categoryVolume() const
{
    if (!m_sound)
        return 1;
    if (!m_sound->categoryObject())
        return 1;
    return m_sound->categoryObject()->volume();
}

void QSoundInstance::sourceStop()
{
    Q_ASSERT(m_soundSource);
    m_soundSource->stop();
    setState(QSoundInstance::StoppedState);
}

void QSoundInstance::detach()
{
    sourceStop();
    m_isReady = false;
    if (m_soundSource)
        m_soundSource->unbindBuffer();
    if (m_bindBuffer) {
        disconnect(m_bindBuffer, SIGNAL(ready()), this, SLOT(bufferReady()));
        m_engine->engine()->releaseSoundBuffer(m_bindBuffer);
        m_bindBuffer = 0;
    }
}

void QSoundInstance::play()
{
#ifdef DEBUG_AUDIOENGINE
    qDebug() << "QSoundInstancePrivate::play()";
#endif
    if (!m_soundSource || m_state == QSoundInstance::PlayingState)
        return;
    if (!m_isReady) {
        setState(QSoundInstance::PlayingState);
        return;
    }
    sourcePlay();
    setState(QSoundInstance::PlayingState);
}

void QSoundInstance::sourcePlay()
{
    update3DVolume(m_engine->listener()->position());
    Q_ASSERT(m_soundSource);
    m_soundSource->play();
}

void QSoundInstance::resume()
{
#ifdef DEBUG_AUDIOENGINE
    qDebug() << "QSoundInstancePrivate::resume()";
#endif
    if (m_state != QSoundInstance::PausedState)
        return;
    play();
}

void QSoundInstance::pause()
{
    if (!m_soundSource || m_state == QSoundInstance::PausedState)
        return;
    if (!m_isReady) {
        setState(QSoundInstance::PausedState);
        return;
    }
    sourcePause();
    setState(QSoundInstance::PausedState);
}

void QSoundInstance::sourcePause()
{
    Q_ASSERT(m_soundSource);
    m_soundSource->pause();
}

void QSoundInstance::stop()
{
    if (!m_isReady || !m_soundSource || m_state == QSoundInstance::StoppedState) {
        setState(QSoundInstance::StoppedState);
        return;
    }
    sourceStop();
    prepareNewVariation();
}

QSoundInstance::State QSoundInstance::state() const
{
    return m_state;
}

void QSoundInstance::setPosition(const QVector3D& position)
{
    if (!m_soundSource)
        return;
    m_soundSource->setPosition(position);
}

void QSoundInstance::setDirection(const QVector3D& direction)
{
    if (!m_soundSource)
        return;
    m_soundSource->setDirection(direction);
}

void QSoundInstance::setVelocity(const QVector3D& velocity)
{
    if (!m_soundSource)
        return;
    m_soundSource->setVelocity(velocity);
}

void QSoundInstance::setGain(qreal gain)
{
    if (!m_soundSource)
        return;
    m_gain = gain;
    updateGain();
}

void QSoundInstance::setPitch(qreal pitch)
{
    if (!m_soundSource)
        return;
    m_pitch = pitch;
    updatePitch();
}

void QSoundInstance::setCone(qreal innerAngle, qreal outerAngle, qreal outerGain)
{
    if (!m_soundSource)
        return;
    m_soundSource->setCone(innerAngle, outerAngle, outerGain);
}

bool QSoundInstance::attenuationEnabled() const
{
    if (!m_sound || !m_sound->attenuationModelObject())
        return false;
    return true;
}

void QSoundInstance::update3DVolume(const QVector3D& listenerPosition)
{
    if (!m_sound || !m_soundSource)
        return;
    QDeclarativeAttenuationModel *attenModel = m_sound->attenuationModelObject();
    if (!attenModel)
        return;
    m_attenuationGain = attenModel->calculateGain(listenerPosition, m_soundSource->position());
    updateGain();
}

void QSoundInstance::updateVariationParameters(qreal varPitch, qreal varGain, bool looping)
{
    if (!m_soundSource)
        return;
    m_soundSource->setLooping(looping);
#ifdef DEBUG_AUDIOENGINE
    qDebug() << "QSoundInstance::updateVariationParameters" << varPitch << varGain << looping;
#endif
    m_varPitch = varPitch;
    m_varGain = varGain;
    updatePitch();
    updateGain();
}

void QSoundInstance::updatePitch()
{
    m_soundSource->setPitch(m_pitch * m_varPitch);
}

void QSoundInstance::updateGain()
{
    m_soundSource->setGain(m_gain * m_varGain * m_attenuationGain * categoryVolume());
}

QT_END_NAMESPACE
