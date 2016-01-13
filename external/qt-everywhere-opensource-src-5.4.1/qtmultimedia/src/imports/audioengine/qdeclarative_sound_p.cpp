/****************************************************************************
**
** Copyright (C) 2014 Digia Plc and/or its subsidiary(-ies).
** Contact: http://www.qt-project.org/legal
**
** This file is part of the plugins of the Qt Toolkit.
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

#include "qdeclarative_sound_p.h"
#include "qdeclarative_audiocategory_p.h"
#include "qdeclarative_attenuationmodel_p.h"
#include "qdeclarative_soundinstance_p.h"
#include "qdeclarative_audioengine_p.h"
#include "qdebug.h"

#define DEBUG_AUDIOENGINE

QT_USE_NAMESPACE

QDeclarativeSoundCone::QDeclarativeSoundCone(QObject *parent)
    : QObject(parent)
    , m_innerAngle(360)
    , m_outerAngle(360)
    , m_outerGain(0)
{
}

/*!
    \qmlproperty real Sound::cone.innerAngle

    This property holds the innerAngle for Sound definition.
    The range is [0, 360] degree. There is no directional attenuation within innerAngle.
*/
qreal QDeclarativeSoundCone::innerAngle() const
{
    return m_innerAngle;
}

void QDeclarativeSoundCone::setInnerAngle(qreal innerAngle)
{
    QDeclarativeSound *s = qobject_cast<QDeclarativeSound*>(parent());
    if (s && s->m_complete)
        return;
    if (innerAngle < 0 || innerAngle > 360) {
        qWarning() << "innerAngle should be within[0, 360] degrees";
        return;
    }
    m_innerAngle = innerAngle;
}

/*!
    \qmlproperty real Sound::cone.outerAngle

    This property holds the outerAngle for Sound definition.
    The range is [0, 360] degree. All audio output from this sound will be attenuated by \l outerGain
    outside outerAngle.
*/
qreal QDeclarativeSoundCone::outerAngle() const
{
    return m_outerAngle;
}

void QDeclarativeSoundCone::setOuterAngle(qreal outerAngle)
{
    QDeclarativeSound *s = qobject_cast<QDeclarativeSound*>(parent());
    if (s && s->m_complete)
        return;
    if (outerAngle < 0 || outerAngle > 360) {
        qWarning() << "outerAngle should be within[0, 360] degrees";
        return;
    }
    m_outerAngle = outerAngle;
}

/*!
    \qmlproperty real Sound::cone.outerGain

    This property holds attenuation value for directional attenuation of this sound.
    The range is [0, 1]. All audio output from this sound will be attenuated by outerGain
    outside \l outerAngle.
*/
qreal QDeclarativeSoundCone::outerGain() const
{
    return m_outerGain;
}

void QDeclarativeSoundCone::setOuterGain(qreal outerGain)
{
    QDeclarativeSound *s = qobject_cast<QDeclarativeSound*>(parent());
    if (s && s->m_complete)
        return;
    if (outerGain < 0 || outerGain > 1) {
        qWarning() << "outerGain should no less than 0 and no more than 1";
        return;
    }
    m_outerGain = outerGain;
}

void QDeclarativeSoundCone::componentComplete()
{
    if (m_outerAngle < m_innerAngle) {
        m_outerAngle = m_innerAngle;
    }
}

////////////////////////////////////////////////////////////
/*!
    \qmltype Sound
    \instantiates QDeclarativeSound
    \since 5.0
    \brief Define a variety of samples and parameters to be used for
    SoundInstance.
    \inqmlmodule QtAudioEngine
    \ingroup multimedia_audioengine
    \inherits Item
    \preliminary

    This type is part of the \b{QtAudioEngine 1.0} module.

    Sound can be accessed through QtAudioEngine::AudioEngine::sounds with its unique name
    and must be defined inside AudioEngine.

    \qml
    import QtQuick 2.0
    import QtAudioEngine 1.0

    Rectangle {
        color:"white"
        width: 300
        height: 500

        AudioEngine {
            id:audioengine

            AudioSample {
                name:"explosion01"
                source: "explosion-01.wav"
            }

            AudioSample {
                name:"explosion02"
                source: "explosion-02.wav"
            }

            Sound {
                name:"explosion"
                PlayVariation {
                    sample:"explosion01"
                    minPitch: 0.8
                    maxPitch: 1.1
                }
                PlayVariation {
                    sample:"explosion01"
                    minGain: 1.1
                    maxGain: 1.5
                }
            }
        }
        MouseArea {
            anchors.fill: parent
            onPressed: {
                audioengine.sounds["explosion"].play();
            }
        }
    }
    \endqml
*/

QDeclarativeSound::QDeclarativeSound(QObject *parent)
    : QObject(parent)
    , m_complete(false)
    , m_playType(Random)
    , m_attenuationModelObject(0)
    , m_categoryObject(0)
{
    m_cone = new QDeclarativeSoundCone(this);
}

QDeclarativeSound::~QDeclarativeSound()
{
}

void QDeclarativeSound::classBegin()
{
    if (!parent() || !parent()->inherits("QDeclarativeAudioEngine")) {
        qWarning("Sound must be defined inside AudioEngine!");
        return;
    }
}

