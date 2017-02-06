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

#include <QtCore/QMutex>
#include <QtCore/QThread>
#include <QtNetwork/QNetworkRequest>
#include <QtNetwork/QNetworkReply>
#include <QtNetwork/QNetworkAccessManager>

#include "qsamplecache_p.h"

#include "qdebug.h"

#define DEBUG_AUDIOENGINE

QT_USE_NAMESPACE

QSoundBufferPrivateAL::QSoundBufferPrivateAL(QObject *parent)
    : QSoundBuffer(parent)
{
}


StaticSoundBufferAL::StaticSoundBufferAL(QObject *parent, const QUrl &url, QSampleCache *sampleLoader)
    : QSoundBufferPrivateAL(parent),
      m_ref(1),
      m_url(url),
      m_alBuffer(0),
      m_state(Creating),
      m_sample(0),
      m_sampleLoader(sampleLoader)
{
#ifdef DEBUG_AUDIOENGINE
    qDebug() << "creating new StaticSoundBufferOpenAL";
#endif
}

StaticSoundBufferAL::~StaticSoundBufferAL()
{
    if (m_sample)
        m_sample->release();

    if (m_alBuffer != 0) {
        alGetError(); // clear error
        alDeleteBuffers(1, &m_alBuffer);
        QAudioEnginePrivate::checkNoError("delete buffer");
    }
}

QSoundBuffer::State StaticSoundBufferAL::state() const
{
    return m_state;
}

void StaticSoundBufferAL::load()
{
    if (m_state == Loading || m_state == Ready)
        return;

    m_state = Loading;
    emit stateChanged(m_state);

    m_sample = m_sampleLoader->requestSample(m_url);
    connect(m_sample, SIGNAL(error()), this, SLOT(decoderError()));
    connect(m_sample, SIGNAL(ready()), this, SLOT(sampleReady()));
    switch (m_sample->state()) {
    case QSample::Ready:
        sampleReady();
        break;
    case QSample::Error:
        decoderError();
        break;
    default:
        break;
    }
}

void StaticSoundBufferAL::bindToSource(ALuint alSource)
{
    Q_ASSERT(m_alBuffer != 0);
    alSourcei(alSource, AL_BUFFER, m_alBuffer);
}

void StaticSoundBufferAL::unbindFromSource(ALuint alSource)
{
    alSourcei(alSource, AL_BUFFER, 0);
}

void StaticSoundBufferAL::sampleReady()
{
#ifdef DEBUG_AUDIOENGINE
    qDebug() << "StaticSoundBufferOpenAL:sample[" << m_url << "] loaded";
#endif

    disconnect(m_sample, SIGNAL(error()), this, SLOT(decoderError()));
    disconnect(m_sample, SIGNAL(ready()), this, SLOT(sampleReady()));

    if (m_sample->data().size() > 1024 * 1024 * 4) {
        qWarning() << "source [" << m_url << "] size too large!";
        decoderError();
        return;
    }

    if (m_sample->format().channelCount() > 2) {
        qWarning() << "source [" << m_url << "] channel > 2!";
        decoderError();
        return;
    }

    ALenum alFormat = 0;
    if (m_sample->format().sampleSize() == 8) {
        alFormat = m_sample->format().channelCount() == 1 ? AL_FORMAT_MONO8 : AL_FORMAT_STEREO8;
    } else if (m_sample->format().sampleSize() == 16) {
        alFormat = m_sample->format().channelCount() == 1 ? AL_FORMAT_MONO16 : AL_FORMAT_STEREO16;
    } else {
        qWarning() << "source [" << m_url << "] invalid sample size:"
                   << m_sample->format().sampleSize() << "(should be 8 or 16)";
        decoderError();
        return;
    }

    alGenBuffers(1, &m_alBuffer);
    if (!QAudioEnginePrivate::checkNoError("create buffer")) {
        decoderError();
        return;
    }
    alBufferData(m_alBuffer, alFormat, m_sample->data().data(),
                 m_sample->data().size(), m_sample->format().sampleRate());
    if (!QAudioEnginePrivate::checkNoError("fill buffer")) {
        decoderError();
        return;
    }

    m_sample->release();
    m_sample = 0;

    m_state = Ready;
    emit stateChanged(m_state);
    emit ready();
}

