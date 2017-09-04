/****************************************************************************
**
** Copyright (C) 2015 The Qt Company Ltd.
** Contact: http://www.qt.io/licensing/
**
** This file is part of the test suite of the Qt Toolkit.
**
** $QT_BEGIN_LICENSE:LGPL21$
** Commercial License Usage
** Licensees holding valid commercial Qt licenses may use this file in
** accordance with the commercial license agreement provided with the
** Software or, alternatively, in accordance with the terms contained in
** a written agreement between you and The Qt Company. For licensing terms
** and conditions see http://www.qt.io/terms-conditions. For further
** information use the contact form at http://www.qt.io/contact-us.
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
** As a special exception, The Qt Company gives you certain additional
** rights. These rights are described in The Qt Company LGPL Exception
** version 1.1, included in the file LGPL_EXCEPTION.txt in this package.
**
** $QT_END_LICENSE$
**
****************************************************************************/


#include <QtTest/QtTest>
#include <qdebug.h>

#include <QtScript/qscriptengine.h>
#include <QtScript/qscriptable.h>

class MyScriptable : public QObject, public QScriptable
{
    Q_OBJECT
    Q_PROPERTY(int baz READ baz WRITE setBaz)
    Q_PROPERTY(QObject* zab READ zab WRITE setZab)
    Q_PROPERTY(int 0 READ baz)
    Q_PROPERTY(QObject* 1 READ zab)
    Q_PROPERTY(int oof WRITE setOof)
public:
    MyScriptable(QObject *parent = 0)
        : QObject(parent), m_lastEngine(0)
        { }
    ~MyScriptable() { }

    QScriptEngine *lastEngine() const;

    void setOof(int)
        { m_oofThisObject = context()->thisObject(); }
    QScriptValue oofThisObject() const
        { return m_oofThisObject; }

    void emitSig(int value)
        { emit sig(value); }

public slots:
    void foo();
    void setX(int x);
    void setX(const QString &x);
    void setX2(int x);
    bool isBar();
    int baz();
    void setBaz(int x);
    void evalIsBar();
    bool useInAnotherEngine();
    void setOtherEngine();
    QObject *zab();
    QObject *setZab(QObject *);
    QScriptValue getArguments();
    int getArgumentCount();

signals:
    void sig(int);

private:
    QScriptEngine *m_lastEngine;
    QScriptEngine *m_otherEngine;
    QScriptValue m_oofThisObject;
};

QScriptEngine *MyScriptable::lastEngine() const
{
    return m_lastEngine;
}

int MyScriptable::baz()
{
    m_lastEngine = engine();
    return 123;
}

void MyScriptable::setBaz(int)
{
    m_lastEngine = engine();
}

QObject *MyScriptable::zab()
{
    return thisObject().toQObject();
}

QObject *MyScriptable::setZab(QObject *)
{
    return thisObject().toQObject();
}

QScriptValue MyScriptable::getArguments()
{
    return context()->argumentsObject();
}

int MyScriptable::getArgumentCount()
{
    return argumentCount();
}

void MyScriptable::foo()
{
    m_lastEngine = engine();
    if (engine())
        context()->throwError("MyScriptable.foo");
}

void MyScriptable::evalIsBar()
{
    engine()->evaluate("this.isBar()");
    m_lastEngine = engine();
}

bool MyScriptable::useInAnotherEngine()
{
    QScriptEngine eng;
    eng.globalObject().setProperty("foo", eng.newQObject(this));
    eng.evaluate("foo.baz()");
    m_lastEngine = engine();
    return (m_otherEngine == &eng);
}

void MyScriptable::setOtherEngine()
{
    m_otherEngine = engine();
}

void MyScriptable::setX(int x)
{
    m_lastEngine = engine();
    if (engine())
        thisObject().setProperty("x", QScriptValue(engine(), x));
}

void MyScriptable::setX(const QString &x)
{
    m_lastEngine = engine();
    if (engine())
        thisObject().setProperty("x", QScriptValue(engine(), x));
}

void MyScriptable::setX2(int)
{
    m_lastEngine = engine();
    thisObject().setProperty("x", argument(0));
}

bool MyScriptable::isBar()
{
    m_lastEngine = engine();
    QString str = thisObject().toString();
    return str.contains(QLatin1Char('@'));
}

class tst_QScriptable : public QObject
{
    Q_OBJECT

public:
    tst_QScriptable();
    virtual ~tst_QScriptable();

private slots:
    void initTestCase();
    void cleanupTestCase();

