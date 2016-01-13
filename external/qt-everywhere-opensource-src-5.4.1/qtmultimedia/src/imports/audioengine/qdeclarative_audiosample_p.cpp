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

    \c AudioSample is part of the \b{QtAudioEngine 1.0} module.

    It can be accessed through QtAudioEngine::AudioEngine::samples with its unique
    name and must be defined inside AudioEngine.

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
        }
    }
    \endqml
*/
QDeclarativeAudioSample::QDeclarativeAudioSample(QObject *parent)
    : QObject(parent)
    , m_complete(false)
    , m_streaming(false)
    , m_preloaded(false)
    , m_soundBuffer(0)
{
}

QDeclarativeAudioSample::~QDeclarativeAudioSample()
{
}

void QDeclarativeAudioSample::classBegin()
{
    if (!parent() || !parent()->inherits("QDeclarativeAudioEngine")) {
        qWarning("AudioSample must be defined inside AudioEngine!");
        return;
    }
}

void QDeclarativeAudioSample::componentComplete()
{
    if (m_name.isEmpty()) {
        qWarning("AudioSample must have a name!");
        return;
    }
    m_complete = true;
}

QUrl QDeclarativeAudioSample::source() const
{
    return m_url;
}

void QDeclarativeAudioSample::setSource(const QUrl& url)
{
    if (m_complete) {
        qWarning("AudioSample: source not changable after initialization.");
        return;
    }
    m_url = url;
}

bool QDeclarativeAudioSample::isStreaming() const
{
    return m_streaming;
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
    return m_soundBuffer->isReady();
}

/*!
    \qmlmethod void QtAudioEngine::AudioSample::load()

    Starts loading the sample into memory if not loaded.
*/
void QDeclarativeAudioSample::load()
{
    if (isLoaded())
        return;
    if (!m_soundBuffer) {
        m_preloaded = true;
        return;
    }
    m_soundBuffer->load();
}

void QDeclarativeAudioSample::setPreloaded(bool preloaded)
{
    if (m_complete) {
        qWarning("AudioSample: preloaded not changable after initialization.");
        return;
    }
    m_preloaded = preloaded;
}

void QDeclarativeAudioSample::setStreaming(bool streaming)
{
    if (m_complete) {
        qWarning("AudioSample: streaming not changable after initialization.");
        return;
    }
    m_streaming = streaming;
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
    if (m_complete) {
        qWarning("AudioSample: name not changable after initialization.");
        return;
    }
    m_name = name;
}

void QDeclarativeAudioSample::init()
{
    if (m_streaming) {
        //TODO

    } else {
        m_soundBuffer =
            qobject_cast<QDeclarativeAudioEngine*>(parent())->engine()->getStaticSoundBuffer(m_url);
        if (m_soundBuffer->isReady()) {
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


