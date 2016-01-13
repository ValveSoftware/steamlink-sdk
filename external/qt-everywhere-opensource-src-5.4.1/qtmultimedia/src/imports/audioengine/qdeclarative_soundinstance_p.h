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

#ifndef QDECLARATIVE_SOUNDINSTANCE_P_H
#define QDECLARATIVE_SOUNDINSTANCE_P_H

#include <QtCore/QObject>
#include <QtGui/qvector3d.h>
#include "qsoundinstance_p.h"

QT_BEGIN_NAMESPACE

class QDeclarativeSound;
class QAudioEngine;
class QDeclarativeAudioEngine;

class QDeclarativeSoundInstance : public QObject
{
    Q_OBJECT
    Q_PROPERTY(QDeclarativeAudioEngine* engine READ engine WRITE setEngine)
    Q_PROPERTY(QString sound READ sound WRITE setSound NOTIFY soundChanged)
    Q_PROPERTY(State state READ state NOTIFY stateChanged)
    Q_PROPERTY(QVector3D position READ position WRITE setPosition NOTIFY positionChanged)
    Q_PROPERTY(QVector3D direction READ direction WRITE setDirection NOTIFY directionChanged)
    Q_PROPERTY(QVector3D velocity READ velocity WRITE setVelocity NOTIFY velocityChanged)
    Q_PROPERTY(qreal gain READ gain WRITE setGain NOTIFY gainChanged)
    Q_PROPERTY(qreal pitch READ pitch WRITE setPitch NOTIFY pitchChanged)

    Q_ENUMS(State)

public:
    enum State
    {
        StoppedState = QSoundInstance::StoppedState,
        PlayingState = QSoundInstance::PlayingState,
        PausedState = QSoundInstance::PausedState
    };

    QDeclarativeSoundInstance(QObject *parent = 0);
    ~QDeclarativeSoundInstance();

    QDeclarativeAudioEngine* engine() const;
    void setEngine(QDeclarativeAudioEngine *engine);

    QString sound() const;
    void setSound(const QString& sound);

    State state() const;

    QVector3D position() const;
    void setPosition(const QVector3D& position);

    QVector3D direction() const;
    void setDirection(const QVector3D& direction);

    QVector3D velocity() const;
    void setVelocity(const QVector3D& velocity);

    qreal gain() const;
    void setGain(qreal gain);

    qreal pitch() const;
    void setPitch(qreal pitch);

    void setConeInnerAngle(qreal innerAngle);
    void setConeOuterAngle(qreal outerAngle);
    void setConeOuterGain(qreal outerGain);

Q_SIGNALS:
    void stateChanged();
    void positionChanged();
    void directionChanged();
    void velocityChanged();
    void gainChanged();
    void pitchChanged();
    void soundChanged();

public Q_SLOTS:
    void play();
    void stop();
    void pause();
    void updatePosition(qreal deltaTime);

private Q_SLOTS:
    void handleStateChanged();

private:
    Q_DISABLE_COPY(QDeclarativeSoundInstance);
    QString m_sound;
    QVector3D m_position;
    QVector3D m_direction;
    QVector3D m_velocity;
    qreal m_gain;
    qreal m_pitch;
    State m_requestState;

    qreal m_coneInnerAngle;
    qreal m_coneOuterAngle;
    qreal m_coneOuterGain;

    void dropInstance();

    QSoundInstance *m_instance;
    QDeclarativeAudioEngine *m_engine;


private Q_SLOTS:
    void engineComplete();
};

QT_END_NAMESPACE

#endif