void StaticSoundBufferAL::decoderError()
{
    qWarning() << "loading [" << m_url << "] failed";

    disconnect(m_sample, SIGNAL(error()), this, SLOT(decoderError()));
    disconnect(m_sample, SIGNAL(ready()), this, SLOT(sampleReady()));

    m_sample->release();
    m_sample = 0;

    m_state = Error;
    emit stateChanged(m_state);
    emit error();
}


/////////////////////////////////////////////////////////////////
QAudioEnginePrivate::QAudioEnginePrivate(QObject *parent)
    : QObject(parent)
{
    m_updateTimer.setInterval(200);
    connect(&m_updateTimer, SIGNAL(timeout()), this, SLOT(updateSoundSources()));

    m_sampleLoader = new QSampleCache(this);
    m_sampleLoader->setCapacity(0);
    connect(m_sampleLoader, SIGNAL(isLoadingChanged()), this, SIGNAL(isLoadingChanged()));

#ifdef DEBUG_AUDIOENGINE
    qDebug() << "default openal device = " << alcGetString(NULL, ALC_DEFAULT_DEVICE_SPECIFIER);

    const ALCchar* devNames = alcGetString(NULL, ALC_DEVICE_SPECIFIER);
    int cc = 0;
    qDebug() << "device list:";
    while (true) {
        qDebug() << "    " << devNames + cc;
        while (devNames[cc] != 0)
            ++cc;
        ++cc;
        if (devNames[cc] == 0)
            break;
    }
#endif

    ALCdevice *device = alcOpenDevice(0);
    if (!device) {
        qWarning() << "Can not create openal device!";
        return;
    }

    ALCcontext* context = alcCreateContext(device, 0);
    if (!context) {
        qWarning() << "Can not create openal context!";
        return;
    }
    alcMakeContextCurrent(context);
    alDistanceModel(AL_NONE);
    alDopplerFactor(0);
}

QAudioEnginePrivate::~QAudioEnginePrivate()
{
#ifdef DEBUG_AUDIOENGINE
    qDebug() << "QAudioEnginePrivate::dtor";
#endif
    const QObjectList children = this->children();
    for (QObject *child : children) {
        QSoundSourcePrivate* s = qobject_cast<QSoundSourcePrivate*>(child);
        if (!s)
            continue;
        s->release();
    }

    for (QSoundBufferPrivateAL *buffer : qAsConst(m_staticBufferPool)) {
        delete buffer;
    }
    m_staticBufferPool.clear();

    delete m_sampleLoader;

    ALCcontext* context = alcGetCurrentContext();
    ALCdevice *device = alcGetContextsDevice(context);
    alcDestroyContext(context);
    alcCloseDevice(device);
#ifdef DEBUG_AUDIOENGINE
    qDebug() << "QAudioEnginePrivate::dtor: all done";
#endif
}

bool QAudioEnginePrivate::isLoading() const
{
    return m_sampleLoader->isLoading();
}

QSoundSource* QAudioEnginePrivate::createSoundSource()
{
#ifdef DEBUG_AUDIOENGINE
    qDebug() << "QAudioEnginePrivate::createSoundSource()";
#endif
    QSoundSourcePrivate *instance = NULL;
    if (m_instancePool.count() == 0) {
        instance = new QSoundSourcePrivate(this);
    } else {
        instance = m_instancePool.front();
        m_instancePool.pop_front();
    }
    connect(instance, SIGNAL(activate(QObject*)), this, SLOT(soundSourceActivate(QObject*)));
    return instance;
}

void QAudioEnginePrivate::releaseSoundSource(QSoundSource *soundInstance)
{
    QSoundSourcePrivate *privInstance = static_cast<QSoundSourcePrivate*>(soundInstance);
#ifdef DEBUG_AUDIOENGINE
    qDebug() << "recycle soundInstance" << privInstance;
#endif
    privInstance->unbindBuffer();
    m_instancePool.push_front(privInstance);
    m_activeInstances.removeOne(privInstance);
}

