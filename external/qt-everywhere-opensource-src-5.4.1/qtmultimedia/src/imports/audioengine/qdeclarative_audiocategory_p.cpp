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

#include "qdeclarative_audiocategory_p.h"
#include "qdebug.h"

#define DEBUG_AUDIOENGINE

QT_USE_NAMESPACE

/*!
    \qmltype AudioCategory
    \instantiates QDeclarativeAudioCategory
    \since 5.0
    \brief Control all active sound instances by group.
    \inqmlmodule QtAudioEngine
    \ingroup multimedia_audioengine
    \inherits Item
    \preliminary

    This type is part of the \b{QtAudioEngine 1.0} module.

    An instance of AudioCategory can be accessed through \l {QtAudioEngine::AudioEngine::categories}
    {AudioEngine.categories} with its unique name and must be defined inside AudioEngine.

    \qml
    import QtQuick 2.0
    import QtAudioEngine 1.0

    Rectangle {
        color:"white"
        width: 300
        height: 500

        AudioEngine {
            id:audioengine

            AudioCategory {
                name: "sfx"
                volume: 0.8
            }

            AudioSample {
                name:"explosion"
                source: "explosion-02.wav"
            }

            Sound {
                name:"explosion"
                category: "sfx"
                PlayVariation {
                    sample:"explosion"
                }
            }
        }

        MouseArea {
            anchors.fill: parent
            onPressed: {
                audioengine.categories["sfx"].volume = 0.5;
            }
        }
    }
    \endqml

    \l Sound instances can be grouped together by specifying the category property. When you change the
    volume of a category, all audio output from related instances will be affected as well.

    Note: there will always be an AudioCategory named \c default whether you explicitly define it or
    not. If you do not specify any category for a \l Sound, it will be grouped into the \c default
    category.

*/
QDeclarativeAudioCategory::QDeclarativeAudioCategory(QObject *parent)
    : QObject(parent)
    , m_complete(false)
    , m_volume(1)
{
}

QDeclarativeAudioCategory::~QDeclarativeAudioCategory()
{
}

void QDeclarativeAudioCategory::classBegin()
{
    if (!parent() || !parent()->inherits("QDeclarativeAudioEngine")) {
        qWarning("AudioCategory must be defined inside AudioEngine!");
        return;
    }
}

void QDeclarativeAudioCategory::componentComplete()
{
    if (m_name.isEmpty()) {
        qWarning("AudioCategory must have a name!");
        return;
    }
    m_complete = true;
}

/*!
    \qmlproperty real QtAudioEngine::AudioCategory::volume

    This property holds the volume of the category and will modulate all audio output from the
    instances which belong to this category.
*/
qreal QDeclarativeAudioCategory::volume() const
{
    return m_volume;
}

void QDeclarativeAudioCategory::setVolume(qreal volume)
{
    if (m_volume == volume)
        return;
    m_volume = volume;
    emit volumeChanged(m_volume);
#ifdef DEBUG_AUDIOENGINE
    qDebug() << "QDeclarativeAudioCategory[" << m_name << "] setVolume(" << volume << ")";
#endif
}

/*!
    \qmlproperty string QtAudioEngine::AudioCategory::name

    This property holds the name of AudioCategory. The name must be unique among all categories and only
    defined once. The name cannot be changed after the instance has been initialized.
*/
void QDeclarativeAudioCategory::setName(const QString& name)
{
    if (m_complete) {
        qWarning("AudioCategory: you can not change name after initialization.");
        return;
    }
    m_name = name;
}

QString QDeclarativeAudioCategory::name() const
{
    return m_name;
}

/*!
    \qmlmethod QtAudioEngine::AudioCategory::stop()

    Stops all active sound instances which belong to this category.
*/
void QDeclarativeAudioCategory::stop()
{
    emit stopped();
}

/*!
    \qmlmethod QtAudioEngine::AudioCategory::pause()

    Pauses all active sound instances which belong to this category.
*/
void QDeclarativeAudioCategory::pause()
{
    emit paused();
}

/*!
    \qmlmethod QtAudioEngine::AudioCategory::pause()

    Resumes all active sound instances from paused state which belong to this category.
*/
void QDeclarativeAudioCategory::resume()
{
    emit resumed();
}
