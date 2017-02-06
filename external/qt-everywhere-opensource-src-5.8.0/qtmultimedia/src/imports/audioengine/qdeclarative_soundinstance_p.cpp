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

#include "qdeclarative_soundinstance_p.h"
#include "qdeclarative_sound_p.h"
#include "qdeclarative_audioengine_p.h"
#include "qaudioengine_p.h"
#include "qsoundinstance_p.h"
#include "qdebug.h"

#define DEBUG_AUDIOENGINE

QT_USE_NAMESPACE

/*!
    \qmltype SoundInstance
    \instantiates QDeclarativeSoundInstance
    \since 5.0
    \brief Play 3d audio content.
    \inqmlmodule QtAudioEngine
    \ingroup multimedia_audioengine
    \inherits Item
    \preliminary

    There are two ways to create SoundInstance objects. You can obtain it by calling newInstance
    method of a \l Sound:

    \qml
    Rectangle {
        id:root
        color:"white"
        width: 300
        height: 500

        AudioEngine {
            id:audioengine

            AudioSample {
                name:"explosion01"
                source: "explosion-01.wav"
            }

            Sound {
                name:"explosion"
                PlayVariation {
                    sample:"explosion01"
                }
            }
        }

        property variant soundEffect: audioengine.sounds["explosion"].newInstance();

        MouseArea {
            anchors.fill: parent
            onPressed: {
                root.soundEffect.play();
            }
        }
    }
    \endqml

    Or alternatively, you can explicitly define SoundInstance outside of AudioEngine for
    easier qml bindings:

    \qml
    Rectangle {
        id:root
        color:"white"
        width: 300
        height: 500

        AudioEngine {
            id:audioengine

            AudioSample {
                name:"explosion01"
                source: "explosion-01.wav"
            }

            Sound {
                name:"explosion"
                PlayVariation {
                    sample:"explosion01"
                }
            }
        }

        Item {
            id: animator
            x: 10 + observer.percent * 100
            y: 20 + observer.percent * 80
            property real percent: 0
            SequentialAnimation on percent {
                loops: Animation.Infinite
                running: true
                NumberAnimation {
                duration: 8000
                from: 0
                to: 1
                }

            }
        }

        SoundInstance {
            id:soundEffect
            engine:audioengine
            sound:"explosion"
            position:Qt.vector3d(animator.x, animator.y, 0);
        }

        MouseArea {
            anchors.fill: parent
            onPressed: {
                soundEffect.play();
            }
        }
    }
    \endqml
*/

QDeclarativeSoundInstance::QDeclarativeSoundInstance(QObject *parent)
    : QObject(parent)
    , m_position(0, 0, 0)
    , m_direction(0, 1, 0)
    , m_velocity(0, 0, 0)
    , m_gain(1)
    , m_pitch(1)
    , m_requestState(QDeclarativeSoundInstance::StoppedState)
    , m_coneInnerAngle(360)
    , m_coneOuterAngle(360)
    , m_coneOuterGain(0)
    , m_instance(0)
    , m_engine(0)
{
#ifdef DEBUG_AUDIOENGINE
    qDebug() << "QDeclarativeSoundInstance::ctor()";
#endif
}

/*!
    \qmlproperty QtAudioEngine::AudioEngine QtAudioEngine::SoundInstance::engine

    This property holds the reference to AudioEngine, must be set only once.
*/
QDeclarativeAudioEngine* QDeclarativeSoundInstance::engine() const
{
    return m_engine;
}

void QDeclarativeSoundInstance::setEngine(QDeclarativeAudioEngine *engine)
{
#ifdef DEBUG_AUDIOENGINE
    qDebug() << "QDeclarativeSoundInstance::setEngine(" << engine << ")";
#endif
    if (!engine)
        return;

    if (m_engine) {
        qWarning("SoundInstance: you can not set different value for engine property");
        return;
    }
    m_engine = engine;
    if (!m_engine->isReady()) {
        connect(m_engine, SIGNAL(ready()), this, SLOT(engineComplete()));
    } else {
        engineComplete();
    }
}

void QDeclarativeSoundInstance::engineComplete()
{
#ifdef DEBUG_AUDIOENGINE
    qDebug() << "QDeclarativeSoundInstance::engineComplete()";
#endif
    disconnect(m_engine, SIGNAL(ready()), this, SLOT(engineComplete()));
    if (m_sound.isEmpty())
        return;

    //rebind to actual engine resource
    QString sound = m_sound;
    m_sound.clear();
    setSound(sound);
}

QDeclarativeSoundInstance::~QDeclarativeSoundInstance()
{
}