void QDeclarativeSound::componentComplete()
{
    m_complete = true;
    m_cone->componentComplete();
}

/*!
    \qmlproperty enumeration QtAudioEngine::Sound::playType

    This property holds the playType.  It can be one of:

    \list
    \li Random - randomly picks up a play variation when playback is triggered
    \li Sequential - plays each variation in sequence when playback is triggered
    \endlist

    The default value is Random.
*/
QDeclarativeSound::PlayType QDeclarativeSound::playType() const
{
    return m_playType;
}

void QDeclarativeSound::setPlayType(PlayType playType)
{
    if (m_complete) {
        qWarning("Sound: playType not changable after initialization.");
        return;
    }
    m_playType = playType;
}

/*!
    \qmlproperty string QtAudioEngine::Sound::category

    This property specifies which AudioCategory this sound belongs to.
*/
QString QDeclarativeSound::category() const
{
    return m_category;
}

void QDeclarativeSound::setCategory(const QString& category)
{
    if (m_complete) {
        qWarning("Sound: category not changable after initialization.");
        return;
    }
    m_category = category;
}

/*!
    \qmlproperty string QtAudioEngine::Sound::name

    This property holds the name of Sound, must be unique among all sounds and only
    defined once.
*/
QString QDeclarativeSound::name() const
{
    return m_name;
}

void QDeclarativeSound::setName(const QString& name)
{
    if (m_complete) {
        qWarning("Sound: category not changable after initialization.");
        return;
    }
    m_name = name;
}

/*!
    \qmlproperty string QtAudioEngine::Sound::attenuationModel

    This property specifies which attenuation model this sound will apply.
*/
QString QDeclarativeSound::attenuationModel() const
{
    return m_attenuationModel;
}

int QDeclarativeSound::genVariationIndex(int oldVariationIndex)
{
    if (m_playlist.count() == 0)
        return -1;

    if (m_playlist.count() == 1)
        return 0;

    switch (m_playType) {
        case QDeclarativeSound::Random: {
            if (oldVariationIndex < 0)
                oldVariationIndex = 0;
            return (oldVariationIndex + (qrand() % (m_playlist.count() + 1))) % m_playlist.count();
        }
        default:
            return (oldVariationIndex + 1) % m_playlist.count();
    }
}

QDeclarativePlayVariation* QDeclarativeSound::getVariation(int index)
{
    Q_ASSERT(index >= 0 && index < m_playlist.count());
    return m_playlist[index];
}

void QDeclarativeSound::setAttenuationModel(QString attenuationModel)
{
    if (m_complete) {
        qWarning("Sound: attenuationModel not changable after initialization.");
        return;
    }
    m_attenuationModel = attenuationModel;
}

QDeclarativeSoundCone* QDeclarativeSound::cone() const
{
    return m_cone;
}

QDeclarativeAttenuationModel* QDeclarativeSound::attenuationModelObject() const
{
    return m_attenuationModelObject;
}

void QDeclarativeSound::setAttenuationModelObject(QDeclarativeAttenuationModel *attenuationModelObject)
{
    m_attenuationModelObject = attenuationModelObject;
}

QDeclarativeAudioCategory* QDeclarativeSound::categoryObject() const
{
    return m_categoryObject;
}

void QDeclarativeSound::setCategoryObject(QDeclarativeAudioCategory *categoryObject)
{
    m_categoryObject = categoryObject;
}

QQmlListProperty<QDeclarativePlayVariation> QDeclarativeSound::playVariationlist()
{
    return QQmlListProperty<QDeclarativePlayVariation>(this, 0, appendFunction, 0, 0, 0);
}

QList<QDeclarativePlayVariation*>& QDeclarativeSound::playlist()
{
    return m_playlist;
}

void QDeclarativeSound::appendFunction(QQmlListProperty<QDeclarativePlayVariation> *property, QDeclarativePlayVariation *value)
{
    QDeclarativeSound *sound = static_cast<QDeclarativeSound*>(property->object);
    if (sound->m_complete) {
        qWarning("Sound: PlayVariation not addable after initialization.");
        return;
    }
    sound->m_playlist.append(value);
}

/*!
    \qmlmethod QtAudioEngine::Sound::play()

    Creates a new \l SoundInstance and starts playing.
    Position, direction and velocity are all set to \c "0,0,0".
*/
void QDeclarativeSound::play()
{
    play(QVector3D(), QVector3D(), QVector3D(), 1, 1);
}

/*!
    \qmlmethod QtAudioEngine::Sound::play(gain)

    Creates a new SoundInstance and starts playing with the adjusted \a gain.
    Position, direction and velocity are all set to \c "0,0,0".
*/
void QDeclarativeSound::play(qreal gain)
{
    play(QVector3D(), QVector3D(), QVector3D(), gain, 1);
}

/*!
    \qmlmethod QtAudioEngine::Sound::play(gain, pitch)

    Creates a new SoundInstance and starts playing with the adjusted \a gain and \a pitch.
    Position, direction and velocity are all set to \c "0,0,0".
*/
void QDeclarativeSound::play(qreal gain, qreal pitch)
{
    play(QVector3D(), QVector3D(), QVector3D(), gain, pitch);
}

