/****************************************************************************
**
** Copyright (C) 2016 The Qt Company Ltd.
** Contact: https://www.qt.io/licensing/
**
** This file is part of the QtScxml module of the Qt Toolkit.
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

#include <QtTest>
#include <QObject>
#include <QXmlStreamReader>
#include <QtScxml/qscxmlcompiler.h>
#include <QtScxml/qscxmlstatemachine.h>
#include <QtScxml/private/qscxmlstatemachine_p.h>

Q_DECLARE_METATYPE(QScxmlError);

enum { SpyWaitTime = 8000 };

class tst_StateMachine: public QObject
{
    Q_OBJECT

private Q_SLOTS:
    void stateNames_data();
    void stateNames();
    void activeStateNames_data();
    void activeStateNames();
    void connections();
    void onExit();
    void eventOccurred();

    void doneDotStateEvent();
    void running();
};

void tst_StateMachine::stateNames_data()
{
    QTest::addColumn<QString>("scxmlFileName");
    QTest::addColumn<bool>("compressed");
    QTest::addColumn<QStringList>("expectedStates");

    QTest::newRow("stateNames-compressed") << QString(":/tst_statemachine/statenames.scxml")
                                      << true
                                      << (QStringList() << QString("a1") << QString("a2") << QString("final"));
    QTest::newRow("stateNames-notCompressed") << QString(":/tst_statemachine/statenames.scxml")
                                      << false
                                      << (QStringList() << QString("top") << QString("a") << QString("a1") <<  QString("a2") << QString("b") << QString("final"));
    QTest::newRow("stateNamesNested-compressed") << QString(":/tst_statemachine/statenamesnested.scxml")
                                      << true
                                      << (QStringList() << QString("a") << QString("b"));
    QTest::newRow("stateNamesNested-notCompressed") << QString(":/tst_statemachine/statenamesnested.scxml")
                                      << false
                                      << (QStringList() << QString("super_top") << QString("a") << QString("b"));

    QTest::newRow("ids1") << QString(":/tst_statemachine/ids1.scxml")
                          << false
                          << (QStringList() << QString("foo.bar") << QString("foo-bar")
                              << QString("foo_bar") << QString("_"));
}

void tst_StateMachine::stateNames()
{
    QFETCH(QString, scxmlFileName);
    QFETCH(bool, compressed);
    QFETCH(QStringList, expectedStates);

    QScopedPointer<QScxmlStateMachine> stateMachine(QScxmlStateMachine::fromFile(scxmlFileName));
    QVERIFY(!stateMachine.isNull());
    QCOMPARE(stateMachine->parseErrors().count(), 0);

    QCOMPARE(stateMachine->stateNames(compressed), expectedStates);
}

void tst_StateMachine::activeStateNames_data()
{
    QTest::addColumn<QString>("scxmlFileName");
    QTest::addColumn<bool>("compressed");
    QTest::addColumn<QStringList>("expectedStates");

    QTest::newRow("stateNames-compressed") << QString(":/tst_statemachine/statenames.scxml")
                                      << true
                                      << (QStringList() << QString("a1") << QString("final"));
    QTest::newRow("stateNames-notCompressed") << QString(":/tst_statemachine/statenames.scxml")
                                      << false
                                      << (QStringList() << QString("top") << QString("a") << QString("a1") << QString("b") << QString("final"));
    QTest::newRow("stateNamesNested-compressed") << QString(":/tst_statemachine/statenamesnested.scxml")
                                      << true
                                      << (QStringList() << QString("a") << QString("b"));
    QTest::newRow("stateNamesNested-notCompressed") << QString(":/tst_statemachine/statenamesnested.scxml")
                                      << false
                                      << (QStringList() << QString("super_top") << QString("a") << QString("b"));
}

void tst_StateMachine::activeStateNames()
{
    QFETCH(QString, scxmlFileName);
    QFETCH(bool, compressed);
    QFETCH(QStringList, expectedStates);

    QScopedPointer<QScxmlStateMachine> stateMachine(QScxmlStateMachine::fromFile(scxmlFileName));
    QVERIFY(!stateMachine.isNull());

    QSignalSpy stableStateSpy(stateMachine.data(), SIGNAL(reachedStableState()));

    stateMachine->start();

    stableStateSpy.wait(5000);

    QCOMPARE(stateMachine->activeStateNames(compressed), expectedStates);
}

class Receiver : public QObject {
    Q_OBJECT
public slots:
    void a(bool enabled)
    {
        aReached = aReached || enabled;
    }

    void b(bool enabled)
    {
        bReached = bReached || enabled;
    }

    void aEnter()
    {
        aEntered = true;
    }

    void aExit()
    {
        aExited = true;
    }

public:
    bool aReached = false;
    bool bReached = false;
    bool aEntered = false;
    bool aExited = false;
};

void tst_StateMachine::connections()
{
    QScopedPointer<QScxmlStateMachine> stateMachine(
                QScxmlStateMachine::fromFile(QString(":/tst_statemachine/statenames.scxml")));
    QVERIFY(!stateMachine.isNull());
    Receiver receiver;

    bool a1Reached = false;
    bool finalReached = false;
    QMetaObject::Connection a = stateMachine->connectToState("a", &receiver, &Receiver::a);
    QVERIFY(a);
    QMetaObject::Connection b = stateMachine->connectToState("b", &receiver, SLOT(b(bool)));
    QVERIFY(b);
    QMetaObject::Connection a1 = stateMachine->connectToState("a1", &receiver,
                                                              [&a1Reached](bool enabled) {
        a1Reached = a1Reached || enabled;
    });
    QVERIFY(a1);
    QMetaObject::Connection final = stateMachine->connectToState("final",
                                                                 [&finalReached](bool enabled) {
        finalReached = finalReached || enabled;
    });
    QVERIFY(final);

    bool a1Entered = false;
    bool a1Exited = false;
    bool finalEntered = false;
    bool finalExited = false;
    typedef QScxmlStateMachine QXSM;

    QMetaObject::Connection aEntry = stateMachine->connectToState(
                "a", QXSM::onEntry(&receiver, &Receiver::aEnter));
    QVERIFY(aEntry);
    QMetaObject::Connection aExit = stateMachine->connectToState(
                "a", QXSM::onExit(&receiver, &Receiver::aExit));
    QVERIFY(aExit);
    QMetaObject::Connection a1Entry = stateMachine->connectToState("a1", &receiver,
                                                                   QXSM::onEntry([&a1Entered]() {
        a1Entered = true;
    }));
    QVERIFY(a1Entry);
    QMetaObject::Connection a1Exit = stateMachine->connectToState("a1", &receiver,
                                                                   QXSM::onExit([&a1Exited]() {
        a1Exited = true;
    }));
    QVERIFY(a1Exit);

    QMetaObject::Connection finalEntry = stateMachine->connectToState(
                "final", QXSM::onEntry([&finalEntered]() {
        finalEntered = true;
    }));
    QVERIFY(finalEntry);

    QMetaObject::Connection finalExit = stateMachine->connectToState(
                "final", QXSM::onExit([&finalExited]() {
        finalExited = true;
    }));
    QVERIFY(finalExit);

    stateMachine->start();

    QTRY_VERIFY(a1Reached);
    QTRY_VERIFY(finalReached);
    QTRY_VERIFY(receiver.aReached);
    QTRY_VERIFY(receiver.bReached);

    QVERIFY(disconnect(a));
    QVERIFY(disconnect(b));
    QVERIFY(disconnect(a1));
    QVERIFY(disconnect(final));

#if defined(__cpp_return_type_deduction) && __cpp_return_type_deduction == 201304
    QVERIFY(receiver.aEntered);
    QVERIFY(!receiver.aExited);
    QVERIFY(a1Entered);
    QVERIFY(!a1Exited);
    QVERIFY(finalEntered);
    QVERIFY(!finalExited);

    QVERIFY(disconnect(aEntry));
    QVERIFY(disconnect(aExit));
    QVERIFY(disconnect(a1Entry));
    QVERIFY(disconnect(a1Exit));
    QVERIFY(disconnect(finalEntry));
    QVERIFY(disconnect(finalExit));
#endif
}

void tst_StateMachine::onExit()
{
#if defined(__cpp_return_type_deduction) && __cpp_return_type_deduction == 201304
    // Test onExit being actually called

    typedef QScxmlStateMachine QXSM;
    QScopedPointer<QXSM> stateMachine(QXSM::fromFile(QString(":/tst_statemachine/eventoccurred.scxml")));

    Receiver receiver;
    bool aExited1 = false;

    stateMachine->connectToState("a", QXSM::onExit([&aExited1]() { aExited1 = true; }));
    stateMachine->connectToState("a", QXSM::onExit(&receiver, &Receiver::aExit));
    stateMachine->connectToState("a", QXSM::onExit(&receiver, "aEnter"));
    {
        // Should not crash
        Receiver receiver2;
        stateMachine->connectToState("a", QXSM::onEntry(&receiver2, &Receiver::aEnter));
        stateMachine->connectToState("a", QXSM::onEntry(&receiver2, "aExit"));
        stateMachine->connectToState("a", QXSM::onExit(&receiver2, &Receiver::aExit));
        stateMachine->connectToState("a", QXSM::onExit(&receiver2, "aEnter"));
    }

    stateMachine->start();
    QTRY_VERIFY(receiver.aEntered);
    QTRY_VERIFY(receiver.aExited);
    QTRY_VERIFY(aExited1);
#endif
}

bool hasChildEventRouters(QScxmlStateMachine *stateMachine)
{
    // Cast to QObject, to avoid ambigous "children" member.
    const QObject &parentRouter = QScxmlStateMachinePrivate::get(stateMachine)->m_router;
    return !parentRouter.children().isEmpty();
}

void tst_StateMachine::eventOccurred()
{
    QScopedPointer<QScxmlStateMachine> stateMachine(QScxmlStateMachine::fromFile(QString(":/tst_statemachine/eventoccurred.scxml")));
    QVERIFY(!stateMachine.isNull());

    qRegisterMetaType<QScxmlEvent>();
    QSignalSpy finishedSpy(stateMachine.data(), SIGNAL(finished()));

    int events = 0;
    auto con1 = stateMachine->connectToEvent("internalEvent2", [&events](const QScxmlEvent &event) {
        QCOMPARE(++events, 1);
        QCOMPARE(event.name(), QString("internalEvent2"));
        QCOMPARE(event.eventType(), QScxmlEvent::ExternalEvent);
    });
    QVERIFY(con1);

    auto con2 = stateMachine->connectToEvent("externalEvent", [&events](const QScxmlEvent &event) {
        QCOMPARE(++events, 2);
        QCOMPARE(event.name(), QString("externalEvent"));
        QCOMPARE(event.eventType(), QScxmlEvent::ExternalEvent);
    });
    QVERIFY(con2);

    auto con3 = stateMachine->connectToEvent("timeout", [&events](const QScxmlEvent &event) {
        QCOMPARE(++events, 3);
        QCOMPARE(event.name(), QString("timeout"));
        QCOMPARE(event.eventType(), QScxmlEvent::ExternalEvent);
    });
    QVERIFY(con3);

    auto con4 = stateMachine->connectToEvent("done.*", [&events](const QScxmlEvent &event) {
        QCOMPARE(++events, 4);
        QCOMPARE(event.name(), QString("done.state.top"));
        QCOMPARE(event.eventType(), QScxmlEvent::ExternalEvent);
    });
    QVERIFY(con4);

    auto con5 = stateMachine->connectToEvent("done.state", [&events](const QScxmlEvent &event) {
        QCOMPARE(++events, 5);
        QCOMPARE(event.name(), QString("done.state.top"));
        QCOMPARE(event.eventType(), QScxmlEvent::ExternalEvent);
    });
    QVERIFY(con5);

    auto con6 = stateMachine->connectToEvent("done.state.top", [&events](const QScxmlEvent &event) {
        QCOMPARE(++events, 6);
        QCOMPARE(event.name(), QString("done.state.top"));
        QCOMPARE(event.eventType(), QScxmlEvent::ExternalEvent);
    });
    QVERIFY(con6);

    stateMachine->start();

    finishedSpy.wait(5000);
    QCOMPARE(events, 6);

    QVERIFY(disconnect(con1));
    QVERIFY(disconnect(con2));
    QVERIFY(disconnect(con3));
    QVERIFY(disconnect(con4));
    QVERIFY(disconnect(con5));
    QVERIFY(disconnect(con6));

    QTRY_VERIFY(!hasChildEventRouters(stateMachine.data()));
}

void tst_StateMachine::doneDotStateEvent()
{
    QScopedPointer<QScxmlStateMachine> stateMachine(QScxmlStateMachine::fromFile(QString(":/tst_statemachine/stateDotDoneEvent.scxml")));
    QVERIFY(!stateMachine.isNull());

    QSignalSpy finishedSpy(stateMachine.data(), SIGNAL(finished()));

    stateMachine->start();
    finishedSpy.wait(5000);
    QCOMPARE(finishedSpy.count(), 1);
    QCOMPARE(stateMachine->activeStateNames(true).size(), 1);
    qDebug() << stateMachine->activeStateNames(true);
    QVERIFY(stateMachine->activeStateNames(true).contains(QLatin1String("success")));
}

void tst_StateMachine::running()
{
    QScopedPointer<QScxmlStateMachine> stateMachine(
                QScxmlStateMachine::fromFile(QString(":/tst_statemachine/statenames.scxml")));
    QVERIFY(!stateMachine.isNull());

    QSignalSpy runningChangedSpy(stateMachine.data(), SIGNAL(runningChanged(bool)));

    QCOMPARE(stateMachine->isRunning(), false);

    stateMachine->start();

    QCOMPARE(runningChangedSpy.count(), 1);
    QCOMPARE(stateMachine->isRunning(), true);

    stateMachine->stop();

    QCOMPARE(runningChangedSpy.count(), 2);
    QCOMPARE(stateMachine->isRunning(), false);
}

QTEST_MAIN(tst_StateMachine)

#include "tst_statemachine.moc"


