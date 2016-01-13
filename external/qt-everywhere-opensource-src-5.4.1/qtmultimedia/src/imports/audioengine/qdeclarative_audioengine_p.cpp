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

#include "qdeclarative_audioengine_p.h"
#include "qdeclarative_audiolistener_p.h"
#include "qdeclarative_audiocategory_p.h"
#include "qdeclarative_audiosample_p.h"
#include "qdeclarative_sound_p.h"
#include "qdeclarative_playvariation_p.h"
#include "qdeclarative_attenuationmodel_p.h"
#include "qdeclarative_soundinstance_p.h"
#include "qsoundinstance_p.h"
#include <QtQml/qqmlengine.h>
#include "qdebug.h"

#define DEBUG_AUDIOENGINE

QT_BEGIN_NAMESPACE

/*!
    \qmltype AudioEngine
    \instantiates QDeclarativeAudioEngine
    \since 5.0
    \brief Organize all your 3d audio content in one place.
    \inqmlmodule QtAudioEngine
    \ingroup multimedia_audioengine
    \inherits Item
    \preliminary

    \c AudioEngine is part of the \b{QtAudioEngine 1.0} module.

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
                name:"explosion"
                source: "explosion-02.wav"
            }

            Sound {
                name:"explosion"
                PlayVariation {
                    sample:"explosion"
                }
            }

            dopplerFactor: 1
            speedOfSound: 343.33 // Approximate speed of sound in air at 20 degrees Celsius

            listener.up:"0,0,1"
            listener.position:"0,0,0"
            listener.velocity:"0,0,0"
            listener.direction:"0,1,0"
        }

        MouseArea {
            anchors.fill: parent
            onPressed: {
                audioengine.sounds["explosion"].play();
            }
        }
    }
    \endqml

    \c AudioEngine acts as a central library for configuring all 3d audio content in an
    app, so you should define only one in your app.

    It is mostly used as a container to access other types such as AudioCategory, AudioSample and
    Sound.

    \sa AudioCategory, AudioSample, Sound, SoundInstance, AttenuationModelLinear, AttenuationModelInverse
*/

QDeclarativeAudioEngine::QDeclarativeAudioEngine(QObject *parent)
    : QObject(parent)
    , m_complete(false)
    , m_defaultCategory(0)
    , m_defaultAttenuationModel(0)
    , m_audioEngine(0)
{
    m_audioEngine = QAudioEngine::create(this);
    connect(m_audioEngine, SIGNAL(isLoadingChanged()), this, SIGNAL(isLoadingChanged()));
    connect(m_audioEngine, SIGNAL(isLoadingChanged()), this, SLOT(handleLoadingChanged()));
    m_listener = new QDeclarativeAudioListener(this);
    m_updateTimer.setInterval(100);
    connect(&m_updateTimer, SIGNAL(timeout()), this, SLOT(updateSoundInstances()));
}

QDeclarativeAudioEngine::~QDeclarativeAudioEngine()
{
#ifdef DEBUG_AUDIOENGINE
    qDebug() << "QDeclarativeAudioEngine::dtor"
             << "active = " << m_activeSoundInstances.count()
             << "pool = " << m_soundInstancePool.count();
#endif
    qDeleteAll(m_activeSoundInstances);
    m_activeSoundInstances.clear();

#ifdef DEBUG_AUDIOENGINE
    qDebug() << "for pool";
#endif
    qDeleteAll(m_soundInstancePool);
    m_soundInstancePool.clear();
}

void QDeclarativeAudioEngine::classBegin()
{
}

bool QDeclarativeAudioEngine::isReady() const
{
    return m_complete;
}

QAudioEngine* QDeclarativeAudioEngine::engine() const
{
    return m_audioEngine;
}

