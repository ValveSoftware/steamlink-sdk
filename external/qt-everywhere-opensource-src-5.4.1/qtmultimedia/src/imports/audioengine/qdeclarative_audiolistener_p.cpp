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

#include "qdeclarative_audiolistener_p.h"
#include "qdeclarative_audioengine_p.h"
#include "qdebug.h"

#define DEBUG_AUDIOENGINE

QT_USE_NAMESPACE

/*!
    \qmltype AudioListener
    \instantiates QDeclarativeAudioListener
    \since 5.0
    \brief Control global listener parameters.
    \inqmlmodule QtAudioEngine
    \ingroup multimedia_audioengine
    \inherits Item
    \preliminary

    This type is part of the \b{QtAudioEngine 1.0} module.

    AudioListener will have only one global instance and you can either access it through the
    listener property of AudioEngine:

    \qml
    import QtQuick 2.0
    import QtAudioEngine 1.0

    Rectangle {
        color:"white"
        width: 300
        height: 500

        AudioEngine {
            id:audioengine
            listener.up:"0,0,1"
            listener.velocity:"0,0,0"
            listener.direction:"0,1,0"
            listener.position:Qt.vector3d(observer.x, observer.y, 0);
        }

        Item {
            id: observer
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
    }
    \endqml

    or alternatively, by defining an AudioListener outside AudioEngine:

    \qml
    import QtQuick 2.0
    import QtAudioEngine 1.0

    Rectangle {
        color:"white"
        width: 300
        height: 500

        AudioEngine {
            id:audioengine
            listener.up:"0,0,1"
            listener.velocity:"0,0,0"
            listener.direction:"0,1,0"
        }

        AudioListener {
            engine:audioengine
            position: Qt.vector3d(observer.x, observer.y, 0);
        }

        Item {
            id: observer
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
    }
    \endqml

    This separate AudioListener definition is allowed to make QML bindings easier in some cases.
*/

QDeclarativeAudioListener::QDeclarativeAudioListener(QObject *parent)
    : QObject(parent)
    , m_engine(0)
{
    m_engine = qobject_cast<QDeclarativeAudioEngine*>(parent);
}

QDeclarativeAudioListener::~QDeclarativeAudioListener()
{
}

/*!
    \qmlproperty QtAudioEngine::AudioEngine QtAudioEngine::AudioListener::engine

    This property holds the reference to AudioEngine, and must only be set once.
*/
QDeclarativeAudioEngine* QDeclarativeAudioListener::engine() const
{
    return m_engine;
}

void QDeclarativeAudioListener::setEngine(QDeclarativeAudioEngine *engine)
{
    setParent(engine);
    m_engine = engine;
}

/*!
    \qmlproperty vector3d QtAudioEngine::AudioListener::position

    This property holds the 3D position of the listener.
*/
QVector3D QDeclarativeAudioListener::position() const
{
    return m_engine->engine()->listenerPosition();
}

void QDeclarativeAudioListener::setPosition(const QVector3D &position)
{
#ifdef DEBUG_AUDIOENGINE
    qDebug() << "QDeclarativeAudioListener::setPosition";
#endif
    m_engine->engine()->setListenerPosition(position);
    emit positionChanged();
}

/*!
    \qmlproperty vector3d QtAudioEngine::AudioListener::direction

    This property holds the normalized 3D direction vector of the listener.
*/
QVector3D QDeclarativeAudioListener::direction() const
{
    return m_engine->engine()->listenerDirection();
}

void QDeclarativeAudioListener::setDirection(const QVector3D &direction)
{
#ifdef DEBUG_AUDIOENGINE
    qDebug() << "QDeclarativeAudioListener::setDirection";
#endif
    m_engine->engine()->setListenerDirection(direction);
    emit directionChanged();
}

/*!
    \qmlproperty vector3d QtAudioEngine::AudioListener::velocity

    This property holds the 3D velocity vector of the listener.
*/
QVector3D QDeclarativeAudioListener::velocity() const
{
    return m_engine->engine()->listenerVelocity();
}

void QDeclarativeAudioListener::setVelocity(const QVector3D &velocity)
{
#ifdef DEBUG_AUDIOENGINE
    qDebug() << "QDeclarativeAudioListener::setVelocity";
#endif
    m_engine->engine()->setListenerVelocity(velocity);
    emit velocityChanged();
}

/*!
    \qmlproperty vector3d QtAudioEngine::AudioListener::up

    This property holds the normalized 3D up vector of the listener.
*/
QVector3D QDeclarativeAudioListener::up() const
{
    return m_engine->engine()->listenerUp();
}

void QDeclarativeAudioListener::setUp(const QVector3D &up)
{
#ifdef DEBUG_AUDIOENGINE
    qDebug() << "QDeclarativeAudioListener::setUp";
#endif
    m_engine->engine()->setListenerUp(up);
    emit upChanged();
}

/*!
    \qmlproperty real QtAudioEngine::AudioListener::gain

    This property will modulate all audio output from audio engine instances.
*/
qreal QDeclarativeAudioListener::gain() const
{
    return m_engine->engine()->listenerGain();
}

void QDeclarativeAudioListener::setGain(qreal gain)
{
#ifdef DEBUG_AUDIOENGINE
    qDebug() << "QDeclarativeAudioListener::setGain";
#endif
    m_engine->engine()->setListenerGain(gain);
    emit gainChanged();
}
