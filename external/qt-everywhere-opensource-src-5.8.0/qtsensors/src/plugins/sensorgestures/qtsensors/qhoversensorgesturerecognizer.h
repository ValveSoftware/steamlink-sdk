/****************************************************************************
**
** Copyright (C) 2016 The Qt Company Ltd.
** Contact: https://www.qt.io/licensing/
**
** This file is part of the QtSensors module of the Qt Toolkit.
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