QDeclarativeSoundInstance* QDeclarativeAudioEngine::newDeclarativeSoundInstance(bool managed)
{
#ifdef DEBUG_AUDIOENGINE
    qDebug() << "QDeclarativeAudioEngine::newDeclarativeSoundInstance(" << managed << ")";
#endif
    QDeclarativeSoundInstance *instance = 0;
    if (managed) {
        if (m_managedDeclSndInstancePool.count() > 0) {
            instance = m_managedDeclSndInstancePool.last();
            m_managedDeclSndInstancePool.pop_back();
        } else {
            instance = new QDeclarativeSoundInstance(this);
            qmlEngine(instance)->setObjectOwnership(instance, QQmlEngine::CppOwnership);
            instance->setEngine(this);
        }
        m_managedDeclSoundInstances.push_back(instance);
    } else {
        instance = new QDeclarativeSoundInstance();
        instance->setEngine(this);
        qmlEngine(instance)->setObjectOwnership(instance, QQmlEngine::JavaScriptOwnership);
    }
    return instance;
}

void QDeclarativeAudioEngine::releaseManagedDeclarativeSoundInstance(QDeclarativeSoundInstance* declSndInstance)
{
    declSndInstance->setSound(QString());
    m_managedDeclSndInstancePool.push_back(declSndInstance);
}

/*!
    \qmlproperty int QtAudioEngine::AudioEngine::liveInstances

    This property indicates how many live sound instances there are at the moment.
*/
int QDeclarativeAudioEngine::liveInstanceCount() const
{
    return m_activeSoundInstances.count();
}

QSoundInstance* QDeclarativeAudioEngine::newSoundInstance(const QString &name)
{
    QSoundInstance *instance = 0;
    if (m_soundInstancePool.count() > 0) {
        instance = m_soundInstancePool.last();
        m_soundInstancePool.pop_back();
    } else {
        instance = new QSoundInstance(this);
    }
    instance->bindSoundDescription(qobject_cast<QDeclarativeSound*>(qvariant_cast<QObject*>(m_sounds.value(name))));
    m_activeSoundInstances.push_back(instance);
    if (!m_updateTimer.isActive())
        m_updateTimer.start();
    emit liveInstanceCountChanged();
    return instance;
}

void QDeclarativeAudioEngine::releaseSoundInstance(QSoundInstance* instance)
{
    instance->bindSoundDescription(0);
    m_activeSoundInstances.removeOne(instance);
    m_soundInstancePool.push_back(instance);
    emit liveInstanceCountChanged();
}

void QDeclarativeAudioEngine::componentComplete()
{
#ifdef DEBUG_AUDIOENGINE
    qDebug() << "AudioEngine begin initialization";
#endif
    if (!m_defaultCategory) {
#ifdef DEBUG_AUDIOENGINE
        qDebug() << "creating default category";
#endif
        m_defaultCategory = new QDeclarativeAudioCategory(this);
        m_defaultCategory->classBegin();
        m_defaultCategory->setName(QString::fromLatin1("default"));
        m_defaultCategory->setVolume(1);
        m_defaultCategory->componentComplete();
    }
#ifdef DEBUG_AUDIOENGINE
        qDebug() << "init samples" << m_samples.keys().count();
#endif
    foreach (const QString& key, m_samples.keys()) {
        QDeclarativeAudioSample *sample = qobject_cast<QDeclarativeAudioSample*>(
                                          qvariant_cast<QObject*>(m_samples[key]));
        if (!sample) {
            qWarning() << "accessing invalid sample[" << key << "]";
            continue;
        }
        sample->init();
    }

#ifdef DEBUG_AUDIOENGINE
        qDebug() << "init sounds" << m_sounds.keys().count();
#endif
    foreach (const QString& key, m_sounds.keys()) {
        QDeclarativeSound *sound = qobject_cast<QDeclarativeSound*>(
                                   qvariant_cast<QObject*>(m_sounds[key]));

        if (!sound) {
            qWarning() << "accessing invalid sound[" << key << "]";
            continue;
        }
        QDeclarativeAudioCategory *category = m_defaultCategory;
        if (m_categories.contains(sound->category())) {
            category = qobject_cast<QDeclarativeAudioCategory*>(
                       qvariant_cast<QObject*>(m_categories[sound->category()]));
        }
        sound->setCategoryObject(category);

        QDeclarativeAttenuationModel *attenuationModel = 0;
        if (sound->attenuationModel().isEmpty()) {
            if (m_defaultAttenuationModel)
                attenuationModel = m_defaultAttenuationModel;
        } else if (m_attenuationModels.contains(sound->attenuationModel())){
            attenuationModel = m_attenuationModels[sound->attenuationModel()];
        } else {
            qWarning() << "Sound[" << sound->name() << "] contains invalid attenuationModel["
                       << sound->attenuationModel() << "]";
        }
        sound->setAttenuationModelObject(attenuationModel);

        foreach (QDeclarativePlayVariation* playVariation, sound->playlist()) {
            if (m_samples.contains(playVariation->sample())) {
                playVariation->setSampleObject(
                            qobject_cast<QDeclarativeAudioSample*>(
                            qvariant_cast<QObject*>(m_samples[playVariation->sample()])));
            } else {
                qWarning() << "Sound[" << sound->name() << "] contains invalid sample["
                           << playVariation->sample() << "] for its playVarations";
            }
        }
    }
    m_complete = true;
#ifdef DEBUG_AUDIOENGINE
    qDebug() << "AudioEngine ready.";
#endif
    emit ready();
}