    void engine();
    void thisObject();
    void arguments();
    void throwError();

private:
    QScriptEngine m_engine;
    MyScriptable m_scriptable;
};

tst_QScriptable::tst_QScriptable()
{
}

tst_QScriptable::~tst_QScriptable()
{
}

void tst_QScriptable::initTestCase()
{
    QScriptValue obj = m_engine.newQObject(&m_scriptable);
    m_engine.globalObject().setProperty("scriptable", obj);
}

void tst_QScriptable::cleanupTestCase()
{
}

void tst_QScriptable::engine()
{
    QCOMPARE(m_scriptable.engine(), (QScriptEngine*)0);
    QCOMPARE(m_scriptable.context(), (QScriptContext*)0);
    QCOMPARE(m_scriptable.lastEngine(), (QScriptEngine *)0);

    // reading property
    {
        QScriptValue ret = m_engine.evaluate("scriptable.baz");
        QCOMPARE(ret.strictlyEquals(QScriptValue(&m_engine, 123)), true);
    }
    QCOMPARE(m_scriptable.lastEngine(), &m_engine);
    {
        QScriptValue ret = m_engine.evaluate("scriptable[0]");
        QCOMPARE(ret.strictlyEquals(QScriptValue(&m_engine, 123)), true);
    }
    QCOMPARE(m_scriptable.lastEngine(), &m_engine);
    // when reading from C++, engine() should be 0
    (void)m_scriptable.property("baz");
    QCOMPARE(m_scriptable.lastEngine(), (QScriptEngine *)0);

    // writing property
    m_engine.evaluate("scriptable.baz = 123");
    QCOMPARE(m_scriptable.lastEngine(), &m_engine);
    (void)m_scriptable.setProperty("baz", 123);
    QCOMPARE(m_scriptable.lastEngine(), (QScriptEngine *)0);

    // calling slot
    m_engine.evaluate("scriptable.setX(123)");
    QCOMPARE(m_scriptable.lastEngine(), &m_engine);
    QCOMPARE(m_engine.evaluate("scriptable.x")
             .strictlyEquals(QScriptValue(&m_engine, 123)), true);
    (void)m_scriptable.setProperty("baz", 123);
    QCOMPARE(m_scriptable.lastEngine(), (QScriptEngine *)0);

    // calling overloaded slot
    m_engine.evaluate("scriptable.setX('123')");
    QCOMPARE(m_scriptable.lastEngine(), &m_engine);
    QCOMPARE(m_engine.evaluate("scriptable.x")
             .strictlyEquals(QScriptValue(&m_engine, QLatin1String("123"))), true);

    // calling a slot from another slot
    m_engine.evaluate("scriptable.evalIsBar()");
    QCOMPARE(m_scriptable.lastEngine(), &m_engine);

    // calling a slot that registers m_scriptable in a different engine
    // and calls evaluate()
    {
        QScriptValue ret = m_engine.evaluate("scriptable.useInAnotherEngine()");
        QCOMPARE(m_scriptable.lastEngine(), &m_engine);
    }
}

