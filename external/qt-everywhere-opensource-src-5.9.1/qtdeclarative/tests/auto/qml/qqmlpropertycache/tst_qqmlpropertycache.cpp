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

#include <qtest.h>
#include <private/qqmlpropertycache_p.h>
#include <QtQml/qqmlengine.h>
#include <private/qv8engine_p.h>
#include <private/qmetaobjectbuilder_p.h>
#include <QCryptographicHash>
#include "../../shared/util.h"

class tst_qqmlpropertycache : public QObject
{
    Q_OBJECT
public:
    tst_qqmlpropertycache() {}

private slots:
    void properties();
    void propertiesDerived();
    void methods();
    void methodsDerived();
    void signalHandlers();
    void signalHandlersDerived();
    void metaObjectSize_data();
    void metaObjectSize();
    void metaObjectChecksum();

private:
    QQmlEngine engine;
};

class BaseObject : public QObject
{
    Q_OBJECT
    Q_PROPERTY(int propertyA READ propertyA NOTIFY propertyAChanged)
    Q_PROPERTY(QString propertyB READ propertyB NOTIFY propertyBChanged)
public:
    BaseObject(QObject *parent = 0) : QObject(parent) {}

    int propertyA() const { return 0; }
    QString propertyB() const { return QString(); }

public Q_SLOTS:
    void slotA() {}

Q_SIGNALS:
    void propertyAChanged();
    void propertyBChanged();
    void signalA();
};

class DerivedObject : public BaseObject
{
    Q_OBJECT
    Q_PROPERTY(int propertyC READ propertyC NOTIFY propertyCChanged)
    Q_PROPERTY(QString propertyD READ propertyD NOTIFY propertyDChanged)
public:
    DerivedObject(QObject *parent = 0) : BaseObject(parent) {}

    int propertyC() const { return 0; }
    QString propertyD() const { return QString(); }

public Q_SLOTS:
    void slotB() {}

Q_SIGNALS:
    void propertyCChanged();
    void propertyDChanged();
    void signalB();
};

QQmlPropertyData *cacheProperty(QQmlPropertyCache *cache, const char *name)
{
    return cache->property(QLatin1String(name), 0, 0);
}

void tst_qqmlpropertycache::properties()
{
    QQmlEngine engine;
    QV4::ExecutionEngine *v4 = QV8Engine::getV4(&engine);
    DerivedObject object;
    const QMetaObject *metaObject = object.metaObject();

    QQmlRefPointer<QQmlPropertyCache> cache(new QQmlPropertyCache(v4, metaObject));
    QQmlPropertyData *data;

    QVERIFY((data = cacheProperty(cache, "propertyA")));
    QCOMPARE(data->coreIndex(), metaObject->indexOfProperty("propertyA"));

    QVERIFY((data = cacheProperty(cache, "propertyB")));
    QCOMPARE(data->coreIndex(), metaObject->indexOfProperty("propertyB"));

    QVERIFY((data = cacheProperty(cache, "propertyC")));
    QCOMPARE(data->coreIndex(), metaObject->indexOfProperty("propertyC"));

    QVERIFY((data = cacheProperty(cache, "propertyD")));
    QCOMPARE(data->coreIndex(), metaObject->indexOfProperty("propertyD"));
}

void tst_qqmlpropertycache::propertiesDerived()
{
    QQmlEngine engine;
    QV4::ExecutionEngine *v4 = QV8Engine::getV4(&engine);
    DerivedObject object;
    const QMetaObject *metaObject = object.metaObject();

    QQmlRefPointer<QQmlPropertyCache> parentCache(new QQmlPropertyCache(v4, &BaseObject::staticMetaObject));
    QQmlRefPointer<QQmlPropertyCache> cache(parentCache->copyAndAppend(object.metaObject()));
    QQmlPropertyData *data;

    QVERIFY((data = cacheProperty(cache, "propertyA")));
    QCOMPARE(data->coreIndex(), metaObject->indexOfProperty("propertyA"));

    QVERIFY((data = cacheProperty(cache, "propertyB")));
    QCOMPARE(data->coreIndex(), metaObject->indexOfProperty("propertyB"));

    QVERIFY((data = cacheProperty(cache, "propertyC")));
    QCOMPARE(data->coreIndex(), metaObject->indexOfProperty("propertyC"));

    QVERIFY((data = cacheProperty(cache, "propertyD")));
    QCOMPARE(data->coreIndex(), metaObject->indexOfProperty("propertyD"));
}

