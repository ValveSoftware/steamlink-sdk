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
#include <private/qqmlpropertycache_p.h>
#include <QtQml/qqmlengine.h>
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
    DerivedObject object;
    const QMetaObject *metaObject = object.metaObject();

    QQmlRefPointer<QQmlPropertyCache> cache(new QQmlPropertyCache(&engine, metaObject));
    QQmlPropertyData *data;

    QVERIFY(data = cacheProperty(cache, "propertyA"));
    QCOMPARE(data->coreIndex, metaObject->indexOfProperty("propertyA"));

    QVERIFY(data = cacheProperty(cache, "propertyB"));
    QCOMPARE(data->coreIndex, metaObject->indexOfProperty("propertyB"));

    QVERIFY(data = cacheProperty(cache, "propertyC"));
    QCOMPARE(data->coreIndex, metaObject->indexOfProperty("propertyC"));

    QVERIFY(data = cacheProperty(cache, "propertyD"));
    QCOMPARE(data->coreIndex, metaObject->indexOfProperty("propertyD"));
}

void tst_qqmlpropertycache::propertiesDerived()
{
    QQmlEngine engine;
    DerivedObject object;
    const QMetaObject *metaObject = object.metaObject();

    QQmlRefPointer<QQmlPropertyCache> parentCache(new QQmlPropertyCache(&engine, &BaseObject::staticMetaObject));
    QQmlRefPointer<QQmlPropertyCache> cache(parentCache->copyAndAppend(&engine, object.metaObject()));
    QQmlPropertyData *data;

    QVERIFY(data = cacheProperty(cache, "propertyA"));
    QCOMPARE(data->coreIndex, metaObject->indexOfProperty("propertyA"));

    QVERIFY(data = cacheProperty(cache, "propertyB"));
    QCOMPARE(data->coreIndex, metaObject->indexOfProperty("propertyB"));

    QVERIFY(data = cacheProperty(cache, "propertyC"));
    QCOMPARE(data->coreIndex, metaObject->indexOfProperty("propertyC"));

    QVERIFY(data = cacheProperty(cache, "propertyD"));
    QCOMPARE(data->coreIndex, metaObject->indexOfProperty("propertyD"));
}

void tst_qqmlpropertycache::methods()
{
    QQmlEngine engine;
    DerivedObject object;
    const QMetaObject *metaObject = object.metaObject();

    QQmlRefPointer<QQmlPropertyCache> cache(new QQmlPropertyCache(&engine, metaObject));
    QQmlPropertyData *data;

    QVERIFY(data = cacheProperty(cache, "slotA"));
    QCOMPARE(data->coreIndex, metaObject->indexOfMethod("slotA()"));

    QVERIFY(data = cacheProperty(cache, "slotB"));
    QCOMPARE(data->coreIndex, metaObject->indexOfMethod("slotB()"));

    QVERIFY(data = cacheProperty(cache, "signalA"));
    QCOMPARE(data->coreIndex, metaObject->indexOfMethod("signalA()"));

    QVERIFY(data = cacheProperty(cache, "signalB"));
    QCOMPARE(data->coreIndex, metaObject->indexOfMethod("signalB()"));

    QVERIFY(data = cacheProperty(cache, "propertyAChanged"));
    QCOMPARE(data->coreIndex, metaObject->indexOfMethod("propertyAChanged()"));

    QVERIFY(data = cacheProperty(cache, "propertyBChanged"));
    QCOMPARE(data->coreIndex, metaObject->indexOfMethod("propertyBChanged()"));

    QVERIFY(data = cacheProperty(cache, "propertyCChanged"));
    QCOMPARE(data->coreIndex, metaObject->indexOfMethod("propertyCChanged()"));

    QVERIFY(data = cacheProperty(cache, "propertyDChanged"));
    QCOMPARE(data->coreIndex, metaObject->indexOfMethod("propertyDChanged()"));
}