void tst_QScriptable::thisObject()
{
    QVERIFY(!m_scriptable.thisObject().isValid());

    m_engine.evaluate("o = { }");
    {
        QScriptValue ret = m_engine.evaluate("o.__proto__ = scriptable;"
                                             "o.setX(123);"
                                             "o.__proto__ = Object.prototype;"
                                             "o.x");
        QCOMPARE(m_scriptable.lastEngine(), &m_engine);
        QCOMPARE(ret.strictlyEquals(QScriptValue(&m_engine, 123)), true);
    }
    {
        QScriptValue ret = m_engine.evaluate("o.__proto__ = scriptable;"
                                             "o.setX2(456);"
                                             "o.__proto__ = Object.prototype;"
                                             "o.x");
        QCOMPARE(m_scriptable.lastEngine(), &m_engine);
        QCOMPARE(ret.strictlyEquals(QScriptValue(&m_engine, 456)), true);
    }
    m_engine.evaluate("o.__proto__ = scriptable");
    {
        QScriptValue ret = m_engine.evaluate("o.isBar()");
        QCOMPARE(m_scriptable.lastEngine(), &m_engine);
        QCOMPARE(ret.strictlyEquals(QScriptValue(&m_engine, false)), true);
    }
    {
        QScriptValue ret = m_engine.evaluate("o.toString = function() { return 'foo@bar'; }; o.isBar()");
        QCOMPARE(m_scriptable.lastEngine(), &m_engine);
        QCOMPARE(ret.strictlyEquals(QScriptValue(&m_engine, true)), true);
    }

    // property getter
    {
        QScriptValue ret = m_engine.evaluate("scriptable.zab");
        QCOMPARE(m_scriptable.lastEngine(), &m_engine);
        QCOMPARE(ret.isQObject(), true);
        QCOMPARE(ret.toQObject(), (QObject *)&m_scriptable);
    }
    {
        QScriptValue ret = m_engine.evaluate("scriptable[1]");
        QCOMPARE(m_scriptable.lastEngine(), &m_engine);
        QCOMPARE(ret.isQObject(), true);
        QCOMPARE(ret.toQObject(), (QObject *)&m_scriptable);
    }
    {
        QScriptValue ret = m_engine.evaluate("o.zab");
        QCOMPARE(m_scriptable.lastEngine(), &m_engine);
        QCOMPARE(ret.toQObject(), (QObject *)0);
    }
    // property setter
    {
        QScriptValue ret = m_engine.evaluate("scriptable.setZab(null)");
        QCOMPARE(m_scriptable.lastEngine(), &m_engine);
        QCOMPARE(ret.isQObject(), true);
        QCOMPARE(ret.toQObject(), (QObject *)&m_scriptable);
    }
    {
        QVERIFY(!m_scriptable.oofThisObject().isValid());
        m_engine.evaluate("o.oof = 123");
        QCOMPARE(m_scriptable.lastEngine(), &m_engine);
        QVERIFY(m_scriptable.oofThisObject().strictlyEquals(m_engine.evaluate("o")));
    }
    {
        m_engine.evaluate("scriptable.oof = 123");
        QCOMPARE(m_scriptable.lastEngine(), &m_engine);
        QVERIFY(m_scriptable.oofThisObject().strictlyEquals(m_engine.evaluate("scriptable")));
    }

    // target of signal
    {
        {
            QScriptValue ret = m_engine.evaluate("scriptable.sig.connect(o, scriptable.setX)");
            QCOMPARE(m_scriptable.lastEngine(), &m_engine);
            QVERIFY(ret.isUndefined());
        }
        QVERIFY(m_engine.evaluate("o.x").strictlyEquals(QScriptValue(&m_engine, 456)));
        QCOMPARE(m_scriptable.lastEngine(), &m_engine);
        m_scriptable.emitSig(654321);
        QVERIFY(m_engine.evaluate("o.x").strictlyEquals(QScriptValue(&m_engine, 654321)));
        QCOMPARE(m_scriptable.lastEngine(), &m_engine);
        {
            QScriptValue ret = m_engine.evaluate("scriptable.sig.disconnect(o, scriptable.setX)");
            QCOMPARE(m_scriptable.lastEngine(), &m_engine);
            QVERIFY(ret.isUndefined());
        }
    }

    m_engine.evaluate("delete o");
}

void tst_QScriptable::arguments()
{
    // even though the C++ slot accepts zero arguments, it should
    // still be invoked; the arguments should be accessible through
    // the QScriptable API
    QScriptValue args = m_engine.evaluate("scriptable.getArguments(10, 20, 30, 'hi')");
    QVERIFY(args.property("length").strictlyEquals(QScriptValue(&m_engine, 4)));
    QVERIFY(args.property("0").strictlyEquals(QScriptValue(&m_engine, 10)));
    QVERIFY(args.property("1").strictlyEquals(QScriptValue(&m_engine, 20)));
    QVERIFY(args.property("2").strictlyEquals(QScriptValue(&m_engine, 30)));
    QVERIFY(args.property("3").strictlyEquals(QScriptValue(&m_engine, "hi")));

    QScriptValue argc = m_engine.evaluate("scriptable.getArgumentCount(1, 2, 3)");
    QVERIFY(argc.isNumber());
    QCOMPARE(argc.toInt32(), 3);

    QCOMPARE(m_scriptable.argumentCount(), -1);
    QVERIFY(!m_scriptable.argument(-1).isValid());
    QVERIFY(!m_scriptable.argument(0).isValid());
}

void tst_QScriptable::throwError()
{
    QScriptValue ret = m_engine.evaluate("scriptable.foo()");
    QCOMPARE(m_scriptable.lastEngine(), &m_engine);
    QCOMPARE(ret.isError(), true);
    QCOMPARE(ret.toString(), QString("Error: MyScriptable.foo"));
}

QTEST_MAIN(tst_QScriptable)
#include "tst_qscriptable.moc"
