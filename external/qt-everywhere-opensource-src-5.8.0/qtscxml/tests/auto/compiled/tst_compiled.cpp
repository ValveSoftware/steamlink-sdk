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
#include <QtScxml/qscxmlinvokableservice.h>
#include "ids1.h"
#include "statemachineunicodename.h"
#include "datainnulldatamodel.h"
#include "submachineunicodename.h"
#include "eventnames1.h"
#include "connection.h"
#include "topmachine.h"

Q_DECLARE_METATYPE(QScxmlError);

enum { SpyWaitTime = 8000 };

class tst_Compiled: public QObject
{
    Q_OBJECT

private Q_SLOTS:
    void stateNames();
    void nullDataInit();
    void subMachineUnicodeName();
    void unicodeEventName();
    void connection();
    void myConnection();
    void topMachine();
    void topMachineDynamic();
    void publicSignals();
};

void tst_Compiled::stateNames()
{
    ids1 stateMachine;

    // The states have to be appear in document order:
    QStringList ids1States({
        "foo.bar",
        "foo-bar",
        "foo_bar",
        "_",
        "näl",
        "n_0xe4_l",
        "_VALID",
        "__valid",
        "qÿ̀i",
    });

    QCOMPARE(stateMachine.stateNames(false), ids1States);

    for (const QString &state : qAsConst(ids1States)) {
        QVariant prop = stateMachine.property(state.toUtf8().constData());
        QVERIFY(!prop.isNull());
        QVERIFY(prop.isValid());
        QCOMPARE(prop.toBool(), false);
    }

    QVariant invalidProp = stateMachine.property("blabla");
    QVERIFY(invalidProp.isNull());
    QVERIFY(!invalidProp.isValid());

    QStringList calculatorStates(QLatin1String("wrapper"));

    Calculator_0xe4_tateMachine stateMachine3;
    QCOMPARE(stateMachine3.stateNames(false), calculatorStates);
}

void tst_Compiled::nullDataInit()
{
    DataInNullDataModel nullData;
    QVERIFY(!nullData.init()); // raises an error, but doesn't crash
}

void tst_Compiled::subMachineUnicodeName()
{
    Directions1 directions;
    QSignalSpy stableStateSpy(&directions, SIGNAL(reachedStableState()));
    directions.start();
    stableStateSpy.wait(5000);
    QScxmlInvokableService *service = directions.invokedServices().value(0);
    QVERIFY(service);
    QCOMPARE(service->name(), QString("änywhere"));
}

void tst_Compiled::unicodeEventName()
{
    eventnames1 names;
    QSignalSpy stableStateSpy(&names, SIGNAL(reachedStableState()));
    names.start();

    stableStateSpy.wait(5000);

    QCOMPARE(names.activeStateNames(), QStringList(QLatin1String("a")));
    names.submitEvent("näl");
    stableStateSpy.wait(5000);
    QCOMPARE(names.activeStateNames(), QStringList(QLatin1String("b")));
}

class Receiver : public QObject
{
    Q_OBJECT
public slots:
    void receive(bool enabled)
    {
        received = received || enabled;
    }

    void enter()
    {
        entered = true;
    }

    void exit()
    {
        exited = true;
    }

public:
    bool received = false;
    bool entered = false;
    bool exited = false;
};

void tst_Compiled::connection()
{
    Connection stateMachine;

    Receiver receiverA;
    Receiver receiverA1;
    Receiver receiverA2;
    Receiver receiverB;
    Receiver receiverFinal;

    QMetaObject::Connection conA     = stateMachine.connectToState("a",     &receiverA,     SLOT(receive(bool)));
    QMetaObject::Connection conA1    = stateMachine.connectToState("a1",    &receiverA1,    SLOT(receive(bool)));
    QMetaObject::Connection conA2    = stateMachine.connectToState("a2",    &receiverA2,    SLOT(receive(bool)));
    QMetaObject::Connection conB     = stateMachine.connectToState("b",     &receiverB,     SLOT(receive(bool)));
    QMetaObject::Connection conFinal = stateMachine.connectToState("final", &receiverFinal, SLOT(receive(bool)));

    typedef QScxmlStateMachine QXSM;
    QMetaObject::Connection aEntry = stateMachine.connectToState("a", QXSM::onEntry(&receiverA, "enter"));
    QMetaObject::Connection aExit  = stateMachine.connectToState("a", QXSM::onExit(&receiverA, "exit"));

    QVERIFY(aEntry);
    QVERIFY(aExit);

    QVERIFY(conA);
    QVERIFY(conA1);
    QVERIFY(conA2);
    QVERIFY(conB);
    QVERIFY(conFinal);

    stateMachine.start();

    QTRY_VERIFY(receiverA.received);
    QTRY_VERIFY(receiverA1.received);
    QTRY_VERIFY(!receiverA2.received);
    QTRY_VERIFY(receiverB.received);
    QTRY_VERIFY(receiverFinal.received);

    QVERIFY(disconnect(conA));
    QVERIFY(disconnect(conA1));
    QVERIFY(disconnect(conA2));
    QVERIFY(disconnect(conB));
    QVERIFY(disconnect(conFinal));

#if defined(__cpp_return_type_deduction) && __cpp_return_type_deduction == 201304
    QVERIFY(receiverA.entered);
    QVERIFY(!receiverA.exited);
    QVERIFY(disconnect(aEntry));
    QVERIFY(disconnect(aExit));
#endif
}