QSoundBuffer* QAudioEnginePrivate::getStaticSoundBuffer(const QUrl& url)
{
    StaticSoundBufferAL *staticBuffer = NULL;
    QMap<QUrl, QSoundBufferPrivateAL*>::iterator it = m_staticBufferPool.find(url);
    if (it == m_staticBufferPool.end()) {
        staticBuffer = new StaticSoundBufferAL(this, url, m_sampleLoader);
        m_staticBufferPool.insert(url, staticBuffer);
    } else {
        staticBuffer = static_cast<StaticSoundBufferAL*>(*it);
        staticBuffer->addRef();
    }
    return staticBuffer;
}

void QAudioEnginePrivate::releaseSoundBuffer(QSoundBuffer *buffer)
{
#ifdef DEBUG_AUDIOENGINE
    qDebug() << "QAudioEnginePrivate: recycle sound buffer";
#endif
    if (StaticSoundBufferAL *staticBuffer = qobject_cast<StaticSoundBufferAL *>(buffer)) {
        //decrement the reference count, still kept in memory for reuse
        staticBuffer->release();
        //TODO implement some resource recycle strategy
    } else {
        //TODO
        Q_ASSERT(0);
        qWarning() << "Unknown soundbuffer type for recycle" << buffer;
    }
}

bool QAudioEnginePrivate::checkNoError(const char *msg)
{
    ALenum error = alGetError();
    if (error != AL_NO_ERROR) {
        qWarning() << "Failed on" << msg << "[OpenAL error code =" << error << "]";
        return false;
    }
    return true;
}

QVector3D QAudioEnginePrivate::listenerPosition() const
{
    ALfloat x, y, z;
    alGetListener3f(AL_POSITION, &x, &y, &z);
    checkNoError("get listener position");
    return QVector3D(x, y, z);
}

QVector3D QAudioEnginePrivate::listenerVelocity() const
{
    ALfloat x, y, z;
    alGetListener3f(AL_VELOCITY, &x, &y, &z);
    checkNoError("get listener velocity");
    return QVector3D(x, y, z);
}

qreal QAudioEnginePrivate::listenerGain() const
{
    ALfloat gain;
    alGetListenerf(AL_GAIN, &gain);
    checkNoError("get listener gain");
    return gain;
}

void QAudioEnginePrivate::setListenerPosition(const QVector3D& position)
{
    alListener3f(AL_POSITION, position.x(), position.y(), position.z());
    checkNoError("set listener position");
}

void QAudioEnginePrivate::setListenerOrientation(const QVector3D& direction, const QVector3D& up)
{
    ALfloat orientation[6];
    orientation[0] = direction.x();
    orientation[1] = direction.y();
    orientation[2] = direction.z();
    orientation[3] = up.x();
    orientation[4] = up.y();
    orientation[5] = up.z();
    alListenerfv(AL_ORIENTATION, orientation);
    checkNoError("set listener orientation");
}

void QAudioEnginePrivate::setListenerVelocity(const QVector3D& velocity)
{
    alListener3f(AL_VELOCITY, velocity.x(), velocity.y(), velocity.z());
    checkNoError("set listener velocity");
}

void QAudioEnginePrivate::setListenerGain(qreal gain)
{
    alListenerf(AL_GAIN, gain);
    checkNoError("set listener gain");
}

void QAudioEnginePrivate::setDopplerFactor(qreal dopplerFactor)
{
    alDopplerFactor(dopplerFactor);
}

void QAudioEnginePrivate::setSpeedOfSound(qreal speedOfSound)
{
    alSpeedOfSound(speedOfSound);
}

void QAudioEnginePrivate::soundSourceActivate(QObject *soundSource)
{
    QSoundSourcePrivate *ss = qobject_cast<QSoundSourcePrivate*>(soundSource);
    ss->checkState();
    if (ss->isLooping())
        return;
    if (!m_activeInstances.contains(ss))
        m_activeInstances.push_back(ss);
    if (!m_updateTimer.isActive())
        m_updateTimer.start();
}

void QAudioEnginePrivate::updateSoundSources()
{
    for (QList<QSoundSourcePrivate*>::Iterator it = m_activeInstances.begin();
         it != m_activeInstances.end();) {
        QSoundSourcePrivate *instance = *it;
        instance->checkState();
        if (instance->state() == QSoundSource::StoppedState) {
            it = m_activeInstances.erase(it);
        } else {
            ++it;
        }
    }

    if (m_activeInstances.count() == 0) {
        m_updateTimer.stop();
    }
}