void tst_qqmlpropertycache::methods()
{
    QQmlEngine engine;
    QV4::ExecutionEngine *v4 = QV8Engine::getV4(&engine);
    DerivedObject object;
    const QMetaObject *metaObject = object.metaObject();

    QQmlRefPointer<QQmlPropertyCache> cache(new QQmlPropertyCache(v4, metaObject));
    QQmlPropertyData *data;

    QVERIFY((data = cacheProperty(cache, "slotA")));
    QCOMPARE(data->coreIndex(), metaObject->indexOfMethod("slotA()"));

    QVERIFY((data = cacheProperty(cache, "slotB")));
    QCOMPARE(data->coreIndex(), metaObject->indexOfMethod("slotB()"));

    QVERIFY((data = cacheProperty(cache, "signalA")));
    QCOMPARE(data->coreIndex(), metaObject->indexOfMethod("signalA()"));

    QVERIFY((data = cacheProperty(cache, "signalB")));
    QCOMPARE(data->coreIndex(), metaObject->indexOfMethod("signalB()"));

    QVERIFY((data = cacheProperty(cache, "propertyAChanged")));
    QCOMPARE(data->coreIndex(), metaObject->indexOfMethod("propertyAChanged()"));

    QVERIFY((data = cacheProperty(cache, "propertyBChanged")));
    QCOMPARE(data->coreIndex(), metaObject->indexOfMethod("propertyBChanged()"));

    QVERIFY((data = cacheProperty(cache, "propertyCChanged")));
    QCOMPARE(data->coreIndex(), metaObject->indexOfMethod("propertyCChanged()"));

    QVERIFY((data = cacheProperty(cache, "propertyDChanged")));
    QCOMPARE(data->coreIndex(), metaObject->indexOfMethod("propertyDChanged()"));
}

void tst_qqmlpropertycache::methodsDerived()
{
    QQmlEngine engine;
    QV4::ExecutionEngine *v4 = QV8Engine::getV4(&engine);
    DerivedObject object;
    const QMetaObject *metaObject = object.metaObject();

    QQmlRefPointer<QQmlPropertyCache> parentCache(new QQmlPropertyCache(v4, &BaseObject::staticMetaObject));
    QQmlRefPointer<QQmlPropertyCache> cache(parentCache->copyAndAppend(object.metaObject()));
    QQmlPropertyData *data;

    QVERIFY((data = cacheProperty(cache, "slotA")));
    QCOMPARE(data->coreIndex(), metaObject->indexOfMethod("slotA()"));

    QVERIFY((data = cacheProperty(cache, "slotB")));
    QCOMPARE(data->coreIndex(), metaObject->indexOfMethod("slotB()"));

    QVERIFY((data = cacheProperty(cache, "signalA")));
    QCOMPARE(data->coreIndex(), metaObject->indexOfMethod("signalA()"));

    QVERIFY((data = cacheProperty(cache, "signalB")));
    QCOMPARE(data->coreIndex(), metaObject->indexOfMethod("signalB()"));

    QVERIFY((data = cacheProperty(cache, "propertyAChanged")));
    QCOMPARE(data->coreIndex(), metaObject->indexOfMethod("propertyAChanged()"));

    QVERIFY((data = cacheProperty(cache, "propertyBChanged")));
    QCOMPARE(data->coreIndex(), metaObject->indexOfMethod("propertyBChanged()"));

    QVERIFY((data = cacheProperty(cache, "propertyCChanged")));
    QCOMPARE(data->coreIndex(), metaObject->indexOfMethod("propertyCChanged()"));

    QVERIFY((data = cacheProperty(cache, "propertyDChanged")));
    QCOMPARE(data->coreIndex(), metaObject->indexOfMethod("propertyDChanged()"));
}

void tst_qqmlpropertycache::signalHandlers()
{
    QQmlEngine engine;
    QV4::ExecutionEngine *v4 = QV8Engine::getV4(&engine);
    DerivedObject object;
    const QMetaObject *metaObject = object.metaObject();

    QQmlRefPointer<QQmlPropertyCache> cache(new QQmlPropertyCache(v4, metaObject));
    QQmlPropertyData *data;

    QVERIFY((data = cacheProperty(cache, "onSignalA")));
    QCOMPARE(data->coreIndex(), metaObject->indexOfMethod("signalA()"));

    QVERIFY((data = cacheProperty(cache, "onSignalB")));
    QCOMPARE(data->coreIndex(), metaObject->indexOfMethod("signalB()"));

    QVERIFY((data = cacheProperty(cache, "onPropertyAChanged")));
    QCOMPARE(data->coreIndex(), metaObject->indexOfMethod("propertyAChanged()"));

    QVERIFY((data = cacheProperty(cache, "onPropertyBChanged")));
    QCOMPARE(data->coreIndex(), metaObject->indexOfMethod("propertyBChanged()"));

    QVERIFY((data = cacheProperty(cache, "onPropertyCChanged")));
    QCOMPARE(data->coreIndex(), metaObject->indexOfMethod("propertyCChanged()"));

    QVERIFY((data = cacheProperty(cache, "onPropertyDChanged")));
    QCOMPARE(data->coreIndex(), metaObject->indexOfMethod("propertyDChanged()"));
}

