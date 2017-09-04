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
#include <QQmlEngine>
#include <QQmlComponent>
#include <QQmlContext>
#include <QDebug>
#include <QScopedPointer>
#include <private/qqmlglobal_p.h>
#include <private/qquickvaluetypes_p.h>
#include "../../shared/util.h"
#include "testtypes.h"

QT_BEGIN_NAMESPACE
extern int qt_defaultDpi(void);
QT_END_NAMESPACE

// There is some overlap between the qqmllanguage and qqmlvaluetypes
// test here, but it needs to be separate to ensure that no QML plugins
// are loaded prior to these tests, which could contaminate the type
// system with more providers.

class tst_qqmlvaluetypeproviders : public QQmlDataTest
{
    Q_OBJECT
public:
    tst_qqmlvaluetypeproviders() {}

private slots:
    void initTestCase();

    void qtqmlValueTypes();   // This test function _must_ be the first test function run.
    void qtquickValueTypes();
    void comparisonSemantics();
    void cppIntegration();
    void jsObjectConversion();
    void invokableFunctions();
    void userType();
    void changedSignal();
};

void tst_qqmlvaluetypeproviders::initTestCase()
{
    QQmlDataTest::initTestCase();
    registerTypes();
}

void tst_qqmlvaluetypeproviders::qtqmlValueTypes()
{
    QQmlEngine e;
    QQmlComponent component(&e, testFileUrl("qtqmlValueTypes.qml"));
    QVERIFY(!component.isError());
    QVERIFY(component.errors().isEmpty());
    QObject *object = component.create();
    QVERIFY(object != 0);
    QVERIFY(object->property("qtqmlTypeSuccess").toBool());
    QVERIFY(object->property("qtquickTypeSuccess").toBool());
    delete object;
}

void tst_qqmlvaluetypeproviders::qtquickValueTypes()
{
    QQmlEngine e;
    QQmlComponent component(&e, testFileUrl("qtquickValueTypes.qml"));
    QVERIFY(!component.isError());
    QVERIFY(component.errors().isEmpty());
    QObject *object = component.create();
    QVERIFY(object != 0);
    QVERIFY(object->property("qtqmlTypeSuccess").toBool());
    QVERIFY(object->property("qtquickTypeSuccess").toBool());
    delete object;
}

void tst_qqmlvaluetypeproviders::comparisonSemantics()
{
    QQmlEngine e;
    QQmlComponent component(&e, testFileUrl("comparisonSemantics.qml"));
    QVERIFY(!component.isError());
    QVERIFY(component.errors().isEmpty());
    QObject *object = component.create();
    QVERIFY(object != 0);
    QVERIFY(object->property("comparisonSuccess").toBool());
    delete object;
}

void tst_qqmlvaluetypeproviders::cppIntegration()
{
    QQmlEngine e;
    QQmlComponent component(&e, testFileUrl("cppIntegration.qml"));
    QVERIFY(!component.isError());
    QVERIFY(component.errors().isEmpty());
    QObject *object = component.create();
    QVERIFY(object != 0);

    // ensure accessing / comparing / assigning cpp-defined props
    // and qml-defined props works in QML.
    QVERIFY(object->property("success").toBool());

    // ensure types match
    QCOMPARE(object->property("g").userType(), object->property("rectf").userType());
    QCOMPARE(object->property("p").userType(), object->property("pointf").userType());
    QCOMPARE(object->property("z").userType(), object->property("sizef").userType());
    QCOMPARE(object->property("v2").userType(), object->property("vector2").userType());
    QCOMPARE(object->property("v3").userType(), object->property("vector").userType());
    QCOMPARE(object->property("v4").userType(), object->property("vector4").userType());
    QCOMPARE(object->property("q").userType(), object->property("quaternion").userType());
    QCOMPARE(object->property("m").userType(), object->property("matrix").userType());
    QCOMPARE(object->property("c").userType(), object->property("color").userType());
    QCOMPARE(object->property("f").userType(), object->property("font").userType());

    // ensure values match
    QCOMPARE(object->property("g").value<QRectF>(), object->property("rectf").value<QRectF>());
    QCOMPARE(object->property("p").value<QPointF>(), object->property("pointf").value<QPointF>());
    QCOMPARE(object->property("z").value<QSizeF>(), object->property("sizef").value<QSizeF>());
    QCOMPARE(object->property("v2").value<QVector2D>(), object->property("vector2").value<QVector2D>());
    QCOMPARE(object->property("v3").value<QVector3D>(), object->property("vector").value<QVector3D>());
    QCOMPARE(object->property("v4").value<QVector4D>(), object->property("vector4").value<QVector4D>());
    QCOMPARE(object->property("q").value<QQuaternion>(), object->property("quaternion").value<QQuaternion>());
    QCOMPARE(object->property("m").value<QMatrix4x4>(), object->property("matrix").value<QMatrix4x4>());
    QCOMPARE(object->property("c").value<QColor>(), object->property("color").value<QColor>());
    QCOMPARE(object->property("f").value<QFont>(), object->property("font").value<QFont>());

    delete object;
}

