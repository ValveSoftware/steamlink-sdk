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

#include <QtCore/QString>
#include <QtTest/QtTest>
#include <QDebug>
#include <QVariant>
#include <QSignalSpy>

#include <qsensorgesture.h>
#include <qsensorgesturemanager.h>

#include <qsensorgesturerecognizer.h>
#include <qsensorgestureplugininterface.h>

Q_IMPORT_PLUGIN(QTestSensorGesturePlugin)
Q_IMPORT_PLUGIN(QTestSensorGestureDupPlugin)

static QString removeParens(const QString &arg)
{
    return arg.left(arg.indexOf("("));
}

class QSensorGestureWithSlots : public QObject
{
    Q_OBJECT
public:
    QSensorGestureWithSlots(const QStringList &ids)
        : gesture(new QSensorGesture(ids, this))
    {
    }

    QSensorGesture *gesture;

public slots:
    void startDetection()
    {
        gesture->startDetection();
    }

    void stopDetection()
    {
        gesture->stopDetection();
    }
};

class QTest3Recognizer : public QSensorGestureRecognizer
{
    Q_OBJECT

public:

    QTest3Recognizer(QObject *parent = 0);

    void create();

    QString id() const;
    bool start() ;
    bool stop();
    bool isActive();
    void changeId(const QString &);

    QString recognizerId;
};

QTest3Recognizer::QTest3Recognizer(QObject *parent) : QSensorGestureRecognizer(parent),
 recognizerId("QtSensors/test3"){}

void QTest3Recognizer::create(){}

QString QTest3Recognizer::id() const{ return recognizerId; }
bool QTest3Recognizer::start(){return true;}
bool QTest3Recognizer::stop() { return true;}
bool QTest3Recognizer::isActive() { return true; }
void QTest3Recognizer::changeId(const QString &id)
{
    recognizerId = id;
}


class Tst_qsensorgestureTest : public QObject
{
    Q_OBJECT

public:
    Tst_qsensorgestureTest();

private Q_SLOTS:
    void tst_sensor_gesture_notinitialized();

    void tst_recognizer_dup(); //comes first to weed out messages

    void tst_manager();
    void tst_manager_gestureids();
    void tst_manager_recognizerSignals();
    void tst_manager_registerSensorGestureRecognizer();
    void tst_manager__newSensorAvailable();

    void tst_sensor_gesture_signals();
    void tst_sensor_gesture_threaded();

    void tst_sensor_gesture();

    void tst_recognizer();

    void tst_sensorgesture_noid();

    void tst_sensor_gesture_multi();

    void shakeDetected(const QString &);


private:
    QString currentSignal;
};

Tst_qsensorgestureTest::Tst_qsensorgestureTest()
{
}

void Tst_qsensorgestureTest::tst_recognizer_dup()
{
    QStringList idList;
    {
//        QTest::ignoreMessage(QtWarningMsg, "\"QtSensors.test.dup\" from the plugin \"TestGesturesDup\" is already known.");
        QSensorGestureManager manager;
        idList = manager.gestureIds();

        for (int i = 0; i < idList.count(); i++) {
              if (idList.at(i) == "QtSensors.test.dup")
                  QTest::ignoreMessage(QtWarningMsg, "Ignoring recognizer  \"QtSensors.test.dup\" from plugin \"TestGesturesDup\" because it is already registered");
            QStringList recognizerSignalsList = manager.recognizerSignals(idList.at(i));

            QVERIFY(!recognizerSignalsList.contains("QtSensors.test2"));
        }

        QScopedPointer<QSensorGesture> sensorGesture(new QSensorGesture(idList));
        QVERIFY(sensorGesture->validIds().contains("QtSensors.test2"));
        QVERIFY(sensorGesture->validIds().contains("QtSensors.test"));
        QVERIFY(sensorGesture->validIds().contains("QtSensors.test.dup"));
    }

    QScopedPointer<QSensorGesture> thisGesture;
    QString plugin;
    plugin = "QtSensors.test2";
    thisGesture.reset(new QSensorGesture(QStringList() << plugin));
    QVERIFY(thisGesture->validIds().contains("QtSensors.test2"));

    plugin = "QtSensors.test.dup";
    thisGesture.reset(new QSensorGesture(QStringList() << plugin));
    QVERIFY(!thisGesture->validIds().contains("QtSensors.test2"));
}

