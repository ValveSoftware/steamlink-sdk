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
#include <qqml.h>
#include <qqmlprivate.h>
#include <qqmlengine.h>
#include <qqmlcomponent.h>

#include <private/qqmlmetatype_p.h>
#include <private/qqmlpropertyvalueinterceptor_p.h>
#include <private/qhashedstring_p.h>
#include "../../shared/util.h"

class tst_qqmlmetatype : public QQmlDataTest
{
    Q_OBJECT
public:
    tst_qqmlmetatype() {}

private slots:
    void initTestCase();

    void qmlParserStatusCast();
    void qmlPropertyValueSourceCast();
    void qmlPropertyValueInterceptorCast();
    void qmlType();
    void invalidQmlTypeName();
    void registrationType();
    void compositeType();

    void isList();

    void defaultObject();
};

class TestType : public QObject
{
    Q_OBJECT
    Q_PROPERTY(int foo READ foo)

    Q_CLASSINFO("DefaultProperty", "foo")
public:
    int foo() { return 0; }
};
QML_DECLARE_TYPE(TestType);

QObject *testTypeProvider(QQmlEngine *engine, QJSEngine *scriptEngine)
{
    Q_UNUSED(engine);
    Q_UNUSED(scriptEngine);
    return new TestType();
}

class ParserStatusTestType : public QObject, public QQmlParserStatus
{
    Q_OBJECT
    void classBegin(){}
    void componentComplete(){}
    Q_CLASSINFO("DefaultProperty", "foo") // Missing default property
    Q_INTERFACES(QQmlParserStatus)
};
QML_DECLARE_TYPE(ParserStatusTestType);

class ValueSourceTestType : public QObject, public QQmlPropertyValueSource
{
    Q_OBJECT
    Q_INTERFACES(QQmlPropertyValueSource)
public:
    virtual void setTarget(const QQmlProperty &) {}
};
QML_DECLARE_TYPE(ValueSourceTestType);

class ValueInterceptorTestType : public QObject, public QQmlPropertyValueInterceptor
{
    Q_OBJECT
    Q_INTERFACES(QQmlPropertyValueInterceptor)
public:
    virtual void setTarget(const QQmlProperty &) {}
    virtual void write(const QVariant &) {}
};
QML_DECLARE_TYPE(ValueInterceptorTestType);

void tst_qqmlmetatype::initTestCase()
{
    QQmlDataTest::initTestCase();
    qmlRegisterType<TestType>("Test", 1, 0, "TestType");
    qmlRegisterSingletonType<TestType>("Test", 1, 0, "TestTypeSingleton", testTypeProvider);
    qmlRegisterType<ParserStatusTestType>("Test", 1, 0, "ParserStatusTestType");
    qmlRegisterType<ValueSourceTestType>("Test", 1, 0, "ValueSourceTestType");
    qmlRegisterType<ValueInterceptorTestType>("Test", 1, 0, "ValueInterceptorTestType");

    QUrl testTypeUrl(testFileUrl("CompositeType.qml"));
    qmlRegisterType(testTypeUrl, "Test", 1, 0, "TestTypeComposite");
}

void tst_qqmlmetatype::qmlParserStatusCast()
{
    QVERIFY(!QQmlMetaType::qmlType(QVariant::Int));
    QVERIFY(QQmlMetaType::qmlType(qMetaTypeId<TestType *>()) != 0);
    QCOMPARE(QQmlMetaType::qmlType(qMetaTypeId<TestType *>())->parserStatusCast(), -1);
    QVERIFY(QQmlMetaType::qmlType(qMetaTypeId<ValueSourceTestType *>()) != 0);
    QCOMPARE(QQmlMetaType::qmlType(qMetaTypeId<ValueSourceTestType *>())->parserStatusCast(), -1);

    QVERIFY(QQmlMetaType::qmlType(qMetaTypeId<ParserStatusTestType *>()) != 0);
    int cast = QQmlMetaType::qmlType(qMetaTypeId<ParserStatusTestType *>())->parserStatusCast();
    QVERIFY(cast != -1);
    QVERIFY(cast != 0);

    ParserStatusTestType t;
    QVERIFY(reinterpret_cast<char *>((QObject *)&t) != reinterpret_cast<char *>((QQmlParserStatus *)&t));

    QQmlParserStatus *status = reinterpret_cast<QQmlParserStatus *>(reinterpret_cast<char *>((QObject *)&t) + cast);
    QCOMPARE(status, (QQmlParserStatus*)&t);
}