void tst_qqmlvaluetypeproviders::jsObjectConversion()
{
    QQmlEngine e;
    QQmlComponent component(&e, testFileUrl("jsObjectConversion.qml"));
    QVERIFY(!component.isError());
    QVERIFY(component.errors().isEmpty());
    QObject *object = component.create();
    QVERIFY(object != 0);
    QVERIFY(object->property("qtquickTypeSuccess").toBool());
    delete object;
}

void tst_qqmlvaluetypeproviders::invokableFunctions()
{
    QQmlEngine e;
    QQmlComponent component(&e, testFileUrl("invokableFunctions.qml"));
    QVERIFY(!component.isError());
    QVERIFY(component.errors().isEmpty());
    QObject *object = component.create();
    QVERIFY(object != 0);
    QVERIFY(object->property("complete").toBool());
    QVERIFY(object->property("success").toBool());
    delete object;
}

namespace {

// A value-type class to export to QML
class TestValue
{
public:
    TestValue() : m_p1(0), m_p2(0.0) {}
    TestValue(int p1, double p2) : m_p1(p1), m_p2(p2) {}
    TestValue(const TestValue &other) : m_p1(other.m_p1), m_p2(other.m_p2) {}
    ~TestValue() {}

    TestValue &operator=(const TestValue &other) { m_p1 = other.m_p1; m_p2 = other.m_p2; return *this; }

    int property1() const { return m_p1; }
    void setProperty1(int p1) { m_p1 = p1; }

    double property2() const { return m_p2; }
    void setProperty2(double p2) { m_p2 = p2; }

    bool operator==(const TestValue &other) const { return (m_p1 == other.m_p1) && (m_p2 == other.m_p2); }
    bool operator!=(const TestValue &other) const { return !operator==(other); }

    bool operator<(const TestValue &other) const { if (m_p1 < other.m_p1) return true; return m_p2 < other.m_p2; }

private:
    int m_p1;
    double m_p2;
};

}

Q_DECLARE_METATYPE(TestValue);

namespace {

class TestValueType
{
    TestValue v;
    Q_GADGET
    Q_PROPERTY(int property1 READ property1 WRITE setProperty1)
    Q_PROPERTY(double property2 READ property2 WRITE setProperty2)
public:
    Q_INVOKABLE QString toString() const { return QString::number(property1()) + QLatin1Char(',') + QString::number(property2()); }

    int property1() const { return v.property1(); }
    void setProperty1(int p1) { v.setProperty1(p1); }

    double property2() const { return v.property2(); }
    void setProperty2(double p2) { v.setProperty2(p2); }
};

class TestValueTypeProvider : public QQmlValueTypeProvider
{
public:
    const QMetaObject *getMetaObjectForMetaType(int type)
    {
        if (type == qMetaTypeId<TestValue>())
            return &TestValueType::staticMetaObject;

        return 0;
    }

};

TestValueTypeProvider *getValueTypeProvider()
{
    static TestValueTypeProvider valueTypeProvider;
    return &valueTypeProvider;
}

bool initializeProviders()
{
    QQml_addValueTypeProvider(getValueTypeProvider());
    return true;
}

const bool initialized = initializeProviders();

class TestValueExporter : public QObject
{
    Q_OBJECT
    Q_PROPERTY(TestValue testValue READ testValue WRITE setTestValue)
public:
    TestValue testValue() const { return m_testValue; }
    void setTestValue(const TestValue &v) { m_testValue = v; }

    Q_INVOKABLE TestValue getTestValue() const { return TestValue(333, 666.999); }

private:
    TestValue m_testValue;
};

}

void tst_qqmlvaluetypeproviders::userType()
{
    Q_ASSERT(initialized);
    Q_ASSERT(qMetaTypeId<TestValue>() >= QMetaType::User);

    qRegisterMetaType<TestValue>();
    QMetaType::registerComparators<TestValue>();
    qmlRegisterType<TestValueExporter>("Test", 1, 0, "TestValueExporter");

    TestValueExporter exporter;

    QQmlEngine e;
    e.rootContext()->setContextProperty("testValueExporter", &exporter);

    QQmlComponent component(&e, testFileUrl("userType.qml"));
    QScopedPointer<QObject> obj(component.create());
    QVERIFY(obj != 0);
    QCOMPARE(obj->property("success").toBool(), true);
}

void tst_qqmlvaluetypeproviders::changedSignal()
{
    QQmlEngine e;
    QQmlComponent component(&e, testFileUrl("changedSignal.qml"));
    QVERIFY(!component.isError());
    QVERIFY(component.errors().isEmpty());
    QScopedPointer<QObject> object(component.create());
    QVERIFY(object != 0);
    QVERIFY(object->property("complete").toBool());
    QVERIFY(object->property("success").toBool());
}

QTEST_MAIN(tst_qqmlvaluetypeproviders)

#include "tst_qqmlvaluetypeproviders.moc"