void QDeclarativeAudioEngine::updateSoundInstances()
{
    for (QList<QDeclarativeSoundInstance*>::Iterator it = m_managedDeclSoundInstances.begin();
         it != m_managedDeclSoundInstances.end();) {
        QDeclarativeSoundInstance *declSndInstance = *it;
        if (declSndInstance->state() == QDeclarativeSoundInstance::StoppedState) {
            it = m_managedDeclSoundInstances.erase(it);
            releaseManagedDeclarativeSoundInstance(declSndInstance);
#ifdef DEBUG_AUDIOENGINE
    qDebug() << "AudioEngine removed managed sounce instance";
#endif
        } else {
            declSndInstance->updatePosition(qreal(0.1));
            ++it;
        }
    }

    QVector3D listenerPosition = this->listener()->position();
    foreach (QSoundInstance *instance, m_activeSoundInstances) {
        if (instance->state() == QSoundInstance::PlayingState
            &&  instance->attenuationEnabled()) {
            instance->update3DVolume(listenerPosition);
        }
    }

    if (m_activeSoundInstances.count() == 0)
        m_updateTimer.stop();
}

void QDeclarativeAudioEngine::appendFunction(QQmlListProperty<QObject> *property, QObject *value)
{
    QDeclarativeAudioEngine* engine = static_cast<QDeclarativeAudioEngine*>(property->object);
    if (engine->m_complete) {
        qWarning("AudioEngine: cannot add child after initialization!");
        return;
    }

    QDeclarativeSound *sound = qobject_cast<QDeclarativeSound*>(value);
    if (sound) {
#ifdef DEBUG_AUDIOENGINE
        qDebug() << "add QDeclarativeSound[" << sound->name() << "]";
#endif
        if (engine->m_sounds.contains(sound->name())) {
            qWarning() << "Failed to add Sound[" << sound->name() << "], already exists!";
            return;
        }
        engine->m_sounds.insert(sound->name(), QVariant::fromValue(value));
        return;
    }

    QDeclarativeAudioSample *sample = qobject_cast<QDeclarativeAudioSample*>(value);
    if (sample) {
#ifdef DEBUG_AUDIOENGINE
        qDebug() << "add QDeclarativeAudioSample[" << sample->name() << "]";
#endif
        if (engine->m_samples.contains(sample->name())) {
            qWarning() << "Failed to add AudioSample[" << sample->name() << "], already exists!";
            return;
        }
        engine->m_samples.insert(sample->name(), QVariant::fromValue(value));
        return;
    }

    QDeclarativeAudioCategory *category = qobject_cast<QDeclarativeAudioCategory*>(value);
    if (category) {
#ifdef DEBUG_AUDIOENGINE
        qDebug() << "add QDeclarativeAudioCategory[" << category->name() << "]";
#endif
        if (engine->m_categories.contains(category->name())) {
            qWarning() << "Failed to add AudioCategory[" << category->name() << "], already exists!";
            return;
        }
        engine->m_categories.insert(category->name(), QVariant::fromValue(value));
        if (category->name() == QLatin1String("default")) {
            engine->m_defaultCategory = category;
        }
    }

    QDeclarativeAttenuationModel *attenModel = qobject_cast<QDeclarativeAttenuationModel*>(value);
    if (attenModel) {
#ifdef DEBUG_AUDIOENGINE
        qDebug() << "add AttenuationModel[" << attenModel->name() << "]";
#endif
        if (attenModel->name() == QLatin1String("default")) {
            engine->m_defaultAttenuationModel = attenModel;
        }
        if (engine->m_attenuationModels.contains(attenModel->name())) {
            qWarning() << "Failed to add AttenuationModel[" << attenModel->name() << "], already exists!";
            return;
        }
        engine->m_attenuationModels.insert(attenModel->name(), attenModel);
        return;
    }

    qWarning("Unknown child type for AudioEngine!");
}

