/****************************************************************************
**
** Copyright (C) 2016 The Qt Company Ltd.
** Contact: https://www.qt.io/licensing/
**
** This file is part of the QtSensors module of the Qt Toolkit.
**
** $QT_BEGIN_LICENSE:GPL-EXCEPT$
** Commercial License Usage
** Licensees holding valid commercial Qt licenses may use this file in
** accordance with the commercial license agreement provided with the
** Software or, alternatively, in accordance with the terms contained in
** a written agreement between you and The Qt Company. For licensing terms
** and conditions see https://www.qt.io/terms-conditions. For further
** information use the contact form at https://www.qt.io/contact-us.
**
** GNU General Public License Usage
** Alternatively, this file may be used under the terms of the GNU
** General Public License version 3 as published by the Free Software
** Foundation with exceptions as appearing in the file LICENSE.GPL3-EXCEPT
** included in the packaging of this file. Please review the following
** information to ensure the GNU General Public License requirements will
** be met: https://www.gnu.org/licenses/gpl-3.0.html.
**
** $QT_END_LICENSE$
**
****************************************************************************/

#include <QObject>
#include <QtPlugin>
#include <QStringList>
#include <QTimer>
#include <QTest>

#include "qtestrecognizer.h"
#include "qtest2recognizer.h"

#include "qtestsensorgestureplugin_p.h"

#include <qsensorgestureplugininterface.h>
#include <qsensorgesturerecognizer.h>
#include <qsensorgesturemanager.h>


QTestSensorGesturePlugin::QTestSensorGesturePlugin()
{
}

QTestSensorGesturePlugin::~QTestSensorGesturePlugin()
{
}


///*!
//  Describes this gesture's possible gesture signals.
//handled through the detected(const QString &) signal.
//  */
//QStringList QTestSensorGesturePlugin::gestureSignals() const
//{
//    QStringList list;
//    Q_FOREACH (const QSensorGestureRecognizer* rec, recognizersList) {
//        list.append(rec->gestureSignals());
//    }
//    return list;
//}

QList <QSensorGestureRecognizer *> QTestSensorGesturePlugin::createRecognizers()
{
    QSensorGestureRecognizer *sRec = new QTestRecognizer(this);
    recognizersList.append(sRec);

    QSensorGestureRecognizer *sRec2 = new QTest2Recognizer(this);
    recognizersList.append(sRec2);

    return recognizersList;
}

QStringList QTestSensorGesturePlugin::supportedIds() const
{
    QStringList list;
    list << "QtSensors.test";
    list <<"QtSensors.test2";

    return list;
}

QList<QSensorGestureRecognizer*> QTestSensorGesturePlugin::recognizers() const
{
    return recognizersList;
}

QString QTestSensorGesturePlugin::name() const
{
    return "TestGestures";
}
