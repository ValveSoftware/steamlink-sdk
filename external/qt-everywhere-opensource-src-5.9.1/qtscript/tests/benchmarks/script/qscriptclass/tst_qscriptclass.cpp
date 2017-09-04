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

#include <qtest.h>
#include <QtScript>

Q_DECLARE_METATYPE(QScriptContext*)
Q_DECLARE_METATYPE(QScriptValue)
Q_DECLARE_METATYPE(QScriptValueList)

// We want reliable numbers so we don't want to rely too much
// on the number of iterations done by testlib.
// this also make the results of valgrind more interesting
const int iterationNumber = 5000;

class tst_QScriptClass : public QObject
{
    Q_OBJECT

private slots:
    void noSuchProperty();
    void property();
    void setProperty();
    void propertyFlags();
    void call();
    void hasInstance();
    void iterate();
};

// Test the overhead of checking for an inexisting property of a QScriptClass
void tst_QScriptClass::noSuchProperty()
{
    QScriptEngine eng;
    QScriptClass cls(&eng);
    QScriptValue obj = eng.newObject(&cls);
    QString propertyName = QString::fromLatin1("foo");
    QBENCHMARK {
        for (int i = 0; i < iterationNumber; ++i)
            (void)obj.property(propertyName);
    }
    Q_ASSERT(!obj.property(propertyName).isValid());
}


class FooScriptClass : public QScriptClass
{
public:
    FooScriptClass(QScriptEngine *engine)
        : QScriptClass(engine)
    {
        foo = engine->toStringHandle("foo");
    }

    QueryFlags queryProperty(const QScriptValue &,
                             const QScriptString &,
                             QueryFlags flags,
                             uint *id)
    {
        *id = 1;
        return flags;
    }

    QScriptValue property(const QScriptValue &,
                          const QScriptString &,
                          uint)
    {
        return QScriptValue(engine(), 35);
    }

    void setProperty(QScriptValue &, const QScriptString &,
                     uint, const QScriptValue &)
    {}

    QScriptValue::PropertyFlags propertyFlags(const QScriptValue &, const QScriptString &, uint)
    {
        return QScriptValue::Undeletable;
    }
private:
    QScriptString foo;
};

// Test the overhead of getting a value of QScriptClass across the Javascript engine
void tst_QScriptClass::property()
{
    QScriptEngine eng;
    FooScriptClass cls(&eng);
    QScriptValue obj = eng.newObject(&cls);
    QScriptString foo = eng.toStringHandle("foo");
    QBENCHMARK {
        for (int i = 0; i < iterationNumber; ++i)
            (void)obj.property(foo);
    }
}

// Test the overhead of setting a value on QScriptClass across the Javascript engine
void tst_QScriptClass::setProperty()
{
    QScriptEngine eng;
    FooScriptClass cls(&eng);
    QScriptValue obj = eng.newObject(&cls);
    QScriptValue value(456);
    QScriptString foo = eng.toStringHandle("foo");
    QBENCHMARK {
        for (int i = 0; i < iterationNumber; ++i)
            obj.setProperty(foo, value);
    }
}

// Test the time taken to get the propeties flags across the engine
void tst_QScriptClass::propertyFlags()
{
    QScriptEngine eng;
    FooScriptClass cls(&eng);
    QScriptValue obj = eng.newObject(&cls);
    QScriptString foo = eng.toStringHandle("foo");
    QBENCHMARK {
        for (int i = 0; i < iterationNumber; ++i)
            (void)obj.propertyFlags(foo);
    }
}



class ExtensionScriptClass : public QScriptClass
{
public:
    ExtensionScriptClass(QScriptEngine *engine)
        : QScriptClass(engine)
    {
    }

    bool supportsExtension(Extension) const
    {
        return true;
    }

    QVariant extension(Extension, const QVariant &argument = QVariant())
    {
        Q_UNUSED(argument);
        return QVariant();
    }
};

// Check the overhead of the extension "call"
void tst_QScriptClass::call()
{
    QScriptEngine eng;
    ExtensionScriptClass cls(&eng);
    QScriptValue obj = eng.newObject(&cls);
    QScriptValue thisObject;
    QScriptValueList args;
    args.append(123);
    QBENCHMARK {
        for (int i = 0; i < iterationNumber; ++i)
            (void)obj.call(thisObject, args);
    }
}

// Check the overhead of the extension "instanceOf"
void tst_QScriptClass::hasInstance()
{
    QScriptEngine eng;
    ExtensionScriptClass cls(&eng);
    QScriptValue obj = eng.newObject(&cls);
    obj.setProperty("foo", 123);
    QScriptValue plain = eng.newObject();
    plain.setProperty("foo", obj.property("foo"));
    QBENCHMARK {
        for (int i = 0; i < iterationNumber; ++i)
            (void)plain.instanceOf(obj);
    }
}



static const int iteratorValuesNumber = 100;
class TestClassPropertyIterator : public QScriptClassPropertyIterator
{
public:
    TestClassPropertyIterator(const QScriptValue &object, QVector<QScriptString> names)
        : QScriptClassPropertyIterator(object)
        , m_index(0)
        , names(names)
    {
    }

    bool hasNext() const
    {
        return m_index < iteratorValuesNumber - 1;
    }
    void next() { ++m_index; }

    bool hasPrevious() const { return m_index > 0; }
    void previous() { --m_index; }

    void toFront() { m_index = 0; }
    void toBack() { m_index = iteratorValuesNumber - 1; }

    QScriptString name() const { return names[m_index]; }
    uint id() const { return m_index; }
    QScriptValue::PropertyFlags flags() const { return 0; }

private:
    int m_index;
    QVector<QScriptString> names;
};


class IteratorScriptClass : public QScriptClass
{
public:
    IteratorScriptClass(QScriptEngine *engine)
        : QScriptClass(engine)
    {
        for (int i = 0; i < iteratorValuesNumber; ++i)
            names.append(engine->toStringHandle(QString("property%1").arg(i)));
    }

    QScriptClassPropertyIterator *newIterator(const QScriptValue &object)
    {
        return new TestClassPropertyIterator(object, names);
    }
private:
    QVector<QScriptString> names;
    friend class TestClassPropertyIterator;
};

// Measure the performance of the interface to iterate over QScriptClassPropertyIterator
void tst_QScriptClass::iterate()
{
    QScriptEngine eng;
    IteratorScriptClass cls(&eng);
    QScriptValue obj = eng.newObject(&cls);
    int iterationNumberIterate = iterationNumber / iteratorValuesNumber;
    QBENCHMARK {
        for (int i = 0; i < iterationNumberIterate; ++i) {
            QScriptValueIterator it(obj);
            while (it.hasNext()) {
                it.next();
                (void)it.scriptName();
            }
        }
    }
}

QTEST_MAIN(tst_QScriptClass)
#include "tst_qscriptclass.moc"
