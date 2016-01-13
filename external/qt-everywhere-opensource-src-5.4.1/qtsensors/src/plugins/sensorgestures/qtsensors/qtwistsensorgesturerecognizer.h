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


#ifndef QWFLICKSENSORGESTURERECOGNIZER_H
#define QWFLICKSENSORGESTURERECOGNIZER_H

#include <QtSensors/qsensorgesturerecognizer.h>
#include <QtSensors/QAccelerometer>
#include <QtSensors/QOrientationSensor>
#include "qtsensorgesturesensorhandler.h"

QT_BEGIN_NAMESPACE

struct twistAccelData {
    qreal x;
    qreal y;
    qreal z;
};

class QTwistSensorGestureRecognizer : public QSensorGestureRecognizer
{
    Q_OBJECT
public:
    explicit QTwistSensorGestureRecognizer(QObject *parent = 0);
    ~QTwistSensorGestureRecognizer();

    void create() Q_DECL_OVERRIDE;

    QString id() const Q_DECL_OVERRIDE;
    bool start() Q_DECL_OVERRIDE;
    bool stop() Q_DECL_OVERRIDE;
    bool isActive() Q_DECL_OVERRIDE;

Q_SIGNALS:
    void twistLeft();
    void twistRight();

private slots:
    void accelChanged(QAccelerometerReading *reading);
    void orientationReadingChanged(QOrientationReading *reading);
    void checkTwist();

private:

    QOrientationReading *orientationReading;
    bool active;
    bool detecting;
    QList <twistAccelData> dataList;
    bool checking;
    void reset();
    bool checkOrientation();
    int increaseCount;
    int decreaseCount;
    qreal lastAngle;
    QList <QOrientationReading::Orientation> orientationList;
    qreal detectedAngle;
};
QT_END_NAMESPACE
#endif // QWFLICKSENSORGESTURERECOGNIZER_H