void Tst_qsensorgestureTest::tst_manager()
{
    QSensorGestureManager *manager2;
    manager2 = new QSensorGestureManager(this);
    QVERIFY(manager2 != 0);
    delete manager2;
}

void Tst_qsensorgestureTest::tst_manager_gestureids()
{
    QStringList idList;
    QSensorGestureManager manager;
    idList = manager.gestureIds();

    QVERIFY(idList.count() > 0);

    QVERIFY(idList.contains("QtSensors.test"));
    QVERIFY(idList.contains("QtSensors.test2"));
    QVERIFY(idList.contains("QtSensors.test.dup"));
}

void Tst_qsensorgestureTest::tst_manager_recognizerSignals()
{
    QStringList idList;

    QSensorGestureManager manager;
    idList = manager.gestureIds();

    idList.removeOne("QtSensors.test.dup");

    for (int i = 0; i < idList.count(); i++) {

        QStringList recognizerSignalsList = manager.recognizerSignals(idList.at(i));

        if (idList.at(i) == "QtSensors.test") {
            QStringList signalList;
            signalList << "detected(QString)";
            signalList << "tested()";
            QCOMPARE(recognizerSignalsList.count(), 2);

            QCOMPARE(recognizerSignalsList, signalList);

        } else if (idList.at(i) == "QtSensors.test2") {
            QStringList signalList;
            signalList << "detected(QString)";
            signalList << "test2()";
            signalList << "test3(bool)";

            QCOMPARE(recognizerSignalsList.count(), 3);
            QCOMPARE(recognizerSignalsList, signalList);
        }
    }
}

void Tst_qsensorgestureTest::tst_manager_registerSensorGestureRecognizer()
{
    QSensorGestureManager manager;
    int num = manager.gestureIds().count();
    QSensorGestureRecognizer *recognizer = new QTest3Recognizer;
    bool ok = manager.registerSensorGestureRecognizer(recognizer);
    QCOMPARE(ok, true);
    QCOMPARE(num+1, manager.gestureIds().count());

    recognizer = new QTest3Recognizer;
//    QTest::ignoreMessage(QtWarningMsg, "\"QtSensors/test3\" is already known");
    ok = manager.registerSensorGestureRecognizer(recognizer);
    QCOMPARE(ok, false);
    QCOMPARE(num+1, manager.gestureIds().count());
}

void Tst_qsensorgestureTest::tst_manager__newSensorAvailable()
{
    QSensorGestureManager manager;
    QSensorGestureManager manager2;

    QSignalSpy spy_manager_available(&manager, SIGNAL(newSensorGestureAvailable()));
    QSignalSpy spy_manager2_available(&manager2, SIGNAL(newSensorGestureAvailable()));

    manager.gestureIds();
    QCOMPARE(spy_manager_available.count(),0);
    QCOMPARE(spy_manager2_available.count(),0);

    QTest3Recognizer *recognizer = new QTest3Recognizer;
    recognizer->changeId("QtSensors.test4");

    bool ok = manager.registerSensorGestureRecognizer(recognizer);
    QCOMPARE(ok, true);
    QCOMPARE(spy_manager_available.count(),1);

    recognizer = new QTest3Recognizer;
    recognizer->changeId("QtSensors.test4");
//    QTest::ignoreMessage(QtWarningMsg, "\"QtSensors.test4\" is already known");
    ok = manager.registerSensorGestureRecognizer(recognizer);
    QCOMPARE(ok, false);
    QCOMPARE(spy_manager_available.count(),1);
    QCOMPARE(spy_manager2_available.count(),1);

    QScopedPointer<QSensorGesture> test4sg;
    test4sg.reset(new QSensorGesture(QStringList() << "QtSensors.test4"));
    QVERIFY(!test4sg->validIds().isEmpty());
    QVERIFY(test4sg->invalidIds().isEmpty());
}