class MyConnection : public Connection
{
    Q_OBJECT
public:
    MyConnection(QObject *parent = 0)
        : Connection(parent)
    {}
};

void tst_Compiled::myConnection()
{
    MyConnection stateMachine;

    Receiver receiverA;
    Receiver receiverA1;
    Receiver receiverA2;
    Receiver receiverB;
    Receiver receiverFinal;

    QMetaObject::Connection conA     = stateMachine.connectToState("a",     &receiverA,     SLOT(receive(bool)));
    QMetaObject::Connection conA1    = stateMachine.connectToState("a1",    &receiverA1,    SLOT(receive(bool)));
    QMetaObject::Connection conA2    = stateMachine.connectToState("a2",    &receiverA2,    SLOT(receive(bool)));
    QMetaObject::Connection conB     = stateMachine.connectToState("b",     &receiverB,     SLOT(receive(bool)));
    QMetaObject::Connection conFinal = stateMachine.connectToState("final", &receiverFinal, SLOT(receive(bool)));

    QVERIFY(conA);
    QVERIFY(conA1);
    QVERIFY(conA2);
    QVERIFY(conB);
    QVERIFY(conFinal);

    stateMachine.start();

    QTRY_VERIFY(receiverA.received);
    QTRY_VERIFY(receiverA1.received);
    QTRY_VERIFY(!receiverA2.received);
    QTRY_VERIFY(receiverB.received);
    QTRY_VERIFY(receiverFinal.received);

    QVERIFY(disconnect(conA));
    QVERIFY(disconnect(conA1));
    QVERIFY(disconnect(conA2));
    QVERIFY(disconnect(conB));
    QVERIFY(disconnect(conFinal));
}

void tst_Compiled::topMachine()
{
    TopMachine stateMachine;
    int doneCounter = 0;
    int invokableServicesCount = 0;

    stateMachine.connectToEvent("done.invoke.submachine", [&doneCounter](const QScxmlEvent &) {
        ++doneCounter;
    });

    QObject::connect(&stateMachine, &QScxmlStateMachine::invokedServicesChanged,
                     [&invokableServicesCount](const QVector<QScxmlInvokableService *> &services) {
        invokableServicesCount = services.count();
    });

    stateMachine.start();

    QTRY_COMPARE(invokableServicesCount, 3);
    QTRY_COMPARE(doneCounter, 3);
    QCOMPARE(stateMachine.invokedServices().count(), 3);
    QTRY_COMPARE(invokableServicesCount, 0);
}

void tst_Compiled::topMachineDynamic()
{
    QScopedPointer<QScxmlStateMachine> stateMachine(
                QScxmlStateMachine::fromFile(QString(":/topmachine.scxml")));
    QVERIFY(!stateMachine.isNull());
    int doneCounter = 0;
    int invokableServicesCount = 0;

    stateMachine->connectToEvent("done.invoke.submachine", [&doneCounter](const QScxmlEvent &) {
        ++doneCounter;
    });

    QObject::connect(stateMachine.data(), &QScxmlStateMachine::invokedServicesChanged,
                     [&invokableServicesCount](const QVector<QScxmlInvokableService *> &services) {
        invokableServicesCount = services.count();
    });

    stateMachine->start();

    QTRY_COMPARE(invokableServicesCount, 3);
    QTRY_COMPARE(doneCounter, 3);
    QCOMPARE(stateMachine->invokedServices().count(), 3);
    QTRY_COMPARE(invokableServicesCount, 0);
}

void tst_Compiled::publicSignals()
{
    const QMetaObject *connectionMeta = &Connection::staticMetaObject;
    int index = connectionMeta->indexOfSignal("aChanged(bool)");
    QVERIFY(index >= 0);

    QMetaMethod aChanged = connectionMeta->method(index);
    QVERIFY(aChanged.isValid());
    QCOMPARE(aChanged.access(), QMetaMethod::Public);
}

QTEST_MAIN(tst_Compiled)

#include "tst_compiled.moc"


