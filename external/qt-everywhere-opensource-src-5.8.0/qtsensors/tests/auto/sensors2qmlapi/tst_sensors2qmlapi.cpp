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

#include <QtTest/QtTest>
#include <QtTest/QSignalSpy>
#include <QtCore/QDebug>
#include "../../../src/imports/sensors/qmlsensorgesture.h"
#include "qtemplategestureplugin.h"
#include "qtemplaterecognizer.h"
#include <qsensorgesturemanager.h>
#include "qsensormanager.h"

QT_USE_NAMESPACE

QT_BEGIN_NAMESPACE

class tst_Sensors2QMLAPI : public QObject
{
    Q_OBJECT

private slots:
    void initTestCase();
    void testGesture();
};

void tst_Sensors2QMLAPI::initTestCase()
{
    qputenv("QT_SENSORS_LOAD_PLUGINS", "0"); // Do not load plugins
}

void tst_Sensors2QMLAPI::testGesture()
{
    QTemplateGesturePlugin* plugin = new QTemplateGesturePlugin();
    QList <QSensorGestureRecognizer *> recognizers = plugin->createRecognizers();
    QSensorGestureManager manager;

    QmlSensorGesture* gs = new QmlSensorGesture(this);
    gs->componentComplete();
    QSignalSpy spy_availableGesturesChanged(gs, SIGNAL(availableGesturesChanged()));
    QSignalSpy spy_detected(gs, SIGNAL(detected(QString)));
    QSignalSpy spy_gesturesChanged(gs, SIGNAL(gesturesChanged()));
    QSignalSpy spy_validGesturesChanged(gs, SIGNAL(validGesturesChanged()));
    QSignalSpy spy_invalidGesturesChanged(gs, SIGNAL(invalidGesturesChanged()));
    QSignalSpy spy_enabledChanged(gs, SIGNAL(enabledChanged()));

    //This flag is needed if you run this unit test with an alread installed template plugin
    bool registered = false;
    for (int i = 0; i < recognizers.count(); i++){
        registered = manager.registerSensorGestureRecognizer(recognizers[i]);
    }
    if (registered) {
        QCOMPARE(spy_availableGesturesChanged.count(), 2);
    }

    //check creation of a not known plugin
    QCOMPARE(spy_invalidGesturesChanged.count(), 0);
    QCOMPARE(spy_gesturesChanged.count(), 0);
    gs->setGestures(QStringList() << "lollipop");
    QCOMPARE(spy_gesturesChanged.count(), 1);
    QCOMPARE(spy_invalidGesturesChanged.count(), 1);

    //check creation of a known plugin
    QCOMPARE(spy_validGesturesChanged.count(), 0);
    QCOMPARE(spy_gesturesChanged.count(), 1);
    spy_invalidGesturesChanged.clear();
    spy_validGesturesChanged.clear();
    gs->setGestures(QStringList() << "QtSensors.template");
    QCOMPARE(spy_gesturesChanged.count(), 2);
    QCOMPARE(spy_invalidGesturesChanged.count(), 1);
    QCOMPARE(spy_validGesturesChanged.count(), 1);

    //enable "QtSensors.template"
    QCOMPARE(spy_enabledChanged.count(), 0);
    QCOMPARE(spy_detected.count(), 0);
    gs->setEnabled(true);
    QCOMPARE(spy_enabledChanged.count(), 1);
    QCOMPARE(spy_detected.count(), 1);

    //set gesture during running sensor should not emit gesture changed
    spy_gesturesChanged.clear();
    gs->setGestures(QStringList() << "QtSensors.template2");
    QCOMPARE(spy_gesturesChanged.count(), 0);

    gs->setEnabled(false);

    QmlSensorGesture* gs1 = new QmlSensorGesture(this);
    QSignalSpy spy1_detected(gs1, SIGNAL(detected(QString)));
    QSignalSpy spy1_gesturesChanged(gs1, SIGNAL(gesturesChanged()));
    QSignalSpy spy1_validGesturesChanged(gs1, SIGNAL(validGesturesChanged()));
    QSignalSpy spy1_invalidGesturesChanged(gs1, SIGNAL(invalidGesturesChanged()));
    QSignalSpy spy1_enabledChanged(gs1, SIGNAL(enabledChanged()));
    gs1->componentComplete();

    //set enable = true without gesture should
    gs1->setEnabled(true);
    QCOMPARE(spy1_enabledChanged.count(), 1);
    gs1->setEnabled(false);
    spy1_enabledChanged.clear();

    //reding gestures check if we get back an empty string list
    QStringList gestures = gs1->gestures();
    QCOMPARE(gestures.count(), 0);
    QStringList validgestures = gs1->validGestures();
    QCOMPARE(validgestures.count(), 0);
    QStringList invalidgestures = gs1->invalidGestures();
    QCOMPARE(invalidgestures.count(), 0);

    //check types "QtSensors.template" "QtSensors.template1" "lollipop"
    //expect valid 2 not available 1
    gestures << "QtSensors.template" << "QtSensors.template1" << "lollipop";
    gs1->setGestures(gestures);
    gestures = gs1->gestures();
    QCOMPARE(gestures.count(), 3);
    QCOMPARE(spy1_validGesturesChanged.count(), 1);
    QCOMPARE(spy1_invalidGesturesChanged.count(), 1);
    QCOMPARE(spy1_gesturesChanged.count(), 1);
    //set same gesture again should not emit gesture changed
    gs1->setGestures(gestures);
    QCOMPARE(spy1_gesturesChanged.count(), 1);

    spy1_gesturesChanged.clear();
    gestures.clear();
    gs1->setGestures(gestures);
    QCOMPARE(spy1_gesturesChanged.count(), 1);

    //enable "QtSensors.template" and "QtSensors.template1"
    gestures << "QtSensors.template" << "QtSensors.template1";
    gs1->setEnabled(false);
    gs1->setGestures(gestures);
    spy1_enabledChanged.clear();
    spy1_detected.clear();
    gs1->setEnabled(true);
    QCOMPARE(spy1_enabledChanged.count(), 1);
    QCOMPARE(spy1_detected.count(), 2);
    gs1->setEnabled(false);

    //check sensor shouldn't run until the componentComplete gets called
    QmlSensorGesture* gs2 = new QmlSensorGesture(this);
    QSignalSpy spy2_detected(gs2, SIGNAL(detected(QString)));
    gs2->setGestures(QStringList() << "QtSensors.template");
    gs2->setEnabled(true);
    QCOMPARE(spy2_detected.count(), 0);
    gs2->componentComplete();
    QCOMPARE(spy2_detected.count(), 1);
}

QT_END_NAMESPACE

QTEST_MAIN(tst_Sensors2QMLAPI)
#include "tst_sensors2qmlapi.moc"