void Tst_qsensorgestureTest::tst_sensor_gesture_signals()
{
    QStringList  testidList;
    testidList << "QtSensors.test";
    testidList << "QtSensors.test2";

    Q_FOREACH (const QString &plugin, testidList) {

        QScopedPointer<QSensorGesture> thisGesture(new QSensorGesture(QStringList() << plugin));

        QSignalSpy spy_gesture_detected(thisGesture.data(), SIGNAL(detected(QString)));
        QScopedPointer<QSignalSpy> spy_gesture_tested(0);

        if (plugin == "QtSensors.test") {
            QStringList signalList;
            signalList << "detected(QString)";
            signalList << "tested()";

            QCOMPARE(thisGesture->gestureSignals().count(), 2);
            QCOMPARE(thisGesture->gestureSignals(), signalList);

            QCOMPARE(thisGesture->gestureSignals().at(1), QString("tested()"));

            spy_gesture_tested.reset(new QSignalSpy(thisGesture.data(), SIGNAL(tested())));
        } else if (plugin == "QtSensors.test2") {
            QStringList signalList;
            signalList << "detected(QString)";
            signalList << "test2()";
            signalList << "test3(bool)";
            QCOMPARE(thisGesture->gestureSignals().count(), 3);
            QCOMPARE(thisGesture->gestureSignals(), signalList);

            QCOMPARE(thisGesture->gestureSignals().at(1), QString("test2()"));
            spy_gesture_tested.reset(new QSignalSpy(thisGesture.data(), SIGNAL(test2())));
        }

        QVERIFY(!thisGesture->validIds().isEmpty());
        thisGesture->startDetection();

        QCOMPARE(spy_gesture_detected.count(),1);

        if (plugin == "QtSensors.test") {
            QCOMPARE(spy_gesture_tested->count(),1);
            QList<QVariant> arguments ;
           arguments = spy_gesture_detected.takeFirst(); // take the first signal
           QCOMPARE(arguments.at(0).toString(), QString("tested"));
        } else if (plugin == "QtSensors.test2") {
            QCOMPARE(spy_gesture_tested->count(),1);
        }
    }

}


void Tst_qsensorgestureTest::tst_sensor_gesture_threaded()
{

    QScopedPointer<QSensorGesture> gesture(new QSensorGesture(QStringList() << "QtSensors.test"));

    QScopedPointer<QThread> thread(new QThread);
    QScopedPointer<QSensorGestureWithSlots> t_gesture(new QSensorGestureWithSlots(QStringList() << "QtSensors.test"));
    t_gesture->moveToThread(thread.data());

    currentSignal = removeParens(gesture->gestureSignals().at(0));

    QSignalSpy thread_gesture(t_gesture->gesture, SIGNAL(detected(QString)));
    QSignalSpy spy_gesture2(gesture.data(), SIGNAL(detected(QString)));

    QCOMPARE(gesture->isActive(),false);
    gesture->startDetection();

    QCOMPARE(thread_gesture.count(),0);
    QCOMPARE(spy_gesture2.count(),1);

    QCOMPARE(gesture->isActive(),true);

    thread->start();
    QTimer::singleShot(0, t_gesture.data(), SLOT(startDetection())); // Delivered on the thread

    QTRY_COMPARE(t_gesture->gesture->isActive(),true);

    QTRY_VERIFY(thread_gesture.count() > 0);
    spy_gesture2.clear();
    QTRY_VERIFY(spy_gesture2.count() > 0);

    QTimer::singleShot(0, t_gesture.data(), SLOT(stopDetection())); // Delivered on the thread

    QTRY_COMPARE(t_gesture->gesture->isActive(),false);
    QCOMPARE(gesture->isActive(),true);

    thread->quit();
    thread->wait();
}

