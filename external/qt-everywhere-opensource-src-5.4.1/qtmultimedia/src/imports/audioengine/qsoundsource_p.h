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

#ifndef QSOUNDSOURCE_P_H
#define QSOUNDSOURCE_P_H

#include <QVector3D>
#include <QObject>

QT_BEGIN_NAMESPACE

class QSoundBuffer;

class QSoundSource : public QObject
{
    Q_OBJECT
public:
    enum State
    {
        StoppedState,
        PlayingState,
        PausedState
    };

    virtual void play() = 0;
    virtual void pause() = 0;
    virtual void stop() = 0;

    virtual QSoundSource::State state() const = 0;

    virtual void setLooping(bool looping) = 0;
    virtual void setDirection(const QVector3D& direction) = 0;
    virtual void setPosition(const QVector3D& position) = 0;
    virtual void setVelocity(const QVector3D& velocity) = 0;

    virtual QVector3D velocity() const = 0;
    virtual QVector3D position() const = 0;
    virtual QVector3D direction() const = 0;

    virtual void setGain(qreal gain) = 0;
    virtual void setPitch(qreal pitch) = 0;
    virtual void setCone(qreal innerAngle, qreal outerAngle, qreal outerGain) = 0;

    virtual void bindBuffer(QSoundBuffer*) = 0;
    virtual void unbindBuffer() = 0;

Q_SIGNALS:
    void stateChanged(QSoundSource::State newState);

protected:
    QSoundSource(QObject *parent);
};

QT_END_NAMESPACE

#endif