void tst_qqmlpropertycache::methodsDerived()
{
    QQmlEngine engine;
    DerivedObject object;
    const QMetaObject *metaObject = object.metaObject();

    QQmlRefPointer<QQmlPropertyCache> parentCache(new QQmlPropertyCache(&engine, &BaseObject::staticMetaObject));
    QQmlRefPointer<QQmlPropertyCache> cache(parentCache->copyAndAppend(&engine, object.metaObject()));
    QQmlPropertyData *data;

    QVERIFY(data = cacheProperty(cache, "slotA"));
    QCOMPARE(data->coreIndex, metaObject->indexOfMethod("slotA()"));

    QVERIFY(data = cacheProperty(cache, "slotB"));
    QCOMPARE(data->coreIndex, metaObject->indexOfMethod("slotB()"));

    QVERIFY(data = cacheProperty(cache, "signalA"));
    QCOMPARE(data->coreIndex, metaObject->indexOfMethod("signalA()"));

    QVERIFY(data = cacheProperty(cache, "signalB"));
    QCOMPARE(data->coreIndex, metaObject->indexOfMethod("signalB()"));

    QVERIFY(data = cacheProperty(cache, "propertyAChanged"));
    QCOMPARE(data->coreIndex, metaObject->indexOfMethod("propertyAChanged()"));

    QVERIFY(data = cacheProperty(cache, "propertyBChanged"));
    QCOMPARE(data->coreIndex, metaObject->indexOfMethod("propertyBChanged()"));

    QVERIFY(data = cacheProperty(cache, "propertyCChanged"));
    QCOMPARE(data->coreIndex, metaObject->indexOfMethod("propertyCChanged()"));

    QVERIFY(data = cacheProperty(cache, "propertyDChanged"));
    QCOMPARE(data->coreIndex, metaObject->indexOfMethod("propertyDChanged()"));
}

void tst_qqmlpropertycache::signalHandlers()
{
    QQmlEngine engine;
    DerivedObject object;
    const QMetaObject *metaObject = object.metaObject();

    QQmlRefPointer<QQmlPropertyCache> cache(new QQmlPropertyCache(&engine, metaObject));
    QQmlPropertyData *data;

    QVERIFY(data = cacheProperty(cache, "onSignalA"));
    QCOMPARE(data->coreIndex, metaObject->indexOfMethod("signalA()"));

    QVERIFY(data = cacheProperty(cache, "onSignalB"));
    QCOMPARE(data->coreIndex, metaObject->indexOfMethod("signalB()"));

    QVERIFY(data = cacheProperty(cache, "onPropertyAChanged"));
    QCOMPARE(data->coreIndex, metaObject->indexOfMethod("propertyAChanged()"));

    QVERIFY(data = cacheProperty(cache, "onPropertyBChanged"));
    QCOMPARE(data->coreIndex, metaObject->indexOfMethod("propertyBChanged()"));

    QVERIFY(data = cacheProperty(cache, "onPropertyCChanged"));
    QCOMPARE(data->coreIndex, metaObject->indexOfMethod("propertyCChanged()"));

    QVERIFY(data = cacheProperty(cache, "onPropertyDChanged"));
    QCOMPARE(data->coreIndex, metaObject->indexOfMethod("propertyDChanged()"));
}

void tst_qqmlpropertycache::signalHandlersDerived()
{
    QQmlEngine engine;
    DerivedObject object;
    const QMetaObject *metaObject = object.metaObject();

    QQmlRefPointer<QQmlPropertyCache> parentCache(new QQmlPropertyCache(&engine, &BaseObject::staticMetaObject));
    QQmlRefPointer<QQmlPropertyCache> cache(parentCache->copyAndAppend(&engine, object.metaObject()));
    QQmlPropertyData *data;

    QVERIFY(data = cacheProperty(cache, "onSignalA"));
    QCOMPARE(data->coreIndex, metaObject->indexOfMethod("signalA()"));

    QVERIFY(data = cacheProperty(cache, "onSignalB"));
    QCOMPARE(data->coreIndex, metaObject->indexOfMethod("signalB()"));

    QVERIFY(data = cacheProperty(cache, "onPropertyAChanged"));
    QCOMPARE(data->coreIndex, metaObject->indexOfMethod("propertyAChanged()"));

    QVERIFY(data = cacheProperty(cache, "onPropertyBChanged"));
    QCOMPARE(data->coreIndex, metaObject->indexOfMethod("propertyBChanged()"));

    QVERIFY(data = cacheProperty(cache, "onPropertyCChanged"));
    QCOMPARE(data->coreIndex, metaObject->indexOfMethod("propertyCChanged()"));

    QVERIFY(data = cacheProperty(cache, "onPropertyDChanged"));
    QCOMPARE(data->coreIndex, metaObject->indexOfMethod("propertyDChanged()"));
}

QTEST_MAIN(tst_qqmlpropertycache)

#include "tst_qqmlpropertycache.moc"
