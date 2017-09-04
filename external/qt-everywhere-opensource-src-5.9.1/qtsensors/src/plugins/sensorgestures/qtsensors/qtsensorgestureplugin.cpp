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

#include <QtPlugin>
#include <QStringList>
#include <QObject>

#include "qtsensorgestureplugin.h"

#include <QtSensors/qsensorgestureplugininterface.h>

#include "qcoversensorgesturerecognizer.h"
#include "qtwistsensorgesturerecognizer.h"
#include "qdoubletapsensorgesturerecognizer.h"
#include "qhoversensorgesturerecognizer.h"
#include "qpickupsensorgesturerecognizer.h"
#include "qshake2recognizer.h"
#include "qslamgesturerecognizer.h"
#include "qturnoversensorgesturerecognizer.h"
#include "qwhipsensorgesturerecognizer.h"
#include "qfreefallsensorgesturerecognizer.h"


QT_BEGIN_NAMESPACE


QtSensorGesturePlugin::QtSensorGesturePlugin()
{
}

QtSensorGesturePlugin::~QtSensorGesturePlugin()
{
}

QStringList QtSensorGesturePlugin::supportedIds() const
{
    QStringList list;
    list << "QtSensors.cover";
    list << "QtSensors.doubletap";
    list << "QtSensors.hover";
    list << "QtSensors.freefall";
    list << "QtSensors.pickup";
    list << "QtSensors.shake2";
    list << "QtSensors.slam";
    list << "QtSensors.turnover";
    list << "QtSensors.twist";
    list << "QtSensors.whip";
    return list;
}

QList <QSensorGestureRecognizer *> QtSensorGesturePlugin::createRecognizers()
{
    QList <QSensorGestureRecognizer *> recognizers;

    recognizers.append(new QCoverSensorGestureRecognizer(this));

    recognizers.append(new QDoubleTapSensorGestureRecognizer(this));

    recognizers.append(new QHoverSensorGestureRecognizer(this));

    recognizers.append(new QPickupSensorGestureRecognizer(this));

    recognizers.append(new QShake2SensorGestureRecognizer(this));

    recognizers.append(new QSlamSensorGestureRecognizer(this));

    recognizers.append(new QTurnoverSensorGestureRecognizer(this));

    recognizers.append(new QWhipSensorGestureRecognizer(this));

    recognizers.append(new QTwistSensorGestureRecognizer(this));

    recognizers.append(new QFreefallSensorGestureRecognizer(this));
    return recognizers;
}

QT_END_NAMESPACE
