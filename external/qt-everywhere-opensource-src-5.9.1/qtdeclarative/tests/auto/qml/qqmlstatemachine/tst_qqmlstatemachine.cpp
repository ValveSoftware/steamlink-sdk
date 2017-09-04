/****************************************************************************
**
** Copyright (C) 2016 Ford Motor Company
** Contact: https://www.qt.io/licensing/
**
** This file is part of the test suite module of the Qt Toolkit.
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
#include <QQmlComponent>
#include <QQmlContext>
#include <QQmlEngine>
#include <QTest>
#include "../../shared/util.h"

class tst_qqmlstatemachine : public QQmlDataTest
{
    Q_OBJECT
public:
    tst_qqmlstatemachine();

private slots:
    void tst_cppObjectSignal();
};


class CppObject : public QObject
{
    Q_OBJECT
    Q_PROPERTY(ObjectState objectState READ objectState WRITE setObjectState NOTIFY objectStateChanged)
    Q_ENUMS(ObjectState)
public:
    enum ObjectState {
        State0,
        State1,
        State2
    };

public:
    CppObject()
        : QObject()
        , m_objectState(State0)
    {}

    ObjectState objectState() const { return m_objectState; }
    void setObjectState(ObjectState objectState) { m_objectState = objectState; emit objectStateChanged();}

signals:
    void objectStateChanged();
    void mySignal(int signalState);

private:
    ObjectState m_objectState;
};

tst_qqmlstatemachine::tst_qqmlstatemachine()
{
    QVERIFY(-1 != qmlRegisterUncreatableType<CppObject>("CppObjectEnum", 1, 0, "CppObject", QString()));
}

void tst_qqmlstatemachine::tst_cppObjectSignal()
{
    CppObject cppObject;
    QQmlEngine engine;
    QQmlComponent component(&engine, testFileUrl("cppsignal.qml"));
    QVERIFY(!component.isError());

    QQmlContext *ctxt = engine.rootContext();
    ctxt->setContextProperty("_cppObject", &cppObject);
    QScopedPointer<QObject> rootObject(component.create());
    QVERIFY(rootObject != 0);

    // wait for state machine to start
    QTRY_VERIFY(rootObject->property("running").toBool());

    // emit signal from cpp
    emit cppObject.mySignal(CppObject::State1);

    // check if the signal was propagated
    QTRY_COMPARE(cppObject.objectState(), CppObject::State1);

    // emit signal from cpp
    emit cppObject.mySignal(CppObject::State2);

    // check if the signal was propagated
    QTRY_COMPARE(cppObject.objectState(), CppObject::State2);

    // wait for state machine to finish
    QTRY_VERIFY(!rootObject->property("running").toBool());
}


QTEST_MAIN(tst_qqmlstatemachine)

#include "tst_qqmlstatemachine.moc"
