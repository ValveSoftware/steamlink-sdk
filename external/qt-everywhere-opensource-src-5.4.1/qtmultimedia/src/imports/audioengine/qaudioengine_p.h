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

#ifndef QAUDIOENGINE_P_H
#define QAUDIOENGINE_P_H

#include <QtCore/QObject>
#include <QtCore/QMap>
#include <QtCore/QUrl>
#include <QtGui/qvector3d.h>

QT_BEGIN_NAMESPACE

class QSoundSource;
class QSoundBuffer;
class QAudioEnginePrivate;

class QAudioEngine : public QObject
{
    Q_OBJECT
public:
    ~QAudioEngine();

    virtual QSoundSource* createSoundSource();
    virtual void releaseSoundSource(QSoundSource *soundInstance);

    virtual QSoundBuffer* getStaticSoundBuffer(const QUrl& url);
    virtual void releaseSoundBuffer(QSoundBuffer *buffer);

    virtual bool isLoading() const;

    virtual QVector3D listenerPosition() const;
    virtual QVector3D listenerDirection() const;
    virtual QVector3D listenerVelocity() const;
    virtual QVector3D listenerUp() const;
    virtual qreal listenerGain() const;
    virtual void setListenerPosition(const QVector3D& position);
    virtual void setListenerDirection(const QVector3D& direction);
    virtual void setListenerVelocity(const QVector3D& velocity);
    virtual void setListenerUp(const QVector3D& up);
    virtual void setListenerGain(qreal gain);

    virtual qreal dopplerFactor() const;
    virtual void setDopplerFactor(qreal dopplerFactor);

    virtual qreal speedOfSound() const;
    virtual void setSpeedOfSound(qreal speedOfSound);

    static QAudioEngine* create(QObject *parent);

Q_SIGNALS:
    void isLoadingChanged();

private:
    QAudioEngine(QObject *parent);
    QAudioEnginePrivate *d;

    void updateListenerOrientation();

    qreal m_dopplerFactor;
    qreal m_speedOfSound;
    QVector3D m_listenerUp;
    QVector3D m_listenerDirection;
};

QT_END_NAMESPACE

#endif
