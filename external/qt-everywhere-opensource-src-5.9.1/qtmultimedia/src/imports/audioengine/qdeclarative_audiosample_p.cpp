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

#include "qdeclarative_audiosample_p.h"
#include "qdeclarative_audioengine_p.h"
#include "qdebug.h"
#include "qsoundbuffer_p.h"
#include "qaudioengine_p.h"

#define DEBUG_AUDIOENGINE

QT_USE_NAMESPACE

/*!
    \qmltype AudioSample
    \instantiates QDeclarativeAudioSample
    \since 5.0
    \brief Load audio samples, mostly .wav.
    \inqmlmodule QtAudioEngine
    \ingroup multimedia_audioengine
    \inherits Item
    \preliminary

    It can be accessed through QtAudioEngine::AudioEngine::samples with its unique
    name and must be defined inside AudioEngine or be added to it using
    \l{QtAudioEngine::AudioEngine::addAudioSample()}{AudioEngine.addAudioSample()}
    if AudioSample is created dynamically.

    \qml
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
        }
    }
    \endqml
*/
QDeclarativeAudioSample::QDeclarativeAudioSample(QObject *parent)
    : QObject(parent)
    , m_streaming(false)
    , m_preloaded(false)
    , m_soundBuffer(0)
    , m_engine(0)
{
}

QDeclarativeAudioSample::~QDeclarativeAudioSample()
{
}

/*!
    \qmlproperty url QtAudioEngine::AudioSample::source

    This property holds the source URL of the audio sample.
*/
QUrl QDeclarativeAudioSample::source() const
{
    return m_url;
}

void QDeclarativeAudioSample::setSource(const QUrl& url)
{
    if (m_engine) {
        qWarning("AudioSample: source not changeable after initialization.");
        return;
    }
    m_url = url;
}

bool QDeclarativeAudioSample::isStreaming() const
{
    return m_streaming;
}

QDeclarativeAudioEngine *QDeclarativeAudioSample::engine() const
{
    return m_engine;
}

/*!
    \qmlproperty bool QtAudioEngine::AudioSample::preloaded

    This property indicates whether this sample needs to be preloaded or not.
    If \c true, the audio engine will start loading the sample file immediately
    when the application starts, otherwise the sample will not be loaded until
    explicitly requested.
*/

bool QDeclarativeAudioSample::isPreloaded() const
{
    return m_preloaded;
}

/*!
    \qmlproperty bool QtAudioEngine::AudioSample::loaded

    This property indicates whether this sample has been loaded into memory or not.
*/
bool QDeclarativeAudioSample::isLoaded() const
{
    if (!m_soundBuffer)
        return false;
    return m_soundBuffer->state() == QSoundBuffer::Ready;
}

/*!
    \qmlmethod void QtAudioEngine::AudioSample::load()

    Starts loading the sample into memory if not loaded.
*/
void QDeclarativeAudioSample::load()
{
    if (!m_soundBuffer) {
        m_preloaded = true;
        return;
    }
    if (m_soundBuffer->state() != QSoundBuffer::Loading && m_soundBuffer->state() != QSoundBuffer::Ready)
        m_soundBuffer->load();
}

void QDeclarativeAudioSample::setPreloaded(bool preloaded)
{
    if (m_engine) {
        qWarning("AudioSample: preloaded not changeable after initialization.");
        return;
    }
    m_preloaded = preloaded;
}

void QDeclarativeAudioSample::setStreaming(bool streaming)
{
    if (m_engine) {
        qWarning("AudioSample: streaming not changeable after initialization.");
        return;
    }
    m_streaming = streaming;
}

void QDeclarativeAudioSample::setEngine(QDeclarativeAudioEngine *engine)
{
    if (m_engine) {
        qWarning("AudioSample: engine not changeable after initialization.");
        return;
    }
    m_engine = engine;
}

/*!
    \qmlproperty string QtAudioEngine::AudioSample::name

    This property holds the name of the sample, which must be unique among all
    samples and only defined once.
*/
QString QDeclarativeAudioSample::name() const
{
    return m_name;
}

void QDeclarativeAudioSample::setName(const QString& name)
{
    if (m_engine) {
        qWarning("AudioSample: name not changeable after initialization.");
        return;
    }
    m_name = name;
}

void QDeclarativeAudioSample::init()
{
    Q_ASSERT(m_engine != 0);

    if (m_streaming) {
        //TODO

    } else {
        m_soundBuffer = m_engine->engine()->getStaticSoundBuffer(m_url);
        if (m_soundBuffer->state() == QSoundBuffer::Ready) {
            emit loadedChanged();
        } else {
            connect(m_soundBuffer, SIGNAL(ready()), this, SIGNAL(loadedChanged()));
        }
        if (m_preloaded) {
            m_soundBuffer->load();
        }
    }
}

QSoundBuffer* QDeclarativeAudioSample::soundBuffer() const
{
    return m_soundBuffer;
}

/*!
    \qmlsignal QtAudioEngine::AudioSample::loadedChanged()

    This signal is emitted when \l loaded is changed.

    The corresponding handler is \c onLoadedChanged.
*/