void tst_qqmlpropertycache::signalHandlersDerived()
{
    QQmlEngine engine;
    QV4::ExecutionEngine *v4 = QV8Engine::getV4(&engine);
    DerivedObject object;
    const QMetaObject *metaObject = object.metaObject();

    QQmlRefPointer<QQmlPropertyCache> parentCache(new QQmlPropertyCache(v4, &BaseObject::staticMetaObject));
    QQmlRefPointer<QQmlPropertyCache> cache(parentCache->copyAndAppend(object.metaObject()));
    QQmlPropertyData *data;

    QVERIFY((data = cacheProperty(cache, "onSignalA")));
    QCOMPARE(data->coreIndex(), metaObject->indexOfMethod("signalA()"));

    QVERIFY((data = cacheProperty(cache, "onSignalB")));
    QCOMPARE(data->coreIndex(), metaObject->indexOfMethod("signalB()"));

    QVERIFY((data = cacheProperty(cache, "onPropertyAChanged")));
    QCOMPARE(data->coreIndex(), metaObject->indexOfMethod("propertyAChanged()"));

    QVERIFY((data = cacheProperty(cache, "onPropertyBChanged")));
    QCOMPARE(data->coreIndex(), metaObject->indexOfMethod("propertyBChanged()"));

    QVERIFY((data = cacheProperty(cache, "onPropertyCChanged")));
    QCOMPARE(data->coreIndex(), metaObject->indexOfMethod("propertyCChanged()"));

    QVERIFY((data = cacheProperty(cache, "onPropertyDChanged")));
    QCOMPARE(data->coreIndex(), metaObject->indexOfMethod("propertyDChanged()"));
}

class TestClass : public QObject
{
    Q_OBJECT
    Q_PROPERTY(int prop READ prop WRITE setProp NOTIFY propChanged)
    int m_prop;

public:
    enum MyEnum {
        First, Second
    };
    Q_ENUM(MyEnum)

    Q_CLASSINFO("Foo", "Bar")

    TestClass() {}

    int prop() const
    {
        return m_prop;
    }

public slots:
    void setProp(int prop)
    {
        if (m_prop == prop)
            return;

        m_prop = prop;
        emit propChanged(prop);
    }
signals:
    void propChanged(int prop);
};

class TestClassWithParameters : public QObject
{
    Q_OBJECT

public:
    Q_INVOKABLE void slotWithArguments(int firstArg) {
        Q_UNUSED(firstArg);
    }
};

class TestClassWithClassInfo : public QObject
{
    Q_OBJECT
    Q_CLASSINFO("Key", "Value")
};

#include "tst_qqmlpropertycache.moc"

#define ARRAY_SIZE(arr) \
    int(sizeof(arr) / sizeof(arr[0]))

#define TEST_CLASS(Class) \
    QTest::newRow(#Class) << &Class::staticMetaObject << ARRAY_SIZE(qt_meta_data_##Class) << ARRAY_SIZE(qt_meta_stringdata_##Class.data)

Q_DECLARE_METATYPE(const QMetaObject*);

void tst_qqmlpropertycache::metaObjectSize_data()
{
    QTest::addColumn<const QMetaObject*>("metaObject");
    QTest::addColumn<int>("expectedFieldCount");
    QTest::addColumn<int>("expectedStringCount");

    TEST_CLASS(TestClass);
    TEST_CLASS(TestClassWithParameters);
    TEST_CLASS(TestClassWithClassInfo);
}

void tst_qqmlpropertycache::metaObjectSize()
{
    QFETCH(const QMetaObject *, metaObject);
    QFETCH(int, expectedFieldCount);
    QFETCH(int, expectedStringCount);

    int size = 0;
    int stringDataSize = 0;
    bool valid = QQmlPropertyCache::determineMetaObjectSizes(*metaObject, &size, &stringDataSize);
    QVERIFY(valid);

    QCOMPARE(size, expectedFieldCount - 1); // Remove trailing zero field until fixed in moc.
    QCOMPARE(stringDataSize, expectedStringCount);
}

void tst_qqmlpropertycache::metaObjectChecksum()
{
    QMetaObjectBuilder builder;
    builder.setClassName("Test");
    builder.addClassInfo("foo", "bar");

    QCryptographicHash hash(QCryptographicHash::Md5);

    QScopedPointer<QMetaObject, QScopedPointerPodDeleter> mo(builder.toMetaObject());
    QVERIFY(!mo.isNull());

    QVERIFY(QQmlPropertyCache::addToHash(hash, *mo.data()));
    QByteArray initialHash = hash.result();
    QVERIFY(!initialHash.isEmpty());
    hash.reset();

    {
        QVERIFY(QQmlPropertyCache::addToHash(hash, *mo.data()));
        QByteArray nextHash = hash.result();
        QVERIFY(!nextHash.isEmpty());
        hash.reset();
        QCOMPARE(initialHash, nextHash);
    }

    builder.addProperty("testProperty", "int", -1);

    mo.reset(builder.toMetaObject());
    {
        QVERIFY(QQmlPropertyCache::addToHash(hash, *mo.data()));
        QByteArray nextHash = hash.result();
        QVERIFY(!nextHash.isEmpty());
        hash.reset();
        QVERIFY(initialHash != nextHash);
    }
}

QTEST_MAIN(tst_qqmlpropertycache)
