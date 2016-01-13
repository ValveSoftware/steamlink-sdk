/****************************************************************************
**
** Copyright (C) 2014 Digia Plc and/or its subsidiary(-ies).
** Contact: http://www.qt-project.org/legal
**
** This file is part of the test suite of the Qt Toolkit.
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

#include <qtest.h>
#include <QDebug>
#include <QDeclarativeEngine>
#include <QDeclarativeContext>
#include <QDeclarativeComponent>
#include <QDeclarativeExpression>

class tst_qdeclarativecontext : public QObject
{
    Q_OBJECT
public:
    tst_qdeclarativecontext() {}

private slots:
    void baseUrl();
    void resolvedUrl();
    void engineMethod();
    void parentContext();
    void setContextProperty();
    void setContextObject();
    void destruction();
    void idAsContextProperty();
    void readOnlyContexts();

private:
    QDeclarativeEngine engine;
};

void tst_qdeclarativecontext::baseUrl()
{
    QDeclarativeContext ctxt(&engine);

    QCOMPARE(ctxt.baseUrl(), QUrl());

    ctxt.setBaseUrl(QUrl("http://www.qt-project.org/"));

    QCOMPARE(ctxt.baseUrl(), QUrl("http://www.qt-project.org/"));
}

void tst_qdeclarativecontext::resolvedUrl()
{
    // Relative to the component
    {
        QDeclarativeContext ctxt(&engine);
        ctxt.setBaseUrl(QUrl("http://www.qt-project.org/"));

        QCOMPARE(ctxt.resolvedUrl(QUrl("main.qml")), QUrl("http://www.qt-project.org/main.qml"));
    }

    // Relative to a parent
    {
        QDeclarativeContext ctxt(&engine);
        ctxt.setBaseUrl(QUrl("http://www.qt-project.org/"));

        QDeclarativeContext ctxt2(&ctxt);
        QCOMPARE(ctxt2.resolvedUrl(QUrl("main2.qml")), QUrl("http://www.qt-project.org/main2.qml"));
    }

    // Relative to the engine
    {
        QDeclarativeContext ctxt(&engine);
        QCOMPARE(ctxt.resolvedUrl(QUrl("main.qml")), engine.baseUrl().resolved(QUrl("main.qml")));
    }

    // Relative to a deleted parent
    {
        QDeclarativeContext *ctxt = new QDeclarativeContext(&engine);
        ctxt->setBaseUrl(QUrl("http://www.qt-project.org/"));

        QDeclarativeContext ctxt2(ctxt);
        QCOMPARE(ctxt2.resolvedUrl(QUrl("main2.qml")), QUrl("http://www.qt-project.org/main2.qml"));

        delete ctxt; ctxt = 0;

        QCOMPARE(ctxt2.resolvedUrl(QUrl("main2.qml")), QUrl());
    }

    // Absolute
    {
        QDeclarativeContext ctxt(&engine);

        QCOMPARE(ctxt.resolvedUrl(QUrl("http://www.qt-project.org/main2.qml")), QUrl("http://www.qt-project.org/main2.qml"));
        QCOMPARE(ctxt.resolvedUrl(QUrl("file:///main2.qml")), QUrl("file:///main2.qml"));
    }
}

void tst_qdeclarativecontext::engineMethod()
{
    QDeclarativeEngine *engine = new QDeclarativeEngine;

    QDeclarativeContext ctxt(engine);
    QDeclarativeContext ctxt2(&ctxt);
    QDeclarativeContext ctxt3(&ctxt2);
    QDeclarativeContext ctxt4(&ctxt2);

    QCOMPARE(ctxt.engine(), engine);
    QCOMPARE(ctxt2.engine(), engine);
    QCOMPARE(ctxt3.engine(), engine);
    QCOMPARE(ctxt4.engine(), engine);

    delete engine; engine = 0;

    QCOMPARE(ctxt.engine(), engine);
    QCOMPARE(ctxt2.engine(), engine);
    QCOMPARE(ctxt3.engine(), engine);
    QCOMPARE(ctxt4.engine(), engine);
}

void tst_qdeclarativecontext::parentContext()
{
    QDeclarativeEngine *engine = new QDeclarativeEngine;

    QCOMPARE(engine->rootContext()->parentContext(), (QDeclarativeContext *)0);

    QDeclarativeContext *ctxt = new QDeclarativeContext(engine);
    QDeclarativeContext *ctxt2 = new QDeclarativeContext(ctxt);
    QDeclarativeContext *ctxt3 = new QDeclarativeContext(ctxt2);
    QDeclarativeContext *ctxt4 = new QDeclarativeContext(ctxt2);
    QDeclarativeContext *ctxt5 = new QDeclarativeContext(ctxt);
    QDeclarativeContext *ctxt6 = new QDeclarativeContext(engine);
    QDeclarativeContext *ctxt7 = new QDeclarativeContext(engine->rootContext());

    QCOMPARE(ctxt->parentContext(), engine->rootContext());
    QCOMPARE(ctxt2->parentContext(), ctxt);
    QCOMPARE(ctxt3->parentContext(), ctxt2);
    QCOMPARE(ctxt4->parentContext(), ctxt2);
    QCOMPARE(ctxt5->parentContext(), ctxt);
    QCOMPARE(ctxt6->parentContext(), engine->rootContext());
    QCOMPARE(ctxt7->parentContext(), engine->rootContext());

    delete ctxt2; ctxt2 = 0;

    QCOMPARE(ctxt->parentContext(), engine->rootContext());
    QCOMPARE(ctxt3->parentContext(), (QDeclarativeContext *)0);
    QCOMPARE(ctxt4->parentContext(), (QDeclarativeContext *)0);
    QCOMPARE(ctxt5->parentContext(), ctxt);
    QCOMPARE(ctxt6->parentContext(), engine->rootContext());
    QCOMPARE(ctxt7->parentContext(), engine->rootContext());

    delete engine; engine = 0;

    QCOMPARE(ctxt->parentContext(), (QDeclarativeContext *)0);
    QCOMPARE(ctxt3->parentContext(), (QDeclarativeContext *)0);
    QCOMPARE(ctxt4->parentContext(), (QDeclarativeContext *)0);
    QCOMPARE(ctxt5->parentContext(), (QDeclarativeContext *)0);
    QCOMPARE(ctxt6->parentContext(), (QDeclarativeContext *)0);
    QCOMPARE(ctxt7->parentContext(), (QDeclarativeContext *)0);

    delete ctxt7;
    delete ctxt6;
    delete ctxt5;
    delete ctxt4;
    delete ctxt3;
    delete ctxt;
}

class TestObject : public QObject
{
    Q_OBJECT
    Q_PROPERTY(int a READ a NOTIFY aChanged)
    Q_PROPERTY(int b READ b NOTIFY bChanged)
    Q_PROPERTY(int c READ c NOTIFY cChanged)

public:
    TestObject() : _a(10), _b(10), _c(10) {}

    int a() const { return _a; }
    void setA(int a) { _a = a; emit aChanged(); }

    int b() const { return _b; }
    void setB(int b) { _b = b; emit bChanged(); }

    int c() const { return _c; }
    void setC(int c) { _c = c; emit cChanged(); }

signals:
    void aChanged();
    void bChanged();
    void cChanged();

private:
    int _a;
    int _b;
    int _c;
};

#define TEST_CONTEXT_PROPERTY(ctxt, name, value) \
{ \
    QDeclarativeComponent component(&engine); \
    component.setData("import QtQuick 1.0; QtObject { property variant test: " #name " }", QUrl()); \
\
    QObject *obj = component.create(ctxt); \
\
    QCOMPARE(obj->property("test"), value); \
\
    delete obj; \
}

void tst_qdeclarativecontext::setContextProperty()
{
    QDeclarativeContext ctxt(&engine);
    QDeclarativeContext ctxt2(&ctxt);

    TestObject obj1;
    obj1.setA(3345);
    TestObject obj2;
    obj2.setA(-19);

    // Static context properties
    ctxt.setContextProperty("a", QVariant(10));
    ctxt.setContextProperty("b", QVariant(9));
    ctxt2.setContextProperty("d", &obj2);
    ctxt2.setContextProperty("b", QVariant(19));
    ctxt2.setContextProperty("c", QVariant(QString("Hello World!")));
    ctxt.setContextProperty("d", &obj1);
    ctxt.setContextProperty("e", &obj1);

    TEST_CONTEXT_PROPERTY(&ctxt2, a, QVariant(10));
    TEST_CONTEXT_PROPERTY(&ctxt2, b, QVariant(19));
    TEST_CONTEXT_PROPERTY(&ctxt2, c, QVariant(QString("Hello World!")));
    TEST_CONTEXT_PROPERTY(&ctxt2, d.a, QVariant(-19));
    TEST_CONTEXT_PROPERTY(&ctxt2, e.a, QVariant(3345));

    ctxt.setContextProperty("a", QVariant(13));
    ctxt.setContextProperty("b", QVariant(4));
    ctxt2.setContextProperty("b", QVariant(8));
    ctxt2.setContextProperty("c", QVariant(QString("Hi World!")));
    ctxt2.setContextProperty("d", &obj1);
    obj1.setA(12);

    TEST_CONTEXT_PROPERTY(&ctxt2, a, QVariant(13));
    TEST_CONTEXT_PROPERTY(&ctxt2, b, QVariant(8));
    TEST_CONTEXT_PROPERTY(&ctxt2, c, QVariant(QString("Hi World!")));
    TEST_CONTEXT_PROPERTY(&ctxt2, d.a, QVariant(12));
    TEST_CONTEXT_PROPERTY(&ctxt2, e.a, QVariant(12));

    // Changes in context properties
    {
        QDeclarativeComponent component(&engine);
        component.setData("import QtQuick 1.0; QtObject { property variant test: a }", QUrl());

        QObject *obj = component.create(&ctxt2);

        QCOMPARE(obj->property("test"), QVariant(13));
        ctxt.setContextProperty("a", QVariant(19));
        QCOMPARE(obj->property("test"), QVariant(19));

        delete obj;
    }
    {
        QDeclarativeComponent component(&engine);
        component.setData("import QtQuick 1.0; QtObject { property variant test: b }", QUrl());

        QObject *obj = component.create(&ctxt2);

        QCOMPARE(obj->property("test"), QVariant(8));
        ctxt.setContextProperty("b", QVariant(5));
        QCOMPARE(obj->property("test"), QVariant(8));
        ctxt2.setContextProperty("b", QVariant(1912));
        QCOMPARE(obj->property("test"), QVariant(1912));

        delete obj;
    }
    {
        QDeclarativeComponent component(&engine);
        component.setData("import QtQuick 1.0; QtObject { property variant test: e.a }", QUrl());

        QObject *obj = component.create(&ctxt2);

        QCOMPARE(obj->property("test"), QVariant(12));
        obj1.setA(13);
        QCOMPARE(obj->property("test"), QVariant(13));

        delete obj;
    }

    // New context properties
    {
        QDeclarativeComponent component(&engine);
        component.setData("import QtQuick 1.0; QtObject { property variant test: a }", QUrl());

        QObject *obj = component.create(&ctxt2);

        QCOMPARE(obj->property("test"), QVariant(19));
        ctxt2.setContextProperty("a", QVariant(1945));
        QCOMPARE(obj->property("test"), QVariant(1945));

        delete obj;
    }

    // Setting an object-variant context property
    {
        QDeclarativeComponent component(&engine);
        component.setData("import QtQuick 1.0; QtObject { id: root; property int a: 10; property int test: ctxtProp.a; property variant obj: root; }", QUrl());

        QDeclarativeContext ctxt(engine.rootContext());
        ctxt.setContextProperty("ctxtProp", QVariant());

        QTest::ignoreMessage(QtWarningMsg, "<Unknown File>:1: TypeError: Result of expression 'ctxtProp' [undefined] is not an object.");
        QObject *obj = component.create(&ctxt);

        QVariant v = obj->property("obj");

        ctxt.setContextProperty("ctxtProp", v);

        QCOMPARE(obj->property("test"), QVariant(10));

        delete obj;
    }
}

void tst_qdeclarativecontext::setContextObject()
{
    QDeclarativeContext ctxt(&engine);

    TestObject to;

    to.setA(2);
    to.setB(192);
    to.setC(18);

    ctxt.setContextObject(&to);
    ctxt.setContextProperty("c", QVariant(9));

    // Static context properties
    TEST_CONTEXT_PROPERTY(&ctxt, a, QVariant(2));
    TEST_CONTEXT_PROPERTY(&ctxt, b, QVariant(192));
    TEST_CONTEXT_PROPERTY(&ctxt, c, QVariant(9));

    to.setA(12);
    to.setB(100);
    to.setC(7);
    ctxt.setContextProperty("c", QVariant(3));

    TEST_CONTEXT_PROPERTY(&ctxt, a, QVariant(12));
    TEST_CONTEXT_PROPERTY(&ctxt, b, QVariant(100));
    TEST_CONTEXT_PROPERTY(&ctxt, c, QVariant(3));

    // Changes in context properties
    {
        QDeclarativeComponent component(&engine);
        component.setData("import QtQuick 1.0; QtObject { property variant test: a }", QUrl());

        QObject *obj = component.create(&ctxt);

        QCOMPARE(obj->property("test"), QVariant(12));
        to.setA(14);
        QCOMPARE(obj->property("test"), QVariant(14));

        delete obj;
    }
}

void tst_qdeclarativecontext::destruction()
{
    QDeclarativeContext *ctxt = new QDeclarativeContext(&engine);

    QObject obj;
    QDeclarativeEngine::setContextForObject(&obj, ctxt);
    QDeclarativeExpression expr(ctxt, 0, "a");

    QCOMPARE(ctxt, QDeclarativeEngine::contextForObject(&obj));
    QCOMPARE(ctxt, expr.context());

    delete ctxt; ctxt = 0;

    QCOMPARE(ctxt, QDeclarativeEngine::contextForObject(&obj));
    QCOMPARE(ctxt, expr.context());
}

void tst_qdeclarativecontext::idAsContextProperty()
{
    QDeclarativeComponent component(&engine);
    component.setData("import QtQuick 1.0; QtObject { property variant a; a: QtObject { id: myObject } }", QUrl());

    QObject *obj = component.create();
    QVERIFY(obj);

    QVariant a = obj->property("a");
    QVERIFY(a.userType() == QMetaType::QObjectStar);

    QVariant ctxt = qmlContext(obj)->contextProperty("myObject");
    QVERIFY(ctxt.userType() == QMetaType::QObjectStar);

    QVERIFY(a == ctxt);

    delete obj;
}

// Internal contexts should be read-only
void tst_qdeclarativecontext::readOnlyContexts()
{
    QDeclarativeComponent component(&engine);
    component.setData("import QtQuick 1.0; QtObject { id: me }", QUrl());

    QObject *obj = component.create();
    QVERIFY(obj);

    QDeclarativeContext *context = qmlContext(obj);
    QVERIFY(context);

    QVERIFY(qvariant_cast<QObject*>(context->contextProperty("me")) == obj);
    QVERIFY(context->contextObject() == obj);

    QTest::ignoreMessage(QtWarningMsg, "QDeclarativeContext: Cannot set property on internal context.");
    context->setContextProperty("hello", 12);
    QVERIFY(context->contextProperty("hello") == QVariant());

    QTest::ignoreMessage(QtWarningMsg, "QDeclarativeContext: Cannot set property on internal context.");
    context->setContextProperty("hello", obj);
    QVERIFY(context->contextProperty("hello") == QVariant());

    QTest::ignoreMessage(QtWarningMsg, "QDeclarativeContext: Cannot set context object for internal context.");
    context->setContextObject(0);
    QVERIFY(context->contextObject() == obj);

    delete obj;
}

QTEST_MAIN(tst_qdeclarativecontext)

#include "tst_qdeclarativecontext.moc"