void tst_qqmlmetatype::qmlPropertyValueSourceCast()
{
    QVERIFY(!QQmlMetaType::qmlType(QVariant::Int));
    QVERIFY(QQmlMetaType::qmlType(qMetaTypeId<TestType *>()) != 0);
    QCOMPARE(QQmlMetaType::qmlType(qMetaTypeId<TestType *>())->propertyValueSourceCast(), -1);
    QVERIFY(QQmlMetaType::qmlType(qMetaTypeId<ParserStatusTestType *>()) != 0);
    QCOMPARE(QQmlMetaType::qmlType(qMetaTypeId<ParserStatusTestType *>())->propertyValueSourceCast(), -1);

    QVERIFY(QQmlMetaType::qmlType(qMetaTypeId<ValueSourceTestType *>()) != 0);
    int cast = QQmlMetaType::qmlType(qMetaTypeId<ValueSourceTestType *>())->propertyValueSourceCast();
    QVERIFY(cast != -1);
    QVERIFY(cast != 0);

    ValueSourceTestType t;
    QVERIFY(reinterpret_cast<char *>((QObject *)&t) != reinterpret_cast<char *>((QQmlPropertyValueSource *)&t));

    QQmlPropertyValueSource *source = reinterpret_cast<QQmlPropertyValueSource *>(reinterpret_cast<char *>((QObject *)&t) + cast);
    QCOMPARE(source, (QQmlPropertyValueSource*)&t);
}

void tst_qqmlmetatype::qmlPropertyValueInterceptorCast()
{
    QVERIFY(!QQmlMetaType::qmlType(QVariant::Int));
    QVERIFY(QQmlMetaType::qmlType(qMetaTypeId<TestType *>()) != 0);
    QCOMPARE(QQmlMetaType::qmlType(qMetaTypeId<TestType *>())->propertyValueInterceptorCast(), -1);
    QVERIFY(QQmlMetaType::qmlType(qMetaTypeId<ParserStatusTestType *>()) != 0);
    QCOMPARE(QQmlMetaType::qmlType(qMetaTypeId<ParserStatusTestType *>())->propertyValueInterceptorCast(), -1);

    QVERIFY(QQmlMetaType::qmlType(qMetaTypeId<ValueInterceptorTestType *>()) != 0);
    int cast = QQmlMetaType::qmlType(qMetaTypeId<ValueInterceptorTestType *>())->propertyValueInterceptorCast();
    QVERIFY(cast != -1);
    QVERIFY(cast != 0);

    ValueInterceptorTestType t;
    QVERIFY(reinterpret_cast<char *>((QObject *)&t) != reinterpret_cast<char *>((QQmlPropertyValueInterceptor *)&t));

    QQmlPropertyValueInterceptor *interceptor = reinterpret_cast<QQmlPropertyValueInterceptor *>(reinterpret_cast<char *>((QObject *)&t) + cast);
    QCOMPARE(interceptor, (QQmlPropertyValueInterceptor*)&t);
}

void tst_qqmlmetatype::qmlType()
{
    QQmlType *type = QQmlMetaType::qmlType(QString("ParserStatusTestType"), QString("Test"), 1, 0);
    QVERIFY(type);
    QVERIFY(type->module() == QLatin1String("Test"));
    QVERIFY(type->elementName() == QLatin1String("ParserStatusTestType"));
    QCOMPARE(type->qmlTypeName(), QLatin1String("Test/ParserStatusTestType"));

    type = QQmlMetaType::qmlType("Test/ParserStatusTestType", 1, 0);
    QVERIFY(type);
    QVERIFY(type->module() == QLatin1String("Test"));
    QVERIFY(type->elementName() == QLatin1String("ParserStatusTestType"));
    QCOMPARE(type->qmlTypeName(), QLatin1String("Test/ParserStatusTestType"));
}