/*!
    \qmlproperty string QtAudioEngine::SoundInstance::sound

    This property specifies which Sound this SoundInstance will use. Unlike some properties in
    other types, this property can be changed dynamically.
*/
QString QDeclarativeSoundInstance::sound() const
{
    return m_sound;
}

void QDeclarativeSoundInstance::setSound(const QString& sound)
{
#ifdef DEBUG_AUDIOENGINE
    qDebug() << "QDeclarativeSoundInstance::setSound(" << sound << ")";
#endif
    if (m_sound == sound)
        return;

    if (!m_engine || !m_engine->isReady()) {
        m_sound = sound;
        emit soundChanged();
        return;
    }

#ifdef DEBUG_AUDIOENGINE
    qDebug() << "SoundInstance switch sound from [" << m_sound << "] to [" << sound << "]";
#endif

    stop();
    dropInstance();

    m_sound = sound;
    if (!m_sound.isEmpty()) {
        m_instance = m_engine->newSoundInstance(m_sound);
        connect(m_instance, SIGNAL(stateChanged(QSoundInstance::State)), this, SLOT(handleStateChanged()));
        m_instance->setPosition(m_position);
        m_instance->setDirection(m_direction);
        m_instance->setVelocity(m_velocity);
        m_instance->setGain(m_gain);
        m_instance->setPitch(m_pitch);
        m_instance->setCone(m_coneInnerAngle, m_coneOuterAngle, m_coneOuterGain);
        if (m_requestState == QDeclarativeSoundInstance::PlayingState) {
            m_instance->play();
        } else if (m_requestState == QDeclarativeSoundInstance::PausedState) {
            m_instance->pause();
        }
    }
    emit soundChanged();
}

void QDeclarativeSoundInstance::dropInstance()
{
    if (m_instance) {
        disconnect(m_instance, SIGNAL(stateChanged(QSoundInstance::State)), this, SLOT(handleStateChanged()));
        m_engine->releaseSoundInstance(m_instance);
        m_instance = 0;
    }
}

/*!
    \qmlproperty enumeration QtAudioEngine::SoundInstance::state

    This property holds the current playback state. It can be one of:

    \table
    \header \li Value \li Description
    \row \li StopppedState
        \li The SoundInstance is not playing, and when playback begins next it
        will play from position zero.
    \row \li PlayingState
        \li The SoundInstance is playing the media.
    \row \li PausedState
        \li The SoundInstance is not playing, and when playback begins next it
        will play from the position that it was paused at.
    \endtable
*/
QDeclarativeSoundInstance::State QDeclarativeSoundInstance::state() const
{
    if (m_instance)
        return State(m_instance->state());
    return m_requestState;
}

/*!
    \qmlmethod QtAudioEngine::SoundInstance::play()

    Starts playback.
*/
void QDeclarativeSoundInstance::play()
{
#ifdef DEBUG_AUDIOENGINE
    qDebug() << "QDeclarativeSoundInstance::play()";
#endif
    if (!m_instance) {
        m_requestState = QDeclarativeSoundInstance::PlayingState;
        return;
    }
    m_instance->play();
}

/*!
    \qmlmethod QtAudioEngine::SoundInstance::stop()

    Stops current playback.
*/
void QDeclarativeSoundInstance::stop()
{
#ifdef DEBUG_AUDIOENGINE
    qDebug() << "QDeclarativeSoundInstance::stop()";
#endif
    m_requestState = QDeclarativeSoundInstance::StoppedState;
    if (!m_instance)
        return;
    m_instance->stop();
}

/*!
    \qmlmethod QtAudioEngine::SoundInstance::pause()

    Pauses current playback.
*/
void QDeclarativeSoundInstance::pause()
{
#ifdef DEBUG_AUDIOENGINE
    qDebug() << "QDeclarativeSoundInstance::pause()";
#endif
    if (!m_instance) {
        m_requestState = QDeclarativeSoundInstance::PausedState;
        return;
    }
    m_instance->pause();
}

void QDeclarativeSoundInstance::updatePosition(qreal deltaTime)
{
    if (!m_instance || deltaTime == 0 || m_velocity.lengthSquared() == 0)
        return;
    setPosition(m_position + m_velocity * deltaTime);
}

/*!
    \qmlproperty vector3d QtAudioEngine::SoundInstance::position

    This property holds the current 3d position.
*/
QVector3D QDeclarativeSoundInstance::position() const
{
    return m_position;
}

void QDeclarativeSoundInstance::setPosition(const QVector3D& position)
{
    if (m_position == position)
        return;
    m_position = position;
    emit positionChanged();
    if (!m_instance) {
        return;
    }
    m_instance->setPosition(m_position);
}