QQmlListProperty<QObject> QDeclarativeAudioEngine::bank()
{
    return QQmlListProperty<QObject>(this, 0, appendFunction, 0, 0, 0);
}

/*!
    \qmlproperty map QtAudioEngine::AudioEngine::categories

    Container of all AudioCategory instances.
*/
QObject* QDeclarativeAudioEngine::categories()
{
    return &m_categories;
}

/*!
    \qmlproperty map QtAudioEngine::AudioEngine::samples

    Container of all AudioSample instances.
*/
QObject* QDeclarativeAudioEngine::samples()
{
    return &m_samples;
}

/*!
    \qmlproperty map QtAudioEngine::AudioEngine::sounds

    Container of all Sound instances.
*/
QObject* QDeclarativeAudioEngine::sounds()
{
    return &m_sounds;
}

/*!
    \qmlproperty QtAudioEngine::AudioListener QtAudioEngine::AudioEngine::listener

    This property holds the listener object.  You can change various
    properties to affect the 3D positioning of sounds.

    \sa AudioListener
*/
QDeclarativeAudioListener* QDeclarativeAudioEngine::listener() const
{
    return m_listener;
}

/*!
    \qmlproperty real QtAudioEngine::AudioEngine::dopplerFactor

    This property holds a simple scaling for the effect of doppler shift.
*/
qreal QDeclarativeAudioEngine::dopplerFactor() const
{
    return m_audioEngine->dopplerFactor();
}

void QDeclarativeAudioEngine::setDopplerFactor(qreal dopplerFactor)
{
    m_audioEngine->setDopplerFactor(dopplerFactor);
}

/*!
    \qmlproperty real QtAudioEngine::AudioEngine::speedOfSound

    This property holds the reference value of the sound speed (in meters per second)
    which will be used in doppler shift calculation. The doppler shift calculation is
    used to emulate the change in frequency in sound that is perceived by an observer when
    the sound source is travelling towards or away from the observer. The speed of sound
    depends on the medium the sound is propagating through.
*/
qreal QDeclarativeAudioEngine::speedOfSound() const
{
    return m_audioEngine->speedOfSound();
}

void QDeclarativeAudioEngine::setSpeedOfSound(qreal speedOfSound)
{
    m_audioEngine->setSpeedOfSound(speedOfSound);
}

/*!
    \qmlproperty bool QtAudioEngine::AudioEngine::loading

    This property indicates if the audio engine is loading any audio sample at the moment. This may
    be useful if you specified the preloaded property in AudioSample and would like to show a loading screen
    to the user before all audio samples are loaded.

    /sa finishedLoading, AudioSample::preloaded
*/
bool QDeclarativeAudioEngine::isLoading() const
{
    return m_audioEngine->isLoading();
}

void QDeclarativeAudioEngine::handleLoadingChanged()
{
    if (!isLoading())
        emit finishedLoading();
}

/*!
    \qmlsignal QtAudioEngine::AudioEngine::finishedLoading()

    This signal is emitted when \l loading has completed.

    The corresponding handler is \c onFinishedLoading.
*/

/*!
    \qmlsignal QtAudioEngine::AudioEngine::ready()

    This signal is emitted when the AudioEngine is ready to use.

    The corresponding handler is \c onReady.
*/

/*!
    \qmlsignal QtAudioEngine::AudioEngine::liveInstanceCountChanged()

    This signal is emitted when the number of live instances managed by the
    AudioEngine is changed.

    The corresponding handler is \c onLiveInstanceCountChanged.
*/

/*!
    \qmlsignal QtAudioEngine::AudioEngine::isLoadingChanged()

    This signal is emitted when the \l loading property changes.

    The corresponding handler is \c onIsLoadingChanged.
*/

QT_END_NAMESPACE
