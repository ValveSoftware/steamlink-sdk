/****************************************************************************
**
** Copyright (C) 2016 The Qt Company Ltd.
** Contact: https://www.qt.io/licensing/
**
** This file is part of the test suite of the Qt Toolkit.
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

#include <QJSEngine>
#include <QJSValue>
#include <QJSValueIterator>

class tst_QJSValueIterator : public QObject
{
    Q_OBJECT

public:
    tst_QJSValueIterator();
    virtual ~tst_QJSValueIterator();

private slots:
    void iterateForward_data();
    void iterateForward();
    void iterateArray_data();
    void iterateArray();
    void iterateString();
#if 0
    void iterateGetterSetter();
#endif
    void assignObjectToIterator();
    void iterateNonObject();
    void iterateOverObjectFromDeletedEngine();
    void iterateWithNext();
};

tst_QJSValueIterator::tst_QJSValueIterator()
{
}

tst_QJSValueIterator::~tst_QJSValueIterator()
{
}

void tst_QJSValueIterator::iterateForward_data()
{
    QTest::addColumn<QStringList>("propertyNames");
    QTest::addColumn<QStringList>("propertyValues");

    QTest::newRow("no properties")
        << QStringList() << QStringList();
    QTest::newRow("foo=bar")
        << (QStringList() << "foo")
        << (QStringList() << "bar");
    QTest::newRow("foo=bar, baz=123")
        << (QStringList() << "foo" << "baz")
        << (QStringList() << "bar" << "123");
    QTest::newRow("foo=bar, baz=123, rab=oof")
        << (QStringList() << "foo" << "baz" << "rab")
        << (QStringList() << "bar" << "123" << "oof");
}

void tst_QJSValueIterator::iterateForward()
{
    QFETCH(QStringList, propertyNames);
    QFETCH(QStringList, propertyValues);
    QMap<QString, QString> pmap;
    QCOMPARE(propertyNames.size(), propertyValues.size());

    QJSEngine engine;
    QJSValue object = engine.newObject();
    for (int i = 0; i < propertyNames.size(); ++i) {
        QString name = propertyNames.at(i);
        QString value = propertyValues.at(i);
        pmap.insert(name, value);
        object.setProperty(name, engine.toScriptValue(value));
    }
    QJSValue otherObject = engine.newObject();
    otherObject.setProperty("foo", engine.toScriptValue(123456));
    otherObject.setProperty("protoProperty", engine.toScriptValue(654321));
    object.setPrototype(otherObject); // should not affect iterator

    QStringList lst;
    QJSValueIterator it(object);
    while (!pmap.isEmpty()) {
        QCOMPARE(it.hasNext(), true);
        QCOMPARE(it.hasNext(), true);
        it.next();
        QString name = it.name();
        QCOMPARE(pmap.contains(name), true);
        QCOMPARE(it.name(), name);
        QCOMPARE(it.value().strictlyEquals(engine.toScriptValue(pmap.value(name))), true);
        pmap.remove(name);
        lst.append(name);
    }

    QCOMPARE(it.hasNext(), false);
    QCOMPARE(it.hasNext(), false);

    it = object;
    for (int i = 0; i < lst.count(); ++i) {
        QCOMPARE(it.hasNext(), true);
        it.next();
        QCOMPARE(it.name(), lst.at(i));
    }
}

void tst_QJSValueIterator::iterateArray_data()
{
    QTest::addColumn<QStringList>("propertyNames");
    QTest::addColumn<QStringList>("propertyValues");

    QTest::newRow("no elements") << QStringList() << QStringList();

    QTest::newRow("0=foo, 1=barr")
        << (QStringList() << "0" << "1")
        << (QStringList() << "foo" << "bar");


    QTest::newRow("0=foo, 3=barr")
        << (QStringList() << "0" << "1" << "2" << "3")
        << (QStringList() << "foo" << "" << "" << "bar");
}

void tst_QJSValueIterator::iterateArray()
{
    QFETCH(QStringList, propertyNames);
    QFETCH(QStringList, propertyValues);

    QJSEngine engine;
    QJSValue array = engine.newArray();

    // Fill the array
    for (int i = 0; i < propertyNames.size(); ++i) {
        array.setProperty(propertyNames.at(i), propertyValues.at(i));
    }

    // Iterate thru array properties. Note that the QJSValueIterator doesn't guarantee
    // any order on the iteration!
    int length = array.property("length").toInt();
    QCOMPARE(length, propertyNames.size());

    bool iteratedThruLength = false;
    QHash<QString, QJSValue> arrayProperties;
    QJSValueIterator it(array);

    // Iterate forward
    while (it.hasNext()) {
        it.next();

        const QString name = it.name();
        if (name == QString::fromLatin1("length")) {
            QVERIFY(it.value().isNumber());
            QCOMPARE(it.value().toInt(), length);
            QVERIFY2(!iteratedThruLength, "'length' appeared more than once during iteration.");
            iteratedThruLength = true;
            continue;
        }

        // Storing the properties we iterate in a hash to compare with test data.
        QVERIFY2(!arrayProperties.contains(name), "property appeared more than once during iteration.");
        arrayProperties.insert(name, it.value());
        QVERIFY(it.value().strictlyEquals(array.property(name)));
    }

    // Verify properties
    QVERIFY(iteratedThruLength);
    QCOMPARE(arrayProperties.size(), propertyNames.size());
    for (int i = 0; i < propertyNames.size(); ++i) {
        QVERIFY(arrayProperties.contains(propertyNames.at(i)));
        QCOMPARE(arrayProperties.value(propertyNames.at(i)).toString(), propertyValues.at(i));
    }

#if 0

    // Iterate backwards
    arrayProperties.clear();
    iteratedThruLength = false;
    it.toBack();

    while (it.hasPrevious()) {
        it.previous();

        const QString name = it.name();
        if (name == QString::fromLatin1("length")) {
            QVERIFY(it.value().isNumber());
            QCOMPARE(it.value().toInt(), length);
            QCOMPARE(it.flags(), QScriptValue::SkipInEnumeration | QScriptValue::Undeletable);
            QVERIFY2(!iteratedThruLength, "'length' appeared more than once during iteration.");
            iteratedThruLength = true;
            continue;
        }

        // Storing the properties we iterate in a hash to compare with test data.
        QVERIFY2(!arrayProperties.contains(name), "property appeared more than once during iteration.");
        arrayProperties.insert(name, it.value());
        QCOMPARE(it.flags(), array.propertyFlags(name));
        QVERIFY(it.value().strictlyEquals(array.property(name)));
    }

    // Verify properties
    QVERIFY(iteratedThruLength);
    QCOMPARE(arrayProperties.size(), propertyNames.size());
    for (int i = 0; i < propertyNames.size(); ++i) {
        QVERIFY(arrayProperties.contains(propertyNames.at(i)));
        QCOMPARE(arrayProperties.value(propertyNames.at(i)).toString(), propertyValues.at(i));
    }

    // ### Do we still need this test?
    // Forward test again but as object
    arrayProperties.clear();
    iteratedThruLength = false;
    QJSValue arrayObject = engine.toObject(array);
    QJSValueIterator it2(arrayObject);

    while (it2.hasNext()) {
        it2.next();

        const QString name = it2.name();
        if (name == QString::fromLatin1("length")) {
            QVERIFY(it2.value().isNumber());
            QCOMPARE(it2.value().toInt(), length);
            QCOMPARE(it2.flags(), QScriptValue::SkipInEnumeration | QScriptValue::Undeletable);
            QVERIFY2(!iteratedThruLength, "'length' appeared more than once during iteration.");
            iteratedThruLength = true;
            continue;
        }

        // Storing the properties we iterate in a hash to compare with test data.
        QVERIFY2(!arrayProperties.contains(name), "property appeared more than once during iteration.");
        arrayProperties.insert(name, it2.value());
        QCOMPARE(it2.flags(), arrayObject.propertyFlags(name));
        QVERIFY(it2.value().strictlyEquals(arrayObject.property(name)));
    }

    // Verify properties
    QVERIFY(iteratedThruLength);
    QCOMPARE(arrayProperties.size(), propertyNames.size());
    for (int i = 0; i < propertyNames.size(); ++i) {
        QVERIFY(arrayProperties.contains(propertyNames.at(i)));
        QCOMPARE(arrayProperties.value(propertyNames.at(i)).toString(), propertyValues.at(i));
    }
#endif
}

void tst_QJSValueIterator::iterateString()
{
    QJSEngine engine;
    QJSValue obj = engine.evaluate("new String('ciao')");
    QVERIFY(obj.property("length").isNumber());
    int length = obj.property("length").toInt();
    QCOMPARE(length, 4);

    QJSValueIterator it(obj);
    QHash<QString, QJSValue> stringProperties;
    bool iteratedThruLength = false;

    while (it.hasNext()) {
        it.next();
        const QString name = it.name();

        if (name == QString::fromLatin1("length")) {
            QVERIFY(it.value().isNumber());
            QCOMPARE(it.value().toInt(), length);
            QVERIFY2(!iteratedThruLength, "'length' appeared more than once during iteration.");
            iteratedThruLength = true;
            continue;
        }

        QVERIFY2(!stringProperties.contains(name), "property appeared more than once during iteration.");
        stringProperties.insert(name, it.value());
        QVERIFY(it.value().strictlyEquals(obj.property(name)));
    }

    QVERIFY(iteratedThruLength);
    QCOMPARE(stringProperties.size(), length);
#if 0
    // And going backwards
    iteratedThruLength = false;
    stringProperties.clear();
    it.toBack();

    while (it.hasPrevious()) {
        it.previous();
        const QString name = it.name();

        if (name == QString::fromLatin1("length")) {
            QVERIFY(it.value().isNumber());
            QCOMPARE(it.value().toInt(), length);
            QVERIFY2(!iteratedThruLength, "'length' appeared more than once during iteration.");
            iteratedThruLength = true;
            continue;
        }

        QVERIFY2(!stringProperties.contains(name), "property appeared more than once during iteration.");
        stringProperties.insert(name, it.value());
        QVERIFY(it.value().strictlyEquals(obj.property(name)));
    }
#endif
}

#if 0 // FIXME what we should to keep from here?
static QJSValue myGetterSetter(QScriptContext *ctx, QJSEngine *)
{
    if (ctx->argumentCount() == 1)
        ctx->thisObject().setProperty("bar", ctx->argument(0));
    return ctx->thisObject().property("bar");
}

static QJSValue myGetter(QScriptContext *ctx, QJSEngine *)
{
    return ctx->thisObject().property("bar");
}

static QJSValue mySetter(QScriptContext *ctx, QJSEngine *)
{
    ctx->thisObject().setProperty("bar", ctx->argument(0));
    return ctx->argument(0);
}

void tst_QJSValueIterator::iterateGetterSetter()
{
    // unified getter/setter function
    {
        QJSEngine eng;
        QJSValue obj = eng.newObject();
        obj.setProperty("foo", eng.newFunction(myGetterSetter),
                        QScriptValue::PropertyGetter | QScriptValue::PropertySetter);
        QJSValue val(&eng, 123);
        obj.setProperty("foo", val);
        QVERIFY(obj.property("bar").strictlyEquals(val));
        QVERIFY(obj.property("foo").strictlyEquals(val));

        QJSValueIterator it(obj);
        QVERIFY(it.hasNext());
        it.next();
        QCOMPARE(it.name(), QString::fromLatin1("foo"));
        QCOMPARE(it.flags(), QScriptValue::PropertyFlags(QScriptValue::PropertyGetter | QScriptValue::PropertySetter));
        QVERIFY(it.value().strictlyEquals(val));
        QJSValue val2(&eng, 456);
        it.setValue(val2);
        QVERIFY(obj.property("bar").strictlyEquals(val2));
        QVERIFY(obj.property("foo").strictlyEquals(val2));

        QVERIFY(it.hasNext());
        it.next();
        QCOMPARE(it.name(), QString::fromLatin1("bar"));
        QVERIFY(!it.hasNext());

        QVERIFY(it.hasPrevious());
        it.previous();
        QCOMPARE(it.name(), QString::fromLatin1("bar"));
        QVERIFY(it.hasPrevious());
        it.previous();
        QCOMPARE(it.name(), QString::fromLatin1("foo"));
        QCOMPARE(it.flags(), QScriptValue::PropertyFlags(QScriptValue::PropertyGetter | QScriptValue::PropertySetter));
        QVERIFY(it.value().strictlyEquals(val2));
        it.setValue(val);
        QVERIFY(obj.property("bar").strictlyEquals(val));
        QVERIFY(obj.property("foo").strictlyEquals(val));
    }
    // separate getter/setter function
    for (int x = 0; x < 2; ++x) {
        QJSEngine eng;
        QJSValue obj = eng.newObject();
        if (x == 0) {
            obj.setProperty("foo", eng.newFunction(myGetter), QScriptValue::PropertyGetter);
            obj.setProperty("foo", eng.newFunction(mySetter), QScriptValue::PropertySetter);
        } else {
            obj.setProperty("foo", eng.newFunction(mySetter), QScriptValue::PropertySetter);
            obj.setProperty("foo", eng.newFunction(myGetter), QScriptValue::PropertyGetter);
        }
        QJSValue val(&eng, 123);
        obj.setProperty("foo", val);
        QVERIFY(obj.property("bar").strictlyEquals(val));
        QVERIFY(obj.property("foo").strictlyEquals(val));

        QJSValueIterator it(obj);
        QVERIFY(it.hasNext());
        it.next();
        QCOMPARE(it.name(), QString::fromLatin1("foo"));
        QVERIFY(it.value().strictlyEquals(val));
        QJSValue val2(&eng, 456);
        it.setValue(val2);
        QVERIFY(obj.property("bar").strictlyEquals(val2));
        QVERIFY(obj.property("foo").strictlyEquals(val2));

        QVERIFY(it.hasNext());
        it.next();
        QCOMPARE(it.name(), QString::fromLatin1("bar"));
        QVERIFY(!it.hasNext());

        QVERIFY(it.hasPrevious());
        it.previous();
        QCOMPARE(it.name(), QString::fromLatin1("bar"));
        QVERIFY(it.hasPrevious());
        it.previous();
        QCOMPARE(it.name(), QString::fromLatin1("foo"));
        QVERIFY(it.value().strictlyEquals(val2));
        it.setValue(val);
        QVERIFY(obj.property("bar").strictlyEquals(val));
        QVERIFY(obj.property("foo").strictlyEquals(val));
    }
}
#endif

void tst_QJSValueIterator::assignObjectToIterator()
{
    QJSEngine eng;
    QJSValue obj1 = eng.newObject();
    obj1.setProperty("foo", 123);
    QJSValue obj2 = eng.newObject();
    obj2.setProperty("bar", 456);

    QJSValueIterator it(obj1);
    QVERIFY(it.hasNext());
    it.next();
    it = obj2;
    QVERIFY(it.hasNext());
    it.next();
    QCOMPARE(it.name(), QString::fromLatin1("bar"));

    it = obj1;
    QVERIFY(it.hasNext());
    it.next();
    QCOMPARE(it.name(), QString::fromLatin1("foo"));

    it = obj2;
    QVERIFY(it.hasNext());
    it.next();
    QCOMPARE(it.name(), QString::fromLatin1("bar"));

    it = obj2;
    QVERIFY(it.hasNext());
    it.next();
    QCOMPARE(it.name(), QString::fromLatin1("bar"));
}

void tst_QJSValueIterator::iterateNonObject()
{
    QJSValueIterator it(123);
    QVERIFY(!it.hasNext());
    it.next();
    it.name();
    it.value();
    QJSValue num(5);
    it = num;
    QVERIFY(!it.hasNext());
}

void tst_QJSValueIterator::iterateOverObjectFromDeletedEngine()
{
    QJSEngine *engine = new QJSEngine;
    QJSValue objet = engine->newObject();

    // populate object with properties
    QHash<QString, int> properties;
    properties.insert("foo",1235);
    properties.insert("oof",5321);
    properties.insert("ofo",3521);
    QHash<QString, int>::const_iterator i = properties.constBegin();
    for (; i != properties.constEnd(); ++i) {
        objet.setProperty(i.key(), i.value());
    }

    // start iterating
    QJSValueIterator it(objet);
    it.next();
    QVERIFY(properties.contains(it.name()));

    delete engine;

    QVERIFY(objet.isUndefined());
    QVERIFY(it.name().isEmpty());
    QVERIFY(it.value().isUndefined());

    QVERIFY(!it.hasNext());
    it.next();

    QVERIFY(it.name().isEmpty());
    QVERIFY(it.value().isUndefined());

}

void tst_QJSValueIterator::iterateWithNext()
{
    QJSEngine engine;
    QJSValue value = engine.newObject();
    value.setProperty("one", 1);
    value.setProperty("two", 2);
    value.setProperty("three", 3);

    QStringList list;
    list << QStringLiteral("one") << QStringLiteral("three") << QStringLiteral("two");

    int counter = 0;
    QJSValueIterator it(value);
    QStringList actualList;
    while (it.next()) {
        ++counter;
        actualList << it.name();
    }

    std::sort(actualList.begin(), actualList.end());

    QCOMPARE(counter, 3);
    QCOMPARE(list, actualList);

}

QTEST_MAIN(tst_QJSValueIterator)
#include "tst_qjsvalueiterator.moc"
