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

#ifndef QDECLARATIVEAUDIOENGINE_P_H
#define QDECLARATIVEAUDIOENGINE_P_H

#include <QtQml/qqml.h>
#include <QtQml/qqmlcomponent.h>
#include <QtQml/qqmlpropertymap.h>
#include <QtCore/QMap>
#include <QtCore/QList>
#include <QTimer>
#include "qaudioengine_p.h"

QT_BEGIN_NAMESPACE

class QDeclarativeSoundInstance;
class QDeclarativeAudioListener;
class QDeclarativeAudioCategory;
class QDeclarativeAudioSample;
class QDeclarativeSound;
class QDeclarativePlayVariation;
class QAudioCategory;
class QDeclarativeAttenuationModel;
class QSoundInstance;

class QDeclarativeAudioEngine : public QObject, public QQmlParserStatus
{
    Q_OBJECT
    Q_INTERFACES(QQmlParserStatus)
    Q_PROPERTY(QQmlListProperty<QObject> bank READ bank CONSTANT)
    Q_PROPERTY(QObject* categories READ categories CONSTANT)
    Q_PROPERTY(QObject* samples READ samples CONSTANT)
    Q_PROPERTY(QObject* sounds READ sounds CONSTANT)
    Q_PROPERTY(bool loading READ isLoading NOTIFY isLoadingChanged)
    Q_PROPERTY(int liveInstances READ liveInstanceCount NOTIFY liveInstanceCountChanged)
    Q_PROPERTY(QDeclarativeAudioListener* listener READ listener CONSTANT)
    Q_PROPERTY(qreal dopplerFactor READ dopplerFactor WRITE setDopplerFactor)
    Q_PROPERTY(qreal speedOfSound READ speedOfSound WRITE setSpeedOfSound)
    Q_CLASSINFO("DefaultProperty", "bank")

public:
    QDeclarativeAudioEngine(QObject *parent = 0);
    ~QDeclarativeAudioEngine();

    void classBegin();
    void componentComplete();

    //This is used for tracking all objects declared inside AudioEngine
    //see appendFunction
    QQmlListProperty<QObject> bank();

    QObject* categories();
    QObject* samples();
    QObject* sounds();

    QDeclarativeAudioListener* listener() const;

    qreal dopplerFactor() const;
    void setDopplerFactor(qreal dopplerFactor);

    qreal speedOfSound() const;
    void setSpeedOfSound(qreal speedOfSound);

    bool isLoading() const;

    int liveInstanceCount() const;

    //for child elements
    bool isReady() const;
    QAudioEngine* engine() const;

    //if managed, then the instance should start playing immediately and will be collected
    //when the playback finished
    QDeclarativeSoundInstance* newDeclarativeSoundInstance(bool managed);

    //internal sound instance is different from declarativeSoundInstance
    //declarative instance is more like a soundInstance helper which can
    //switch to different sound instance conveniently while sound instance
    //must be mapped to one sound definition.
    QSoundInstance* newSoundInstance(const QString &name);
    void releaseSoundInstance(QSoundInstance* instance);

Q_SIGNALS:
    void ready();
    void liveInstanceCountChanged();
    void isLoadingChanged();
    void finishedLoading();

private Q_SLOTS:
    void updateSoundInstances();
    void handleLoadingChanged();

private:
    Q_DISABLE_COPY(QDeclarativeAudioEngine);
    bool m_complete;

    //see bank()
    static void appendFunction(QQmlListProperty<QObject> *property, QObject *value);

    QDeclarativeAudioListener *m_listener;
    QDeclarativeAudioCategory* m_defaultCategory;
    QQmlPropertyMap m_categories;
    QQmlPropertyMap m_samples;
    QQmlPropertyMap m_sounds;

    //Use strong type here since no need to for AttenuationModel to be accessible directly by user
    //after config stage
    QDeclarativeAttenuationModel *m_defaultAttenuationModel;
    QMap<QString, QDeclarativeAttenuationModel*> m_attenuationModels;

    QAudioEngine *m_audioEngine;

    //for execution stage management
    QList<QSoundInstance*> m_soundInstancePool;
    QList<QSoundInstance*> m_activeSoundInstances;

    QTimer m_updateTimer;
    QList<QDeclarativeSoundInstance*> m_managedDeclSoundInstances;
    QList<QDeclarativeSoundInstance*> m_managedDeclSndInstancePool;
    void releaseManagedDeclarativeSoundInstance(QDeclarativeSoundInstance* declSndInstance);
};

QT_END_NAMESPACE

#endif