/*!
    \qmlmethod QtAudioEngine::Sound::play(position)

    Creates a new SoundInstance and starts playing with specified \a position.
    Direction and velocity are all set to \c "0,0,0".
*/
void QDeclarativeSound::play(const QVector3D& position)
{
    play(position, QVector3D(), QVector3D(), 1, 1);
}

/*!
    \qmlmethod QtAudioEngine::Sound::play(position, velocity)

    Creates a new SoundInstance and starts playing with specified \a position and \a velocity.
    Direction is set to \c "0,0,0".
*/
void QDeclarativeSound::play(const QVector3D& position, const QVector3D& velocity)
{
    play(position, velocity, QVector3D(), 1, 1);
}

/*!
    \qmlmethod QtAudioEngine::Sound::play(position, velocity, direction)

    Creates a new SoundInstance and starts playing with specified \a position, \a velocity and
    \a direction.
*/
void QDeclarativeSound::play(const QVector3D& position, const QVector3D& velocity,
                             const QVector3D& direction)
{
    play(position, velocity, direction, 1, 1);
}

/*!
    \qmlmethod QtAudioEngine::Sound::play(position, gain)

    Creates a new SoundInstance and starts playing with specified \a position and adjusted \a gain.
    Direction and velocity are all set to \c "0,0,0".
*/
void QDeclarativeSound::play(const QVector3D& position, qreal gain)
{
    play(position, QVector3D(), QVector3D(), gain, 1);
}

/*!
    \qmlmethod QtAudioEngine::Sound::play(position, velocity, gain)

    Creates a new SoundInstance and starts playing with specified \a position, \a velocity and
    adjusted \a gain.
    Direction is set to \c "0,0,0".
*/
void QDeclarativeSound::play(const QVector3D& position, const QVector3D& velocity, qreal gain)
{
    play(position, velocity, QVector3D(), gain, 1);
}

/*!
    \qmlmethod QtAudioEngine::Sound::play(position, velocity, direction, gain)

    Creates a new SoundInstance and starts playing with specified \a position, \a velocity,
    \a direction and adjusted \a gain.
*/
void QDeclarativeSound::play(const QVector3D& position, const QVector3D& velocity, const QVector3D& direction, qreal gain)
{
    play(position, velocity, direction, gain, 1);
}

/*!
    \qmlmethod QtAudioEngine::Sound::play(position, gain, pitch)

    Creates a new SoundInstance and starts playing with specified \a position, adjusted \a gain and
    \a pitch.
    Direction and velocity are all set to \c "0,0,0".
*/
void QDeclarativeSound::play(const QVector3D& position, qreal gain, qreal pitch)
{
    play(position, QVector3D(), QVector3D(), gain, pitch);
}

/*!
    \qmlmethod QtAudioEngine::Sound::play(position, velocity, gain, pitch)

    Creates a new SoundInstance and starts playing with specified \a position, \a velocity,
    adjusted \a gain and \a pitch.
    Direction is set to \c "0,0,0".
*/
void QDeclarativeSound::play(const QVector3D& position, const QVector3D& velocity, qreal gain, qreal pitch)
{
    play(position, velocity, QVector3D(), gain, pitch);
}

/*!
    \qmlmethod QtAudioEngine::Sound::play(position, velocity, direction, gain, pitch)

    Creates a new SoundInstance and starts playing with specified \a position, \a velocity,
    \a direction, adjusted \a gain and \a pitch.
*/
void QDeclarativeSound::play(const QVector3D& position, const QVector3D& velocity, const QVector3D& direction, qreal gain, qreal pitch)
{
    if (!m_complete) {
        qWarning() << "AudioEngine::play not ready!";
        return;
    }
    QDeclarativeSoundInstance *instance = this->newInstance(true);
    if (!instance)
        return;
    instance->setPosition(position);
    instance->setVelocity(velocity);
    instance->setDirection(direction);
    instance->setGain(gain);
    instance->setPitch(pitch);
    instance->setConeInnerAngle(cone()->innerAngle());
    instance->setConeOuterAngle(cone()->outerAngle());
    instance->setConeOuterGain(cone()->outerGain());
    instance->play();
#ifdef DEBUG_AUDIOENGINE
    qDebug() << "Sound[" << m_name << "] play ("
             << position << ","
             << velocity <<","
             << direction << ","
             << gain << ","
             << pitch << ")triggered";
#endif

}

/*!
    \qmlmethod QtAudioEngine::SoundInstance QtAudioEngine::Sound::newInstance()

    Returns a new \l SoundInstance.
*/
QDeclarativeSoundInstance* QDeclarativeSound::newInstance()
{
    return newInstance(false);
}

QDeclarativeSoundInstance* QDeclarativeSound::newInstance(bool managed)
{
    QDeclarativeSoundInstance *instance =
            qobject_cast<QDeclarativeAudioEngine*>(this->parent())->newDeclarativeSoundInstance(managed);
    instance->setSound(m_name);
    return instance;
}