void Tst_qsensorgestureTest::tst_sensor_gesture()
{
    QScopedPointer<QSensorGesture> gesture(new QSensorGesture(QStringList() << "QtSensors.test"));

    QScopedPointer<QSensorGesture> gesture2(new QSensorGesture(QStringList() << "QtSensors.test2"));
    QScopedPointer<QSensorGesture> gesture3(new QSensorGesture(QStringList() << "QtSensors.test2"));

    QCOMPARE(gesture->validIds(),QStringList() << "QtSensors.test");

    QCOMPARE(gesture->gestureSignals().at(1), QString("tested()"));

    QVERIFY(gesture->invalidIds().isEmpty());
    QVERIFY(gesture2->invalidIds().isEmpty());
    QVERIFY(gesture3->invalidIds().isEmpty());

    currentSignal = removeParens(gesture->gestureSignals().at(1));

    QSignalSpy spy_gesture(gesture.data(), SIGNAL(detected(QString)));

    QSignalSpy spy_gesture2(gesture2.data(), SIGNAL(detected(QString)));

    QSignalSpy spy_gesture3_detected(gesture3.data(), SIGNAL(detected(QString)));

    QSignalSpy spy_gesture4_test2(gesture3.data(), SIGNAL(test2()));
    QSignalSpy spy_gesture5_test3(gesture3.data(), SIGNAL(test3(bool)));


    QCOMPARE(gesture->isActive(),false);
    gesture->startDetection();

    QCOMPARE(spy_gesture.count(),1);

    QCOMPARE(gesture->isActive(),true);
    QCOMPARE(gesture2->validIds(),QStringList() <<"QtSensors.test2");
    QCOMPARE(gesture2->gestureSignals().at(1), QString("test2()"));
    currentSignal = removeParens(gesture2->gestureSignals().at(1));

    connect(gesture2.data(),SIGNAL(detected(QString)),
            this,SLOT(shakeDetected(QString)));

    QCOMPARE(gesture2->isActive(),false);

    gesture2->startDetection();

    QCOMPARE(gesture2->isActive(),true);

    QCOMPARE(spy_gesture2.count(),1);

    QCOMPARE(spy_gesture3_detected.count(),0);

    gesture2->stopDetection();

    QCOMPARE(gesture2->isActive(),false);
    QCOMPARE(gesture3->isActive(),false);

    gesture3->startDetection();

    QCOMPARE(gesture3->isActive(),true);
    QCOMPARE(gesture2->isActive(),false);

    QCOMPARE(spy_gesture.count(),1);

    QCOMPARE(spy_gesture2.count(),1);


    QCOMPARE(spy_gesture3_detected.count(),1);

    QCOMPARE(spy_gesture4_test2.count(),1);

    QCOMPARE(spy_gesture5_test3.count(),1);

    QList<QVariant> arguments2 = spy_gesture5_test3.takeFirst();
    QCOMPARE(arguments2.at(0).toBool(), true);
}

void Tst_qsensorgestureTest::tst_recognizer()
{
    QScopedPointer<QSensorGesture> gesture(new QSensorGesture(QStringList() << "QtSensors.test"));
    QScopedPointer<QSensorGesture> gesture2(new QSensorGesture(QStringList() << "QtSensors.test"));

    QSignalSpy spy_gesture(gesture.data(), SIGNAL(detected(QString)));
    QSignalSpy spy_gesture2(gesture2.data(), SIGNAL(detected(QString)));

    QCOMPARE(gesture->isActive(),false);
    QCOMPARE(gesture2->isActive(),false);

    currentSignal = removeParens(gesture2->gestureSignals().at(0));

    gesture2->startDetection();//activate 2

    QCOMPARE(gesture->isActive(),false);
    QCOMPARE(gesture2->isActive(),true);

    QCOMPARE(spy_gesture.count(),0);

    QCOMPARE(spy_gesture2.count(),1);

    QList<QVariant> arguments = spy_gesture2.takeFirst();
    QCOMPARE(arguments.at(0).toString(), QString("tested"));

    QCOMPARE(spy_gesture2.count(),0);

    gesture->startDetection(); //activate 1

    QCOMPARE(gesture->isActive(),true);
    QCOMPARE(gesture2->isActive(),true);

    QTRY_COMPARE(spy_gesture.count(),1);

    QCOMPARE(spy_gesture2.count(),1);

    arguments = spy_gesture.takeFirst(); // take the first signal
    QCOMPARE(arguments.at(0).toString(), QString("tested"));
    spy_gesture2.removeFirst();

    gesture->stopDetection(); //stop 1 gesture object

    QCOMPARE(gesture->isActive(),false);
    QCOMPARE(gesture2->isActive(),true);

    spy_gesture2.clear();
    gesture2->startDetection();

    QCOMPARE(gesture->isActive(),false);
    QCOMPARE(spy_gesture.count(),0);

    QCOMPARE(gesture2->isActive(),true);

    QTRY_COMPARE(spy_gesture2.count(), 1);
}


