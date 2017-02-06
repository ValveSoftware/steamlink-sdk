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

#include "qdeclarative_attenuationmodel_p.h"
#include "qdeclarative_audioengine_p.h"
#include "qdebug.h"

#define DEBUG_AUDIOENGINE

QT_USE_NAMESPACE

QDeclarativeAttenuationModel::QDeclarativeAttenuationModel(QObject *parent)
    : QObject(parent)
    , m_engine(0)
{
}

QDeclarativeAttenuationModel::~QDeclarativeAttenuationModel()
{
}

void QDeclarativeAttenuationModel::setEngine(QDeclarativeAudioEngine *engine)
{
    m_engine = engine;
}

QString QDeclarativeAttenuationModel::name() const
{
    return m_name;
}

void QDeclarativeAttenuationModel::setName(const QString& name)
{
    if (m_engine) {
        qWarning("AttenuationModel: you can not change name after initialization.");
        return;
    }
    m_name = name;
}

//////////////////////////////////////////////////////////////////////////////////////////
/*!
    \qmltype AttenuationModelLinear
    \instantiates QDeclarativeAttenuationModelLinear
    \since 5.0
    \brief Defines a linear attenuation curve for a \l Sound.
    \inqmlmodule QtAudioEngine
    \ingroup multimedia_audioengine
    \inherits Item
    \preliminary

    AttenuationModelLinear must be defined inside \l AudioEngine or be added to it using
    \l{QtAudioEngine::AudioEngine::addAttenuationModel()}{AudioEngine.addAttenuationModel()}
    if AttenuationModelLinear is created dynamically.

    \qml
    Rectangle {
        color:"white"
        width: 300
        height: 500

        AudioEngine {
            id:audioengine

            AttenuationModelLinear {
               name:"linear"
               start: 20
               end: 180
            }

            AudioSample {
                name:"explosion"
                source: "explosion-02.wav"
            }

            Sound {
                name:"explosion"
                attenuationModel: "linear"
                PlayVariation {
                    sample:"explosion"
                }
            }
        }
    }
    \endqml
*/

/*!
    \qmlproperty string QtAudioEngine::AttenuationModelLinear::name

    This property holds the name of AttenuationModelLinear, must be unique among all attenuation
    models and only defined once.
*/
QDeclarativeAttenuationModelLinear::QDeclarativeAttenuationModelLinear(QObject *parent)
    : QDeclarativeAttenuationModel(parent)
    , m_start(0)
    , m_end(1)
{
}

void QDeclarativeAttenuationModelLinear::setEngine(QDeclarativeAudioEngine *engine)
{
    if (m_start > m_end) {
        qSwap(m_start, m_end);
        qWarning() << "AttenuationModelLinear[" << m_name << "]: start must be less or equal than end.";
    }
    QDeclarativeAttenuationModel::setEngine(engine);
}

/*!
    \qmlproperty real QtAudioEngine::AttenuationModelLinear::start

    This property holds the start distance. There will be no attenuation if the distance from sound
    to listener is within this range.
    The default value is 0.
*/
qreal QDeclarativeAttenuationModelLinear::startDistance() const
{
    return m_start;
}

void QDeclarativeAttenuationModelLinear::setStartDistance(qreal startDist)
{
    if (m_engine) {
        qWarning() << "AttenuationModelLinear[" << m_name << "]: you can not change properties after initialization.";
        return;
    }
    if (startDist < 0) {
        qWarning() << "AttenuationModelLinear[" << m_name << "]: start must be no less than 0.";
        return;
    }
    m_start = startDist;
}

/*!
    \qmlproperty real QtAudioEngine::AttenuationModelLinear::end

    This property holds the end distance. There will be no sound hearable if the distance from sound
    to listener is larger than this.
    The default value is 1.
*/
qreal QDeclarativeAttenuationModelLinear::endDistance() const
{
    return m_end;
}

void QDeclarativeAttenuationModelLinear::setEndDistance(qreal endDist)
{
    if (m_engine) {
        qWarning() << "AttenuationModelLinear[" << m_name << "]: you can not change properties after initialization.";
        return;
    }
    if (endDist < 0) {
        qWarning() << "AttenuationModelLinear[" << m_name << "]: end must be no greater than 0.";
        return;
    }
    m_end = endDist;
}

qreal QDeclarativeAttenuationModelLinear::calculateGain(const QVector3D &listenerPosition, const QVector3D &sourcePosition) const
{
    qreal md = m_end - m_start;
    if (md == 0)
        return 1;
    qreal d = qBound(qreal(0), (listenerPosition - sourcePosition).length() - m_start, md);
    return qreal(1) - (d / md);
}