/*!
    \qmlproperty vector3d QtAudioEngine::SoundInstance::direction

    This property holds the current 3d direction.
*/
QVector3D QDeclarativeSoundInstance::direction() const
{
    return m_direction;
}

void QDeclarativeSoundInstance::setDirection(const QVector3D& direction)
{
    if (m_direction == direction)
        return;
    m_direction = direction;
    emit directionChanged();
    if (!m_instance) {
        return;
    }
    m_instance->setDirection(m_direction);
}

/*!
    \qmlproperty vector3d QtAudioEngine::SoundInstance::velocity

    This property holds the current 3d velocity.
*/
QVector3D QDeclarativeSoundInstance::velocity() const
{
    return m_velocity;
}

void QDeclarativeSoundInstance::setVelocity(const QVector3D& velocity)
{
    if (m_velocity == velocity)
        return;
    m_velocity = velocity;
    emit velocityChanged();
    if (!m_instance)
        return;
    m_instance->setVelocity(m_velocity);
}

/*!
    \qmlproperty vector3d QtAudioEngine::SoundInstance::gain

    This property holds the gain adjustment which will be used to modulate the audio output level
    from this SoundInstance.
*/
qreal QDeclarativeSoundInstance::gain() const
{
    return m_gain;
}

void QDeclarativeSoundInstance::setGain(qreal gain)
{
    if (gain == m_gain)
        return;
    if (gain < 0) {
        qWarning("gain must be a positive value!");
        return;
    }
    m_gain = gain;
    emit gainChanged();
    if (!m_instance)
        return;
    m_instance->setGain(m_gain);
}

/*!
    \qmlproperty vector3d QtAudioEngine::SoundInstance::pitch

    This property holds the pitch adjustment which will be used to modulate the audio pitch
    from this SoundInstance.
*/
qreal QDeclarativeSoundInstance::pitch() const
{
    return m_pitch;
}

void QDeclarativeSoundInstance::setPitch(qreal pitch)
{
    if (pitch == m_pitch)
        return;
    if (pitch < 0) {
        qWarning("pitch must be a positive value!");
        return;
    }
    m_pitch = pitch;
    emit pitchChanged();
    if (!m_instance)
        return;
    m_instance->setPitch(m_pitch);
}

void QDeclarativeSoundInstance::setConeInnerAngle(qreal innerAngle)
{
    if (m_coneInnerAngle == innerAngle)
        return;
    m_coneInnerAngle = innerAngle;
    if (!m_instance)
        return;
    m_instance->setCone(m_coneInnerAngle, m_coneOuterAngle, m_coneOuterGain);
}

void QDeclarativeSoundInstance::setConeOuterAngle(qreal outerAngle)
{
    if (m_coneOuterAngle == outerAngle)
        return;
    m_coneOuterAngle = outerAngle;
    if (!m_instance)
        return;
    m_instance->setCone(m_coneInnerAngle, m_coneOuterAngle, m_coneOuterGain);
}

void QDeclarativeSoundInstance::setConeOuterGain(qreal outerGain)
{
    if (m_coneOuterGain == outerGain)
        return;
    m_coneOuterGain = outerGain;
    if (!m_instance)
        return;
    m_instance->setCone(m_coneInnerAngle, m_coneOuterAngle, m_coneOuterGain);
}

void QDeclarativeSoundInstance::handleStateChanged()
{
    emit stateChanged();
}

/*!
    \qmlsignal QtAudioEngine::SoundInstance::stateChanged(state)

    This signal is emitted when \l state is changed

    The corresponding handler is \c onStateChanged.
*/

/*!
    \qmlsignal QtAudioEngine::SoundInstance::positionChanged()

    This signal is emitted when \l position is changed

    The corresponding handler is \c onPositionChanged.
*/

/*!
    \qmlsignal QtAudioEngine::SoundInstance::directionChanged()

    This signal is emitted when \l direction is changed

    The corresponding handler is \c onDirectionChanged.
*/

/*!
    \qmlsignal QtAudioEngine::SoundInstance::velocityChanged()

    This signal is emitted when \l velocity is changed

    The corresponding handler is \c onVelocityChanged.
*/

/*!
    \qmlsignal QtAudioEngine::SoundInstance::gainChanged()

    This signal is emitted when \l gain is changed

    The corresponding handler is \c onGainChanged.
*/

/*!
    \qmlsignal QtAudioEngine::SoundInstance::pitchChanged()

    This signal is emitted when \l pitch is changed

    The corresponding handler is \c onPitchChanged.
*/

/*!
    \qmlsignal QtAudioEngine::SoundInstance::soundChanged()

    This signal is emitted when \l sound is changed

    The corresponding handler is \c onSoundChanged.
*/