void Tst_qsensorgestureTest::tst_sensorgesture_noid()
{
    QScopedPointer<QSensorGesture> gesture(new QSensorGesture(QStringList() << "QtSensors.noid"));
    QVERIFY(gesture->validIds().isEmpty());
    QCOMPARE(gesture->invalidIds(), QStringList() << "QtSensors.noid");

    QTest::ignoreMessage(QtWarningMsg, "QSignalSpy: No such signal: 'detected(QString)'");
    QSignalSpy spy_gesture(gesture.data(), SIGNAL(detected(QString)));

    QCOMPARE(spy_gesture.count(),0);

    gesture->startDetection();
    QCOMPARE(gesture->isActive(),false);
    QCOMPARE(spy_gesture.count(),0);

    gesture->stopDetection();
    QCOMPARE(gesture->isActive(),false);
    QCOMPARE(spy_gesture.count(),0);

    QVERIFY(gesture->gestureSignals().isEmpty());

    QCOMPARE(gesture->invalidIds() ,QStringList() << "QtSensors.noid");

    QSensorGestureManager manager;
    QStringList recognizerSignalsList = manager.recognizerSignals( "QtSensors.noid");
    QVERIFY(recognizerSignalsList.isEmpty());

    QVERIFY(!recognizerSignalsList.contains("QtSensors.noid"));

    QSensorGestureRecognizer *fakeRecognizer = manager.sensorGestureRecognizer("QtSensors.noid");
    QVERIFY(!fakeRecognizer);
}

void Tst_qsensorgestureTest::tst_sensor_gesture_multi()
{

    QStringList ids;
    ids << "QtSensors.test";
    ids <<"QtSensors.test2";
    ids << "QtSensors.bogus";

    QScopedPointer<QSensorGesture> gesture(new QSensorGesture(ids));
    QStringList gestureSignals = gesture->gestureSignals();

    gestureSignals.removeDuplicates() ;
    QCOMPARE(gestureSignals, gesture->gestureSignals());

    QCOMPARE(gesture->gestureSignals().count(), 4);
    QCOMPARE(gesture->invalidIds(), QStringList() << "QtSensors.bogus");

    QCOMPARE(gesture->isActive(),false);

    QSignalSpy spy_gesture_detected(gesture.data(), SIGNAL(detected(QString)));
    gesture->startDetection();
    QCOMPARE(gesture->isActive(),true);
    QCOMPARE(spy_gesture_detected.count(),2);

    QList<QVariant> arguments ;
    arguments = spy_gesture_detected.takeAt(0);
    QCOMPARE(arguments.at(0).toString(), QString("tested"));

    arguments = spy_gesture_detected.takeAt(0);
    QCOMPARE(arguments.at(0).toString(), QString("test2"));

    QTRY_COMPARE(spy_gesture_detected.count(),1);

    gesture->stopDetection();

    QCOMPARE(gesture->isActive(),false);

    {
        QSensorGestureManager manager;
        QVERIFY(!manager.gestureIds().contains("QtSensors.bogus"));
        QSensorGestureRecognizer *recognizer = manager.sensorGestureRecognizer("QtSensors.bogus");
        QVERIFY(recognizer == 0);
    }

}

void Tst_qsensorgestureTest::shakeDetected(const QString &type)
{
    QCOMPARE(type,currentSignal);
}

void Tst_qsensorgestureTest::tst_sensor_gesture_notinitialized()
{
    QTest::ignoreMessage(QtWarningMsg, "\"QtSensors.test.dup\" from the plugin \"TestGesturesDup\" is already known.");
    QSensorGestureManager manager;
    QSensorGestureRecognizer *recognizer = manager.sensorGestureRecognizer("QtSensors.test");

    QTest::ignoreMessage(QtWarningMsg, "Not starting. Gesture Recognizer not initialized");
    recognizer->startBackend();
    QVERIFY(recognizer->isActive() == false);

    QTest::ignoreMessage(QtWarningMsg, "Not stopping. Gesture Recognizer not initialized");
    recognizer->stopBackend();
    QVERIFY(recognizer->isActive() == false);

    recognizer->createBackend();
    QVERIFY(recognizer->isActive() == false);

}


QTEST_MAIN(Tst_qsensorgestureTest);

#include "tst_qsensorgesturetest.moc"