//////////////////////////////////////////////////////////////////////////////////////////
/*!
    \qmltype AttenuationModelInverse
    \instantiates QDeclarativeAttenuationModelInverse

    \since 5.0
    \brief Defines a non-linear attenuation curve for a \l Sound.
    \inqmlmodule QtAudioEngine
    \ingroup multimedia_audioengine
    \inherits Item
    \preliminary

    AttenuationModelInverse must be defined inside \l AudioEngine or be added to it using
    \l{QtAudioEngine::AudioEngine::addAttenuationModel()}{AudioEngine.addAttenuationModel()}
    if AttenuationModelInverse is created dynamically.

    \qml
    Rectangle {
        color:"white"
        width: 300
        height: 500

        AudioEngine {
            id:audioengine

            AttenuationModelInverse {
               name:"linear"
               start: 20
               end: 500
               rolloff: 1.5
            }

            AudioSample {
                name:"explosion"
                source: "explosion-02.wav"
            }

            Sound {
                name:"explosion"
                attenuationModel: "linear"
                PlayVariation {
                    sample:"explosion"
                }
            }
        }
    }
    \endqml

    Attenuation factor is calculated as below:

    distance: distance from sound to listener
    d = min(max(distance, start), end);
    attenuation = start / (start + (d - start) * rolloff);
*/

/*!
    \qmlproperty string QtAudioEngine::AttenuationModelInverse::name

    This property holds the name of AttenuationModelInverse, must be unique among all attenuation
    models and only defined once.
*/

/*!
    \qmlproperty real QtAudioEngine::AttenuationModelInverse::start

    This property holds the start distance. There will be no attenuation if the distance from sound
    to listener is within this range.
    The default value is 1.
*/

/*!
    \qmlproperty real QtAudioEngine::AttenuationModelInverse::end

    This property holds the end distance. There will be no further attenuation if the distance from
    sound to listener is larger than this.
    The default value is 1000.
*/

/*!
    \qmlproperty real QtAudioEngine::AttenuationModelInverse::rolloff

    This property holds the rolloff factor. The bigger the value is, the faster the sound attenuates.
    The default value is 1.
*/

QDeclarativeAttenuationModelInverse::QDeclarativeAttenuationModelInverse(QObject *parent)
    : QDeclarativeAttenuationModel(parent)
    , m_ref(1)
    , m_max(1000)
    , m_rolloff(1)
{
}

void QDeclarativeAttenuationModelInverse::setEngine(QDeclarativeAudioEngine *engine)
{
    if (m_ref > m_max) {
        qSwap(m_ref, m_max);
        qWarning() << "AttenuationModelInverse[" << m_name << "]: referenceDistance must be less or equal than maxDistance.";
    }
    QDeclarativeAttenuationModel::setEngine(engine);
}

qreal QDeclarativeAttenuationModelInverse::referenceDistance() const
{
    return m_ref;
}

void QDeclarativeAttenuationModelInverse::setReferenceDistance(qreal referenceDistance)
{
    if (m_engine) {
        qWarning() << "AttenuationModelInverse[" << m_name << "]: you can not change properties after initialization.";
        return;
    }
    if (referenceDistance <= 0) {
        qWarning() << "AttenuationModelInverse[" << m_name << "]: referenceDistance must be greater than 0.";
        return;
    }
    m_ref = referenceDistance;
}

qreal QDeclarativeAttenuationModelInverse::maxDistance() const
{
    return m_max;
}

void QDeclarativeAttenuationModelInverse::setMaxDistance(qreal maxDistance)
{
    if (m_engine) {
        qWarning() << "AttenuationModelInverse[" << m_name << "]: you can not change properties after initialization.";
        return;
    }
    if (maxDistance <= 0) {
        qWarning() << "AttenuationModelInverse[" << m_name << "]: maxDistance must be greater than 0.";
        return;
    }
    m_max = maxDistance;
}

qreal QDeclarativeAttenuationModelInverse::rolloffFactor() const
{
    return m_rolloff;
}

void QDeclarativeAttenuationModelInverse::setRolloffFactor(qreal rolloffFactor)
{
    if (m_engine) {
        qWarning() << "AttenuationModelInverse[" << m_name << "]: you can not change properties after initialization.";
        return;
    }
    m_rolloff = rolloffFactor;
}

qreal QDeclarativeAttenuationModelInverse::calculateGain(const QVector3D &listenerPosition, const QVector3D &sourcePosition) const
{
    Q_ASSERT(m_ref > 0);
    return m_ref / (m_ref + (qBound<qreal>(m_ref, (listenerPosition - sourcePosition).length(), m_max) - m_ref) * m_rolloff);
}

