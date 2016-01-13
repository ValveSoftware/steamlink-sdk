/****************************************************************************
**
** Copyright (C) 2014 Digia Plc and/or its subsidiary(-ies).
** Contact: http://www.qt-project.org/legal
**
** This file is part of the QtSensors module of the Qt Toolkit.
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


#ifndef QHOVERSENSORGESTURERECOGNIZER_H
#define QHOVERSENSORGESTURERECOGNIZER_H

#include <QtSensors/QSensorGestureRecognizer>

#include "qtsensorgesturesensorhandler.h"


QT_BEGIN_NAMESPACE

class QHoverSensorGestureRecognizer : public QSensorGestureRecognizer
{
    Q_OBJECT
public:
    explicit QHoverSensorGestureRecognizer(QObject *parent = 0);
    ~QHoverSensorGestureRecognizer();

    void create() Q_DECL_OVERRIDE;

    QString id() const Q_DECL_OVERRIDE;
    bool start() Q_DECL_OVERRIDE;
    bool stop() Q_DECL_OVERRIDE;
    bool isActive() Q_DECL_OVERRIDE;

Q_SIGNALS:
    void hover();

private slots:
    void orientationReadingChanged(QOrientationReading *reading);
    void irProximityReadingChanged(QIRProximityReading *reading);
    void timeout();
    void timeout2();
private:
    QOrientationReading *orientationReading;
    qreal reflectance;
    bool hoverOk;
    bool detecting;

    qreal detectedHigh;
    bool active;
    qreal initialReflectance;
    bool checkForHovering();
    bool useHack;

    quint64 lastTimestamp;

    bool timer2Active;
    quint64 lapsedTime2;

};
QT_END_NAMESPACE
#endif // QHOVERSENSORGESTURERECOGNIZER_H