void tst_qqmlmetatype::invalidQmlTypeName()
{
    QStringList currFailures = QQmlMetaType::typeRegistrationFailures();
    QCOMPARE(qmlRegisterType<TestType>("TestNamespace", 1, 0, "Test$Type"), -1); // should fail due to invalid QML type name.
    QStringList nowFailures = QQmlMetaType::typeRegistrationFailures();

    foreach (const QString &f, currFailures)
        nowFailures.removeOne(f);

    QCOMPARE(nowFailures.size(), 1);
    QCOMPARE(nowFailures.at(0), QStringLiteral("Invalid QML element name \"Test$Type\""));
}

void tst_qqmlmetatype::isList()
{
    QCOMPARE(QQmlMetaType::isList(QVariant::Invalid), false);
    QCOMPARE(QQmlMetaType::isList(QVariant::Int), false);

    QQmlListProperty<TestType> list;

    QCOMPARE(QQmlMetaType::isList(qMetaTypeId<QQmlListProperty<TestType> >()), true);
}

void tst_qqmlmetatype::defaultObject()
{
    QVERIFY(!QQmlMetaType::defaultProperty(&QObject::staticMetaObject).name());
    QVERIFY(!QQmlMetaType::defaultProperty(&ParserStatusTestType::staticMetaObject).name());
    QCOMPARE(QString(QQmlMetaType::defaultProperty(&TestType::staticMetaObject).name()), QString("foo"));

    QObject o;
    TestType t;
    ParserStatusTestType p;

    QVERIFY(QQmlMetaType::defaultProperty((QObject *)0).name() == 0);
    QVERIFY(!QQmlMetaType::defaultProperty(&o).name());
    QVERIFY(!QQmlMetaType::defaultProperty(&p).name());
    QCOMPARE(QString(QQmlMetaType::defaultProperty(&t).name()), QString("foo"));
}

void tst_qqmlmetatype::registrationType()
{
    QQmlType *type = QQmlMetaType::qmlType(QString("TestType"), QString("Test"), 1, 0);
    QVERIFY(type);
    QVERIFY(!type->isInterface());
    QVERIFY(!type->isSingleton());
    QVERIFY(!type->isComposite());

    type = QQmlMetaType::qmlType(QString("TestTypeSingleton"), QString("Test"), 1, 0);
    QVERIFY(type);
    QVERIFY(!type->isInterface());
    QVERIFY(type->isSingleton());
    QVERIFY(!type->isComposite());

    type = QQmlMetaType::qmlType(QString("TestTypeComposite"), QString("Test"), 1, 0);
    QVERIFY(type);
    QVERIFY(!type->isInterface());
    QVERIFY(!type->isSingleton());
    QVERIFY(type->isComposite());
}

void tst_qqmlmetatype::compositeType()
{
    QQmlEngine engine;

    //Loading the test file also loads all composite types it imports
    QQmlComponent c(&engine, testFileUrl("testImplicitComposite.qml"));
    QObject* obj = c.create();
    QVERIFY(obj);

    QQmlType *type = QQmlMetaType::qmlType(QString("ImplicitType"), QString(""), 1, 0);
    QVERIFY(type);
    QVERIFY(type->module().isEmpty());
    QCOMPARE(type->elementName(), QLatin1String("ImplicitType"));
    QCOMPARE(type->qmlTypeName(), QLatin1String("ImplicitType"));
    QCOMPARE(type->sourceUrl(), testFileUrl("ImplicitType.qml"));
}

QTEST_MAIN(tst_qqmlmetatype)

#include "tst_qqmlmetatype.moc"
