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
#include <qdeclarativedatatest.h>
#include <QtDeclarative/qdeclarativecomponent.h>
#include <QtDeclarative/qdeclarativeengine.h>
#include <QtDeclarative/qdeclarativeexpression.h>
#include <QtDeclarative/qdeclarativecontext.h>
#include <QtCore/qfileinfo.h>
#include <QtCore/qdebug.h>
#include <QtDeclarative/private/qdeclarativeguard_p.h>
#include <QtCore/qdir.h>
#include <QtCore/qnumeric.h>
#include <private/qdeclarativeengine_p.h>
#include <private/qdeclarativeglobalscriptclass_p.h>
#include <private/qscriptdeclarativeclass_p.h>
#include "testtypes.h"
#include "testhttpserver.h"

/*
This test covers evaluation of ECMAScript expressions and bindings from within
QML.  This does not include static QML language issues.

Static QML language issues are covered in qmllanguage
*/

class tst_qdeclarativeecmascript : public QDeclarativeDataTest
{
    Q_OBJECT
public:
    tst_qdeclarativeecmascript() {}

private slots:
    void initTestCase();
    void assignBasicTypes();
    void idShortcutInvalidates();
    void boolPropertiesEvaluateAsBool();
    void methods();
    void signalAssignment();
    void bindingLoop();
    void basicExpressions();
    void basicExpressions_data();
    void arrayExpressions();
    void contextPropertiesTriggerReeval();
    void objectPropertiesTriggerReeval();
    void deferredProperties();
    void deferredPropertiesErrors();
    void extensionObjects();
    void overrideExtensionProperties();
    void attachedProperties();
    void enums();
    void valueTypeFunctions();
    void constantsOverrideBindings();
    void outerBindingOverridesInnerBinding();
    void aliasPropertyAndBinding();
    void nonExistentAttachedObject();
    void scope();
    void signalParameterTypes();
    void objectsCompareAsEqual();
    void dynamicCreation_data();
    void dynamicCreation();
    void dynamicDestruction();
    void objectToString();
    void selfDeletingBinding();
    void extendedObjectPropertyLookup();
    void scriptErrors();
    void functionErrors();
    void propertyAssignmentErrors();
    void signalTriggeredBindings();
    void listProperties();
    void exceptionClearsOnReeval();
    void exceptionSlotProducesWarning();
    void exceptionBindingProducesWarning();
    void transientErrors();
    void shutdownErrors();
    void compositePropertyType();
    void jsObject();
    void undefinedResetsProperty();
    void listToVariant();
    void multiEngineObject();
    void deletedObject();
    void attachedPropertyScope();
    void scriptConnect();
    void scriptDisconnect();
    void ownership();
    void cppOwnershipReturnValue();
    void ownershipCustomReturnValue();
    void qlistqobjectMethods();
    void strictlyEquals();
    void compiled();
    void numberAssignment();
    void propertySplicing();
    void signalWithUnknownTypes();

    void bug1();
    void bug2();
    void dynamicCreationCrash();
    void regExpBug();
    void nullObjectBinding();
    void deletedEngine();
    void libraryScriptAssert();
    void variantsAssignedUndefined();
    void qtbug_9792();
    void qtcreatorbug_1289();
    void noSpuriousWarningsAtShutdown();
    void canAssignNullToQObject();
    void functionAssignment_fromBinding();
    void functionAssignment_fromJS();
    void functionAssignment_fromJS_data();
    void functionAssignmentfromJS_invalid();
    void eval();
    void function();
    void qtbug_10696();
    void qtbug_11606();
    void qtbug_11600();
    void nonscriptable();
    void deleteLater();
    void in();
    void sharedAttachedObject();
    void objectName();
    void writeRemovesBinding();
    void aliasBindingsAssignCorrectly();
    void aliasBindingsOverrideTarget();
    void aliasWritesOverrideBindings();
    void pushCleanContext();
    void realToInt();
    void qtbug_20648();

    void include();

    void callQtInvokables();
    void invokableObjectArg();
    void invokableObjectRet();

    void revisionErrors();
    void revision();
private:
    QDeclarativeEngine engine;
};

void tst_qdeclarativeecmascript::initTestCase()
{
    QDeclarativeDataTest::initTestCase();
    registerTypes();
}

void tst_qdeclarativeecmascript::assignBasicTypes()
{
    {
    QDeclarativeComponent component(&engine, testFileUrl("assignBasicTypes.qml"));
    MyTypeObject *object = qobject_cast<MyTypeObject *>(component.create());
    QVERIFY(object != 0);
    QCOMPARE(object->flagProperty(), MyTypeObject::FlagVal1 | MyTypeObject::FlagVal3);
    QCOMPARE(object->enumProperty(), MyTypeObject::EnumVal2);
    QCOMPARE(object->stringProperty(), QString("Hello World!"));
    QCOMPARE(object->uintProperty(), uint(10));
    QCOMPARE(object->intProperty(), -19);
    QCOMPARE((float)object->realProperty(), float(23.2));
    QCOMPARE((float)object->doubleProperty(), float(-19.75));
    QCOMPARE((float)object->floatProperty(), float(8.5));
    QCOMPARE(object->colorProperty(), QColor("red"));
    QCOMPARE(object->dateProperty(), QDate(1982, 11, 25));
    QCOMPARE(object->timeProperty(), QTime(11, 11, 32));
    QCOMPARE(object->dateTimeProperty(), QDateTime(QDate(2009, 5, 12), QTime(13, 22, 1)));
    QCOMPARE(object->pointProperty(), QPoint(99,13));
    QCOMPARE(object->pointFProperty(), QPointF(-10.1, 12.3));
    QCOMPARE(object->sizeProperty(), QSize(99, 13));
    QCOMPARE(object->sizeFProperty(), QSizeF(0.1, 0.2));
    QCOMPARE(object->rectProperty(), QRect(9, 7, 100, 200));
    QCOMPARE(object->rectFProperty(), QRectF(1000.1, -10.9, 400, 90.99));
    QCOMPARE(object->boolProperty(), true);
    QCOMPARE(object->variantProperty(), QVariant("Hello World!"));
    QCOMPARE(object->vectorProperty(), QVector3D(10, 1, 2.2));
    QCOMPARE(object->urlProperty(), component.url().resolved(QUrl("main.qml")));
    delete object;
    }
    {
    QDeclarativeComponent component(&engine, testFileUrl("assignBasicTypes.2.qml"));
    MyTypeObject *object = qobject_cast<MyTypeObject *>(component.create());
    QVERIFY(object != 0);
    QCOMPARE(object->flagProperty(), MyTypeObject::FlagVal1 | MyTypeObject::FlagVal3);
    QCOMPARE(object->enumProperty(), MyTypeObject::EnumVal2);
    QCOMPARE(object->stringProperty(), QString("Hello World!"));
    QCOMPARE(object->uintProperty(), uint(10));
    QCOMPARE(object->intProperty(), -19);
    QCOMPARE((float)object->realProperty(), float(23.2));
    QCOMPARE((float)object->doubleProperty(), float(-19.75));
    QCOMPARE((float)object->floatProperty(), float(8.5));
    QCOMPARE(object->colorProperty(), QColor("red"));
    QCOMPARE(object->dateProperty(), QDate(1982, 11, 25));
    QCOMPARE(object->timeProperty(), QTime(11, 11, 32));
    QCOMPARE(object->dateTimeProperty(), QDateTime(QDate(2009, 5, 12), QTime(13, 22, 1)));
    QCOMPARE(object->pointProperty(), QPoint(99,13));
    QCOMPARE(object->pointFProperty(), QPointF(-10.1, 12.3));
    QCOMPARE(object->sizeProperty(), QSize(99, 13));
    QCOMPARE(object->sizeFProperty(), QSizeF(0.1, 0.2));
    QCOMPARE(object->rectProperty(), QRect(9, 7, 100, 200));
    QCOMPARE(object->rectFProperty(), QRectF(1000.1, -10.9, 400, 90.99));
    QCOMPARE(object->boolProperty(), true);
    QCOMPARE(object->variantProperty(), QVariant("Hello World!"));
    QCOMPARE(object->vectorProperty(), QVector3D(10, 1, 2.2));
    QCOMPARE(object->urlProperty(), component.url().resolved(QUrl("main.qml")));
    delete object;
    }
}

void tst_qdeclarativeecmascript::idShortcutInvalidates()
{
    {
        QDeclarativeComponent component(&engine, testFileUrl("idShortcutInvalidates.qml"));
        MyQmlObject *object = qobject_cast<MyQmlObject *>(component.create());
        QVERIFY(object != 0);
        QVERIFY(object->objectProperty() != 0);
        delete object->objectProperty();
        QVERIFY(object->objectProperty() == 0);
    }

    {
        QDeclarativeComponent component(&engine, testFileUrl("idShortcutInvalidates.1.qml"));
        MyQmlObject *object = qobject_cast<MyQmlObject *>(component.create());
        QVERIFY(object != 0);
        QVERIFY(object->objectProperty() != 0);
        delete object->objectProperty();
        QVERIFY(object->objectProperty() == 0);
    }
}

void tst_qdeclarativeecmascript::boolPropertiesEvaluateAsBool()
{
    {
        QDeclarativeComponent component(&engine, testFileUrl("boolPropertiesEvaluateAsBool.1.qml"));
        MyQmlObject *object = qobject_cast<MyQmlObject *>(component.create());
        QVERIFY(object != 0);
        QCOMPARE(object->stringProperty(), QLatin1String("pass"));
    }
    {
        QDeclarativeComponent component(&engine, testFileUrl("boolPropertiesEvaluateAsBool.2.qml"));
        MyQmlObject *object = qobject_cast<MyQmlObject *>(component.create());
        QVERIFY(object != 0);
        QCOMPARE(object->stringProperty(), QLatin1String("pass"));
    }
}

void tst_qdeclarativeecmascript::signalAssignment()
{
    {
        QDeclarativeComponent component(&engine, testFileUrl("signalAssignment.1.qml"));
        MyQmlObject *object = qobject_cast<MyQmlObject *>(component.create());
        QVERIFY(object != 0);
        QCOMPARE(object->string(), QString());
        emit object->basicSignal();
        QCOMPARE(object->string(), QString("pass"));
    }

    {
        QDeclarativeComponent component(&engine, testFileUrl("signalAssignment.2.qml"));
        MyQmlObject *object = qobject_cast<MyQmlObject *>(component.create());
        QVERIFY(object != 0);
        QCOMPARE(object->string(), QString());
        emit object->argumentSignal(19, "Hello world!", 10.25, MyQmlObject::EnumValue4, Qt::RightButton);
        QCOMPARE(object->string(), QString("pass 19 Hello world! 10.25 3 2"));
    }
}

void tst_qdeclarativeecmascript::methods()
{
    {
        QDeclarativeComponent component(&engine, testFileUrl("methods.1.qml"));
        MyQmlObject *object = qobject_cast<MyQmlObject *>(component.create());
        QVERIFY(object != 0);
        QCOMPARE(object->methodCalled(), false);
        QCOMPARE(object->methodIntCalled(), false);
        emit object->basicSignal();
        QCOMPARE(object->methodCalled(), true);
        QCOMPARE(object->methodIntCalled(), false);
    }

    {
        QDeclarativeComponent component(&engine, testFileUrl("methods.2.qml"));
        MyQmlObject *object = qobject_cast<MyQmlObject *>(component.create());
        QVERIFY(object != 0);
        QCOMPARE(object->methodCalled(), false);
        QCOMPARE(object->methodIntCalled(), false);
        emit object->basicSignal();
        QCOMPARE(object->methodCalled(), false);
        QCOMPARE(object->methodIntCalled(), true);
    }

    {
        QDeclarativeComponent component(&engine, testFileUrl("methods.3.qml"));
        QObject *object = component.create();
        QVERIFY(object != 0);
        QCOMPARE(object->property("test").toInt(), 19);
    }

    {
        QDeclarativeComponent component(&engine, testFileUrl("methods.4.qml"));
        QObject *object = component.create();
        QVERIFY(object != 0);
        QCOMPARE(object->property("test").toInt(), 19);
        QCOMPARE(object->property("test2").toInt(), 17);
        QCOMPARE(object->property("test3").toInt(), 16);
    }

    {
        QDeclarativeComponent component(&engine, testFileUrl("methods.5.qml"));
        QObject *object = component.create();
        QVERIFY(object != 0);
        QCOMPARE(object->property("test").toInt(), 9);
    }
}

void tst_qdeclarativeecmascript::bindingLoop()
{
    QDeclarativeComponent component(&engine, testFileUrl("bindingLoop.qml"));
    QString warning = component.url().toString() + ":9:9: QML MyQmlObject: Binding loop detected for property \"stringProperty\"";
    QTest::ignoreMessage(QtWarningMsg, warning.toLatin1().constData());
    QObject *object = component.create();
    QVERIFY(object != 0);
}

void tst_qdeclarativeecmascript::basicExpressions_data()
{
    QTest::addColumn<QString>("expression");
    QTest::addColumn<QVariant>("result");
    QTest::addColumn<bool>("nest");

    QTest::newRow("Syntax error (self test)") << "{console.log({'a':1'}.a)}" << QVariant() << false;
    QTest::newRow("Context property") << "a" << QVariant(1944) << false;
    QTest::newRow("Context property") << "a" << QVariant(1944) << true;
    QTest::newRow("Context property expression") << "a * 2" << QVariant(3888) << false;
    QTest::newRow("Context property expression") << "a * 2" << QVariant(3888) << true;
    QTest::newRow("Overridden context property") << "b" << QVariant("Milk") << false;
    QTest::newRow("Overridden context property") << "b" << QVariant("Cow") << true;
    QTest::newRow("Object property") << "object.stringProperty" << QVariant("Object1") << false;
    QTest::newRow("Object property") << "object.stringProperty" << QVariant("Object1") << true;
    QTest::newRow("Overridden object property") << "objectOverride.stringProperty" << QVariant("Object2") << false;
    QTest::newRow("Overridden object property") << "objectOverride.stringProperty" << QVariant("Object3") << true;
    QTest::newRow("Default object property") << "horseLegs" << QVariant(4) << false;
    QTest::newRow("Default object property") << "antLegs" << QVariant(6) << false;
    QTest::newRow("Default object property") << "emuLegs" << QVariant(2) << false;
    QTest::newRow("Nested default object property") << "horseLegs" << QVariant(4) << true;
    QTest::newRow("Nested default object property") << "antLegs" << QVariant(7) << true;
    QTest::newRow("Nested default object property") << "emuLegs" << QVariant(2) << true;
    QTest::newRow("Nested default object property") << "humanLegs" << QVariant(2) << true;
    QTest::newRow("Context property override default object property") << "millipedeLegs" << QVariant(100) << true;
}

void tst_qdeclarativeecmascript::basicExpressions()
{
    QFETCH(QString, expression);
    QFETCH(QVariant, result);
    QFETCH(bool, nest);

    MyQmlObject object1;
    MyQmlObject object2;
    MyQmlObject object3;
    MyDefaultObject1 default1;
    MyDefaultObject3 default3;
    object1.setStringProperty("Object1");
    object2.setStringProperty("Object2");
    object3.setStringProperty("Object3");

    QDeclarativeContext context(engine.rootContext());
    QDeclarativeContext nestedContext(&context);

    context.setContextObject(&default1);
    context.setContextProperty("a", QVariant(1944));
    context.setContextProperty("b", QVariant("Milk"));
    context.setContextProperty("object", &object1);
    context.setContextProperty("objectOverride", &object2);
    nestedContext.setContextObject(&default3);
    nestedContext.setContextProperty("b", QVariant("Cow"));
    nestedContext.setContextProperty("objectOverride", &object3);
    nestedContext.setContextProperty("millipedeLegs", QVariant(100));

    MyExpression expr(nest?&nestedContext:&context, expression);
    QCOMPARE(expr.evaluate(), result);
}

void tst_qdeclarativeecmascript::arrayExpressions()
{
    QObject obj1;
    QObject obj2;
    QObject obj3;

    QDeclarativeContext context(engine.rootContext());
    context.setContextProperty("a", &obj1);
    context.setContextProperty("b", &obj2);
    context.setContextProperty("c", &obj3);

    MyExpression expr(&context, "[a, b, c, 10]");
    QVariant result = expr.evaluate();
    QCOMPARE(result.userType(), qMetaTypeId<QList<QObject *> >());
    QList<QObject *> list = qvariant_cast<QList<QObject *> >(result);
    QCOMPARE(list.count(), 4);
    QCOMPARE(list.at(0), &obj1);
    QCOMPARE(list.at(1), &obj2);
    QCOMPARE(list.at(2), &obj3);
    QCOMPARE(list.at(3), (QObject *)0);

    MyExpression expr2(&context, "[1, 2, \"foo\", \"bar\"]");
    result = expr2.evaluate();
    QCOMPARE(result.userType(), qMetaTypeId<QList<QVariant> >());
    QList<QVariant> list2 = qvariant_cast<QList<QVariant> >(result);
    QCOMPARE(list2.count(), 4);
    QCOMPARE(list2.at(0), QVariant(1));
    QCOMPARE(list2.at(1), QVariant(2));
    QCOMPARE(list2.at(2), QVariant(QString("foo")));
    QCOMPARE(list2.at(3), QVariant(QString("bar")));

    MyExpression expr3(&context, "[]");
    result = expr3.evaluate();
    QCOMPARE(result.userType(), qMetaTypeId<QList<QObject *> >());
    QList<QObject *> list3 = qvariant_cast<QList<QObject *> >(result);
    QCOMPARE(list3.count(), 0);
}

// Tests that modifying a context property will reevaluate expressions
void tst_qdeclarativeecmascript::contextPropertiesTriggerReeval()
{
    QDeclarativeContext context(engine.rootContext());
    MyQmlObject object1;
    MyQmlObject object2;
    MyQmlObject *object3 = new MyQmlObject;

    object1.setStringProperty("Hello");
    object2.setStringProperty("World");

    context.setContextProperty("testProp", QVariant(1));
    context.setContextProperty("testObj", &object1);
    context.setContextProperty("testObj2", object3);

    {
        MyExpression expr(&context, "testProp + 1");
        QCOMPARE(expr.changed, false);
        QCOMPARE(expr.evaluate(), QVariant(2));

        context.setContextProperty("testProp", QVariant(2));
        QCOMPARE(expr.changed, true);
        QCOMPARE(expr.evaluate(), QVariant(3));
    }

    {
        MyExpression expr(&context, "testProp + testProp + testProp");
        QCOMPARE(expr.changed, false);
        QCOMPARE(expr.evaluate(), QVariant(6));

        context.setContextProperty("testProp", QVariant(4));
        QCOMPARE(expr.changed, true);
        QCOMPARE(expr.evaluate(), QVariant(12));
    }

    {
        MyExpression expr(&context, "testObj.stringProperty");
        QCOMPARE(expr.changed, false);
        QCOMPARE(expr.evaluate(), QVariant("Hello"));

        context.setContextProperty("testObj", &object2);
        QCOMPARE(expr.changed, true);
        QCOMPARE(expr.evaluate(), QVariant("World"));
    }

    {
        MyExpression expr(&context, "testObj.stringProperty /**/");
        QCOMPARE(expr.changed, false);
        QCOMPARE(expr.evaluate(), QVariant("World"));

        context.setContextProperty("testObj", &object1);
        QCOMPARE(expr.changed, true);
        QCOMPARE(expr.evaluate(), QVariant("Hello"));
    }

    {
        MyExpression expr(&context, "testObj2");
        QCOMPARE(expr.changed, false);
        QCOMPARE(expr.evaluate(), QVariant::fromValue((QObject *)object3));
    }

}

void tst_qdeclarativeecmascript::objectPropertiesTriggerReeval()
{
    QDeclarativeContext context(engine.rootContext());
    MyQmlObject object1;
    MyQmlObject object2;
    MyQmlObject object3;
    context.setContextProperty("testObj", &object1);

    object1.setStringProperty(QLatin1String("Hello"));
    object2.setStringProperty(QLatin1String("Dog"));
    object3.setStringProperty(QLatin1String("Cat"));

    {
        MyExpression expr(&context, "testObj.stringProperty");
        QCOMPARE(expr.changed, false);
        QCOMPARE(expr.evaluate(), QVariant("Hello"));

        object1.setStringProperty(QLatin1String("World"));
        QCOMPARE(expr.changed, true);
        QCOMPARE(expr.evaluate(), QVariant("World"));
    }

    {
        MyExpression expr(&context, "testObj.objectProperty.stringProperty");
        QCOMPARE(expr.changed, false);
        QCOMPARE(expr.evaluate(), QVariant());

        object1.setObjectProperty(&object2);
        QCOMPARE(expr.changed, true);
        expr.changed = false;
        QCOMPARE(expr.evaluate(), QVariant("Dog"));

        object1.setObjectProperty(&object3);
        QCOMPARE(expr.changed, true);
        expr.changed = false;
        QCOMPARE(expr.evaluate(), QVariant("Cat"));

        object1.setObjectProperty(0);
        QCOMPARE(expr.changed, true);
        expr.changed = false;
        QCOMPARE(expr.evaluate(), QVariant());

        object1.setObjectProperty(&object3);
        QCOMPARE(expr.changed, true);
        expr.changed = false;
        QCOMPARE(expr.evaluate(), QVariant("Cat"));

        object3.setStringProperty("Donkey");
        QCOMPARE(expr.changed, true);
        expr.changed = false;
        QCOMPARE(expr.evaluate(), QVariant("Donkey"));
    }
}

void tst_qdeclarativeecmascript::deferredProperties()
{
    QDeclarativeComponent component(&engine, testFileUrl("deferredProperties.qml"));
    MyDeferredObject *object =
        qobject_cast<MyDeferredObject *>(component.create());
    QVERIFY(object != 0);
    QCOMPARE(object->value(), 0);
    QVERIFY(object->objectProperty() == 0);
    QVERIFY(object->objectProperty2() != 0);
    qmlExecuteDeferred(object);
    QCOMPARE(object->value(), 10);
    QVERIFY(object->objectProperty() != 0);
    MyQmlObject *qmlObject =
        qobject_cast<MyQmlObject *>(object->objectProperty());
    QVERIFY(qmlObject != 0);
    QCOMPARE(qmlObject->value(), 10);
    object->setValue(19);
    QCOMPARE(qmlObject->value(), 19);
}

// Check errors on deferred properties are correctly emitted
void tst_qdeclarativeecmascript::deferredPropertiesErrors()
{
    QDeclarativeComponent component(&engine, testFileUrl("deferredPropertiesErrors.qml"));
    MyDeferredObject *object =
        qobject_cast<MyDeferredObject *>(component.create());
    QVERIFY(object != 0);
    QCOMPARE(object->value(), 0);
    QVERIFY(object->objectProperty() == 0);
    QVERIFY(object->objectProperty2() == 0);

    QString warning = component.url().toString() + ":6: Unable to assign [undefined] to QObject* objectProperty";
    QTest::ignoreMessage(QtWarningMsg, qPrintable(warning));

    qmlExecuteDeferred(object);

    delete object;
}

void tst_qdeclarativeecmascript::extensionObjects()
{
    QDeclarativeComponent component(&engine, testFileUrl("extensionObjects.qml"));
    MyExtendedObject *object =
        qobject_cast<MyExtendedObject *>(component.create());
    QVERIFY(object != 0);
    QCOMPARE(object->baseProperty(), 13);
    QCOMPARE(object->coreProperty(), 9);
    object->setProperty("extendedProperty", QVariant(11));
    object->setProperty("baseExtendedProperty", QVariant(92));
    QCOMPARE(object->coreProperty(), 11);
    QCOMPARE(object->baseProperty(), 92);

    MyExtendedObject *nested = qobject_cast<MyExtendedObject*>(qvariant_cast<QObject *>(object->property("nested")));
    QVERIFY(nested);
    QCOMPARE(nested->baseProperty(), 13);
    QCOMPARE(nested->coreProperty(), 9);
    nested->setProperty("extendedProperty", QVariant(11));
    nested->setProperty("baseExtendedProperty", QVariant(92));
    QCOMPARE(nested->coreProperty(), 11);
    QCOMPARE(nested->baseProperty(), 92);

}

void tst_qdeclarativeecmascript::overrideExtensionProperties()
{
    QDeclarativeComponent component(&engine, testFileUrl("extensionObjectsPropertyOverride.qml"));
    OverrideDefaultPropertyObject *object =
        qobject_cast<OverrideDefaultPropertyObject *>(component.create());
    QVERIFY(object != 0);
    QVERIFY(object->secondProperty() != 0);
    QVERIFY(object->firstProperty() == 0);
}

void tst_qdeclarativeecmascript::attachedProperties()
{
    {
        QDeclarativeComponent component(&engine, testFileUrl("attachedProperty.qml"));
        QObject *object = component.create();
        QVERIFY(object != 0);
        QCOMPARE(object->property("a").toInt(), 19);
        QCOMPARE(object->property("b").toInt(), 19);
        QCOMPARE(object->property("c").toInt(), 19);
        QCOMPARE(object->property("d").toInt(), 19);
    }

    {
        QDeclarativeComponent component(&engine, testFileUrl("attachedProperty.2.qml"));
        QObject *object = component.create();
        QVERIFY(object != 0);
        QCOMPARE(object->property("a").toInt(), 26);
        QCOMPARE(object->property("b").toInt(), 26);
        QCOMPARE(object->property("c").toInt(), 26);
        QCOMPARE(object->property("d").toInt(), 26);
    }

    {
        QDeclarativeComponent component(&engine, testFileUrl("writeAttachedProperty.qml"));
        QObject *object = component.create();
        QVERIFY(object != 0);

        QMetaObject::invokeMethod(object, "writeValue2");

        MyQmlAttachedObject *attached =
            qobject_cast<MyQmlAttachedObject *>(qmlAttachedPropertiesObject<MyQmlObject>(object));
        QVERIFY(attached != 0);

        QCOMPARE(attached->value2(), 9);
    }
}

void tst_qdeclarativeecmascript::enums()
{
    // Existent enums
    {
    QDeclarativeComponent component(&engine, testFileUrl("enums.1.qml"));
    QObject *object = component.create();
    QVERIFY(object != 0);

    QCOMPARE(object->property("a").toInt(), 0);
    QCOMPARE(object->property("b").toInt(), 1);
    QCOMPARE(object->property("c").toInt(), 2);
    QCOMPARE(object->property("d").toInt(), 3);
    QCOMPARE(object->property("e").toInt(), 0);
    QCOMPARE(object->property("f").toInt(), 1);
    QCOMPARE(object->property("g").toInt(), 2);
    QCOMPARE(object->property("h").toInt(), 3);
    QCOMPARE(object->property("i").toInt(), 19);
    QCOMPARE(object->property("j").toInt(), 19);
    }
    // Non-existent enums
    {
    QDeclarativeComponent component(&engine, testFileUrl("enums.2.qml"));

    QString warning1 = component.url().toString() + ":5: Unable to assign [undefined] to int a";
    QString warning2 = component.url().toString() + ":6: Unable to assign [undefined] to int b";
    QTest::ignoreMessage(QtWarningMsg, qPrintable(warning1));
    QTest::ignoreMessage(QtWarningMsg, qPrintable(warning2));

    QObject *object = component.create();
    QVERIFY(object != 0);
    QCOMPARE(object->property("a").toInt(), 0);
    QCOMPARE(object->property("b").toInt(), 0);
    }
}

void tst_qdeclarativeecmascript::valueTypeFunctions()
{
    QDeclarativeComponent component(&engine, testFileUrl("valueTypeFunctions.qml"));
    MyTypeObject *obj = qobject_cast<MyTypeObject*>(component.create());
    QVERIFY(obj != 0);
    QCOMPARE(obj->rectProperty(), QRect(0,0,100,100));
    QCOMPARE(obj->rectFProperty(), QRectF(0,0.5,100,99.5));
}

/*
Tests that writing a constant to a property with a binding on it disables the
binding.
*/
void tst_qdeclarativeecmascript::constantsOverrideBindings()
{
    // From ECMAScript
    {
        QDeclarativeComponent component(&engine, testFileUrl("constantsOverrideBindings.1.qml"));
        MyQmlObject *object = qobject_cast<MyQmlObject *>(component.create());
        QVERIFY(object != 0);

        QCOMPARE(object->property("c2").toInt(), 0);
        object->setProperty("c1", QVariant(9));
        QCOMPARE(object->property("c2").toInt(), 9);

        emit object->basicSignal();

        QCOMPARE(object->property("c2").toInt(), 13);
        object->setProperty("c1", QVariant(8));
        QCOMPARE(object->property("c2").toInt(), 13);
    }

    // During construction
    {
        QDeclarativeComponent component(&engine, testFileUrl("constantsOverrideBindings.2.qml"));
        MyQmlObject *object = qobject_cast<MyQmlObject *>(component.create());
        QVERIFY(object != 0);

        QCOMPARE(object->property("c1").toInt(), 0);
        QCOMPARE(object->property("c2").toInt(), 10);
        object->setProperty("c1", QVariant(9));
        QCOMPARE(object->property("c1").toInt(), 9);
        QCOMPARE(object->property("c2").toInt(), 10);
    }

#if 0
    // From C++
    {
        QDeclarativeComponent component(&engine, testFileUrl("constantsOverrideBindings.3.qml"));
        MyQmlObject *object = qobject_cast<MyQmlObject *>(component.create());
        QVERIFY(object != 0);

        QCOMPARE(object->property("c2").toInt(), 0);
        object->setProperty("c1", QVariant(9));
        QCOMPARE(object->property("c2").toInt(), 9);

        object->setProperty("c2", QVariant(13));
        QCOMPARE(object->property("c2").toInt(), 13);
        object->setProperty("c1", QVariant(7));
        QCOMPARE(object->property("c1").toInt(), 7);
        QCOMPARE(object->property("c2").toInt(), 13);
    }
#endif

    // Using an alias
    {
        QDeclarativeComponent component(&engine, testFileUrl("constantsOverrideBindings.4.qml"));
        MyQmlObject *object = qobject_cast<MyQmlObject *>(component.create());
        QVERIFY(object != 0);

        QCOMPARE(object->property("c1").toInt(), 0);
        QCOMPARE(object->property("c3").toInt(), 10);
        object->setProperty("c1", QVariant(9));
        QCOMPARE(object->property("c1").toInt(), 9);
        QCOMPARE(object->property("c3").toInt(), 10);
    }
}

/*
Tests that assigning a binding to a property that already has a binding causes
the original binding to be disabled.
*/
void tst_qdeclarativeecmascript::outerBindingOverridesInnerBinding()
{
    QDeclarativeComponent component(&engine,
                           testFileUrl("outerBindingOverridesInnerBinding.qml"));
    MyQmlObject *object = qobject_cast<MyQmlObject *>(component.create());
    QVERIFY(object != 0);

    QCOMPARE(object->property("c1").toInt(), 0);
    QCOMPARE(object->property("c2").toInt(), 0);
    QCOMPARE(object->property("c3").toInt(), 0);

    object->setProperty("c1", QVariant(9));
    QCOMPARE(object->property("c1").toInt(), 9);
    QCOMPARE(object->property("c2").toInt(), 0);
    QCOMPARE(object->property("c3").toInt(), 0);

    object->setProperty("c3", QVariant(8));
    QCOMPARE(object->property("c1").toInt(), 9);
    QCOMPARE(object->property("c2").toInt(), 8);
    QCOMPARE(object->property("c3").toInt(), 8);
}

/*
Access a non-existent attached object.

Tests for a regression where this used to crash.
*/
void tst_qdeclarativeecmascript::nonExistentAttachedObject()
{
    QDeclarativeComponent component(&engine, testFileUrl("nonExistentAttachedObject.qml"));

    QString warning = component.url().toString() + ":4: Unable to assign [undefined] to QString stringProperty";
    QTest::ignoreMessage(QtWarningMsg, qPrintable(warning));

    QObject *object = component.create();
    QVERIFY(object != 0);
}

void tst_qdeclarativeecmascript::scope()
{
    {
        QDeclarativeComponent component(&engine, testFileUrl("scope.qml"));
        QObject *object = component.create();
        QVERIFY(object != 0);

        QCOMPARE(object->property("test1").toInt(), 1);
        QCOMPARE(object->property("test2").toInt(), 2);
        QCOMPARE(object->property("test3").toString(), QString("1Test"));
        QCOMPARE(object->property("test4").toString(), QString("2Test"));
        QCOMPARE(object->property("test5").toInt(), 1);
        QCOMPARE(object->property("test6").toInt(), 1);
        QCOMPARE(object->property("test7").toInt(), 2);
        QCOMPARE(object->property("test8").toInt(), 2);
        QCOMPARE(object->property("test9").toInt(), 1);
        QCOMPARE(object->property("test10").toInt(), 3);
    }

    {
        QDeclarativeComponent component(&engine, testFileUrl("scope.2.qml"));
        QObject *object = component.create();
        QVERIFY(object != 0);

        QCOMPARE(object->property("test1").toInt(), 19);
        QCOMPARE(object->property("test2").toInt(), 19);
        QCOMPARE(object->property("test3").toInt(), 14);
        QCOMPARE(object->property("test4").toInt(), 14);
        QCOMPARE(object->property("test5").toInt(), 24);
        QCOMPARE(object->property("test6").toInt(), 24);
    }

    {
        QDeclarativeComponent component(&engine, testFileUrl("scope.3.qml"));
        QObject *object = component.create();
        QVERIFY(object != 0);

        QCOMPARE(object->property("test1").toBool(), true);
        QCOMPARE(object->property("test2").toBool(), true);
        QCOMPARE(object->property("test3").toBool(), true);
    }

    // Signal argument scope
    {
        QDeclarativeComponent component(&engine, testFileUrl("scope.4.qml"));
        MyQmlObject *object = qobject_cast<MyQmlObject *>(component.create());
        QVERIFY(object != 0);

        QCOMPARE(object->property("test").toInt(), 0);
        QCOMPARE(object->property("test2").toString(), QString());

        emit object->argumentSignal(13, "Argument Scope", 9, MyQmlObject::EnumValue4, Qt::RightButton);

        QCOMPARE(object->property("test").toInt(), 13);
        QCOMPARE(object->property("test2").toString(), QString("Argument Scope"));

        delete object;
    }
}

/*
Tests that "any" type passes through a synthesized signal parameter.  This
is essentially a test of QDeclarativeMetaType::copy()
*/
void tst_qdeclarativeecmascript::signalParameterTypes()
{
    QDeclarativeComponent component(&engine, testFileUrl("signalParameterTypes.qml"));
    MyQmlObject *object = qobject_cast<MyQmlObject *>(component.create());
    QVERIFY(object != 0);

    emit object->basicSignal();

    QCOMPARE(object->property("intProperty").toInt(), 10);
    QCOMPARE(object->property("realProperty").toReal(), 19.2);
    QVERIFY(object->property("colorProperty").value<QColor>() == QColor(255, 255, 0, 255));
    QVERIFY(object->property("variantProperty") == QVariant::fromValue(QColor(255, 0, 255, 255)));
    QVERIFY(object->property("enumProperty") == MyQmlObject::EnumValue3);
    QVERIFY(object->property("qtEnumProperty") == Qt::LeftButton);
}

/*
Test that two JS objects for the same QObject compare as equal.
*/
void tst_qdeclarativeecmascript::objectsCompareAsEqual()
{
    QDeclarativeComponent component(&engine, testFileUrl("objectsCompareAsEqual.qml"));
    QObject *object = component.create();
    QVERIFY(object != 0);

    QCOMPARE(object->property("test1").toBool(), true);
    QCOMPARE(object->property("test2").toBool(), true);
    QCOMPARE(object->property("test3").toBool(), true);
    QCOMPARE(object->property("test4").toBool(), true);
    QCOMPARE(object->property("test5").toBool(), true);
}

/*
Confirm bindings and alias properties can coexist.

Tests for a regression where the binding would not reevaluate.
*/
void tst_qdeclarativeecmascript::aliasPropertyAndBinding()
{
    QDeclarativeComponent component(&engine, testFileUrl("aliasPropertyAndBinding.qml"));
    QObject *object = component.create();
    QVERIFY(object != 0);

    QCOMPARE(object->property("c2").toInt(), 3);
    QCOMPARE(object->property("c3").toInt(), 3);

    object->setProperty("c2", QVariant(19));

    QCOMPARE(object->property("c2").toInt(), 19);
    QCOMPARE(object->property("c3").toInt(), 19);
}

void tst_qdeclarativeecmascript::dynamicCreation_data()
{
    QTest::addColumn<QString>("method");
    QTest::addColumn<QString>("createdName");

    QTest::newRow("One") << "createOne" << "objectOne";
    QTest::newRow("Two") << "createTwo" << "objectTwo";
    QTest::newRow("Three") << "createThree" << "objectThree";
}

/*
Test using createQmlObject to dynamically generate an item
Also using createComponent is tested.
*/
void tst_qdeclarativeecmascript::dynamicCreation()
{
    QFETCH(QString, method);
    QFETCH(QString, createdName);

    QDeclarativeComponent component(&engine, testFileUrl("dynamicCreation.qml"));
    MyQmlObject *object = qobject_cast<MyQmlObject*>(component.create());
    QVERIFY(object != 0);

    QMetaObject::invokeMethod(object, method.toUtf8());
    QObject *created = object->objectProperty();
    QVERIFY(created);
    QCOMPARE(created->objectName(), createdName);
}

/*
   Tests the destroy function
*/
void tst_qdeclarativeecmascript::dynamicDestruction()
{
    QDeclarativeComponent component(&engine, testFileUrl("dynamicDeletion.qml"));
    QDeclarativeGuard<MyQmlObject> object = qobject_cast<MyQmlObject*>(component.create());
    QVERIFY(object != 0);
    QDeclarativeGuard<QObject> createdQmlObject = 0;

    QMetaObject::invokeMethod(object, "create");
    createdQmlObject = object->objectProperty();
    QVERIFY(createdQmlObject);
    QCOMPARE(createdQmlObject->objectName(), QString("emptyObject"));

    QMetaObject::invokeMethod(object, "killOther");
    QVERIFY(createdQmlObject);
    QCoreApplication::sendPostedEvents(0, QEvent::DeferredDelete);
    QVERIFY(createdQmlObject);
    for (int ii = 0; createdQmlObject && ii < 50; ++ii) { // After 5 seconds we should give up
        if (createdQmlObject) {
            QTest::qWait(100);
            QCoreApplication::sendPostedEvents(0, QEvent::DeferredDelete);
        }
    }
    QVERIFY(!createdQmlObject);

    QDeclarativeEngine::setObjectOwnership(object, QDeclarativeEngine::JavaScriptOwnership);
    QMetaObject::invokeMethod(object, "killMe");
    QVERIFY(object);
    QTest::qWait(0);
    QCoreApplication::sendPostedEvents(0, QEvent::DeferredDelete);
    QVERIFY(!object);
}

/*
   tests that id.toString() works
*/
void tst_qdeclarativeecmascript::objectToString()
{
    QDeclarativeComponent component(&engine, testFileUrl("declarativeToString.qml"));
    MyQmlObject *object = qobject_cast<MyQmlObject*>(component.create());
    QVERIFY(object != 0);
    QMetaObject::invokeMethod(object, "testToString");
    QVERIFY(object->stringProperty().startsWith("MyQmlObject_QML_"));
    QVERIFY(object->stringProperty().endsWith(", \"objName\")"));
}

/*
Tests bindings that indirectly cause their own deletion work.

This test is best run under valgrind to ensure no invalid memory access occur.
*/
void tst_qdeclarativeecmascript::selfDeletingBinding()
{
    {
        QDeclarativeComponent component(&engine, testFileUrl("selfDeletingBinding.qml"));
        QObject *object = component.create();
        QVERIFY(object != 0);
        object->setProperty("triggerDelete", true);
    }

    {
        QDeclarativeComponent component(&engine, testFileUrl("selfDeletingBinding.2.qml"));
        QObject *object = component.create();
        QVERIFY(object != 0);
        object->setProperty("triggerDelete", true);
    }
}

/*
Test that extended object properties can be accessed.

This test a regression where this used to crash.  The issue was specificially
for extended objects that did not include a synthesized meta object (so non-root
and no synthesiszed properties).
*/
void tst_qdeclarativeecmascript::extendedObjectPropertyLookup()
{
    QDeclarativeComponent component(&engine, testFileUrl("extendedObjectPropertyLookup.qml"));
    QObject *object = component.create();
    QVERIFY(object != 0);
}

/*
Test file/lineNumbers for binding/Script errors.
*/
void tst_qdeclarativeecmascript::scriptErrors()
{
    QDeclarativeComponent component(&engine, testFileUrl("scriptErrors.qml"));
    QString url = component.url().toString();

    QString warning1 = url.left(url.length() - 3) + "js:2: Error: Invalid write to global property \"a\"";
    QString warning2 = url + ":5: ReferenceError: Can't find variable: a";
    QString warning3 = url.left(url.length() - 3) + "js:4: Error: Invalid write to global property \"a\"";
    QString warning4 = url + ":10: ReferenceError: Can't find variable: a";
    QString warning5 = url + ":8: ReferenceError: Can't find variable: a";
    QString warning6 = url + ":7: Unable to assign [undefined] to int x";
    QString warning7 = url + ":12: Error: Cannot assign to read-only property \"trueProperty\"";
    QString warning8 = url + ":13: Error: Cannot assign to non-existent property \"fakeProperty\"";

    QTest::ignoreMessage(QtWarningMsg, warning1.toLatin1().constData());
    QTest::ignoreMessage(QtWarningMsg, warning2.toLatin1().constData());
    QTest::ignoreMessage(QtWarningMsg, warning3.toLatin1().constData());
    QTest::ignoreMessage(QtWarningMsg, warning5.toLatin1().constData());
    QTest::ignoreMessage(QtWarningMsg, warning6.toLatin1().constData());
    MyQmlObject *object = qobject_cast<MyQmlObject *>(component.create());
    QVERIFY(object != 0);

    QTest::ignoreMessage(QtWarningMsg, warning4.toLatin1().constData());
    emit object->basicSignal();

    QTest::ignoreMessage(QtWarningMsg, warning7.toLatin1().constData());
    emit object->anotherBasicSignal();

    QTest::ignoreMessage(QtWarningMsg, warning8.toLatin1().constData());
    emit object->thirdBasicSignal();
}

/*
Test file/lineNumbers for inline functions.
*/
void tst_qdeclarativeecmascript::functionErrors()
{
    QDeclarativeComponent component(&engine, testFileUrl("functionErrors.qml"));
    QString url = component.url().toString();

    QString warning = url + ":5: Error: Invalid write to global property \"a\"";

    QTest::ignoreMessage(QtWarningMsg, warning.toLatin1().constData());

    QObject *object = component.create();
    QVERIFY(object != 0);
    delete object;
}

/*
Test various errors that can occur when assigning a property from script
*/
void tst_qdeclarativeecmascript::propertyAssignmentErrors()
{
    QDeclarativeComponent component(&engine, testFileUrl("propertyAssignmentErrors.qml"));

    QString url = component.url().toString();

    QString warning1 = url + ":11:Error: Cannot assign [undefined] to int";
    QString warning2 = url + ":17:Error: Cannot assign QString to int";

    QTest::ignoreMessage(QtDebugMsg, warning1.toLatin1().constData());
    QTest::ignoreMessage(QtDebugMsg, warning2.toLatin1().constData());

    QObject *object = component.create();
    QVERIFY(object != 0);

    delete object;
}

/*
Test bindings still work when the reeval is triggered from within
a signal script.
*/
void tst_qdeclarativeecmascript::signalTriggeredBindings()
{
    QDeclarativeComponent component(&engine, testFileUrl("signalTriggeredBindings.qml"));
    MyQmlObject *object = qobject_cast<MyQmlObject*>(component.create());
    QVERIFY(object != 0);

    QCOMPARE(object->property("base").toReal(), 50.);
    QCOMPARE(object->property("test1").toReal(), 50.);
    QCOMPARE(object->property("test2").toReal(), 50.);

    object->basicSignal();

    QCOMPARE(object->property("base").toReal(), 200.);
    QCOMPARE(object->property("test1").toReal(), 200.);
    QCOMPARE(object->property("test2").toReal(), 200.);

    object->argumentSignal(10, QString(), 10, MyQmlObject::EnumValue4, Qt::RightButton);

    QCOMPARE(object->property("base").toReal(), 400.);
    QCOMPARE(object->property("test1").toReal(), 400.);
    QCOMPARE(object->property("test2").toReal(), 400.);
}

/*
Test that list properties can be iterated from ECMAScript
*/
void tst_qdeclarativeecmascript::listProperties()
{
    QDeclarativeComponent component(&engine, testFileUrl("listProperties.qml"));
    MyQmlObject *object = qobject_cast<MyQmlObject*>(component.create());
    QVERIFY(object != 0);

    QCOMPARE(object->property("test1").toInt(), 21);
    QCOMPARE(object->property("test2").toInt(), 2);
    QCOMPARE(object->property("test3").toBool(), true);
    QCOMPARE(object->property("test4").toBool(), true);
}

void tst_qdeclarativeecmascript::exceptionClearsOnReeval()
{
    QDeclarativeComponent component(&engine, testFileUrl("exceptionClearsOnReeval.qml"));
    QString url = component.url().toString();

    QString warning = url + ":4: TypeError: Result of expression 'objectProperty' [null] is not an object.";

    QTest::ignoreMessage(QtWarningMsg, warning.toLatin1().constData());
    MyQmlObject *object = qobject_cast<MyQmlObject*>(component.create());
    QVERIFY(object != 0);

    QCOMPARE(object->property("test").toBool(), false);

    MyQmlObject object2;
    MyQmlObject object3;
    object2.setObjectProperty(&object3);
    object->setObjectProperty(&object2);

    QCOMPARE(object->property("test").toBool(), true);
}

void tst_qdeclarativeecmascript::exceptionSlotProducesWarning()
{
    QDeclarativeComponent component(&engine, testFileUrl("exceptionProducesWarning.qml"));
    QString url = component.url().toString();

    QString warning = component.url().toString() + ":6: Error: JS exception";

    QTest::ignoreMessage(QtWarningMsg, warning.toLatin1().constData());
    MyQmlObject *object = qobject_cast<MyQmlObject*>(component.create());
    QVERIFY(object != 0);
}

void tst_qdeclarativeecmascript::exceptionBindingProducesWarning()
{
    QDeclarativeComponent component(&engine, testFileUrl("exceptionProducesWarning2.qml"));
    QString url = component.url().toString();

    QString warning = component.url().toString() + ":5: Error: JS exception";

    QTest::ignoreMessage(QtWarningMsg, warning.toLatin1().constData());
    MyQmlObject *object = qobject_cast<MyQmlObject*>(component.create());
    QVERIFY(object != 0);
}

static int transientErrorsMsgCount = 0;
static void transientErrorsMsgHandler(QtMsgType, const QMessageLogContext &, const QString &)
{
    ++transientErrorsMsgCount;
}

// Check that transient binding errors are not displayed
void tst_qdeclarativeecmascript::transientErrors()
{
    {
    QDeclarativeComponent component(&engine, testFileUrl("transientErrors.qml"));

    transientErrorsMsgCount = 0;
    QtMessageHandler old = qInstallMessageHandler(transientErrorsMsgHandler);

    QObject *object = component.create();
    QVERIFY(object != 0);

    qInstallMessageHandler(old);

    QCOMPARE(transientErrorsMsgCount, 0);
    }

    // One binding erroring multiple times, but then resolving
    {
    QDeclarativeComponent component(&engine, testFileUrl("transientErrors.2.qml"));

    transientErrorsMsgCount = 0;
    QtMessageHandler old = qInstallMessageHandler(transientErrorsMsgHandler);

    QObject *object = component.create();
    QVERIFY(object != 0);

    qInstallMessageHandler(old);

    QCOMPARE(transientErrorsMsgCount, 0);
    }
}

// Check that errors during shutdown are minimized
void tst_qdeclarativeecmascript::shutdownErrors()
{
    QDeclarativeComponent component(&engine, testFileUrl("shutdownErrors.qml"));
    QObject *object = component.create();
    QVERIFY(object != 0);

    transientErrorsMsgCount = 0;
    QtMessageHandler old = qInstallMessageHandler(transientErrorsMsgHandler);

    delete object;

    qInstallMessageHandler(old);
    QCOMPARE(transientErrorsMsgCount, 0);
}

void tst_qdeclarativeecmascript::compositePropertyType()
{
    QDeclarativeComponent component(&engine, testFileUrl("compositePropertyType.qml"));
    QTest::ignoreMessage(QtDebugMsg, "hello world");
    QObject *object = qobject_cast<QObject *>(component.create());
    delete object;
}

// QTBUG-5759
void tst_qdeclarativeecmascript::jsObject()
{
    QDeclarativeComponent component(&engine, testFileUrl("jsObject.qml"));
    QObject *object = component.create();
    QVERIFY(object != 0);

    QCOMPARE(object->property("test").toInt(), 92);

    delete object;
}

void tst_qdeclarativeecmascript::undefinedResetsProperty()
{
    {
    QDeclarativeComponent component(&engine, testFileUrl("undefinedResetsProperty.qml"));
    QObject *object = component.create();
    QVERIFY(object != 0);

    QCOMPARE(object->property("resettableProperty").toInt(), 92);

    object->setProperty("setUndefined", true);

    QCOMPARE(object->property("resettableProperty").toInt(), 13);

    object->setProperty("setUndefined", false);

    QCOMPARE(object->property("resettableProperty").toInt(), 92);

    delete object;
    }
    {
    QDeclarativeComponent component(&engine, testFileUrl("undefinedResetsProperty.2.qml"));
    QObject *object = component.create();
    QVERIFY(object != 0);

    QCOMPARE(object->property("resettableProperty").toInt(), 19);

    QMetaObject::invokeMethod(object, "doReset");

    QCOMPARE(object->property("resettableProperty").toInt(), 13);

    delete object;
    }
}

// QTBUG-6781
void tst_qdeclarativeecmascript::bug1()
{
    QDeclarativeComponent component(&engine, testFileUrl("bug.1.qml"));
    QObject *object = component.create();
    QVERIFY(object != 0);

    QCOMPARE(object->property("test").toInt(), 14);

    object->setProperty("a", 11);

    QCOMPARE(object->property("test").toInt(), 3);

    object->setProperty("b", true);

    QCOMPARE(object->property("test").toInt(), 9);

    delete object;
}

void tst_qdeclarativeecmascript::bug2()
{
    QDeclarativeComponent component(&engine);
    component.setData("import Qt.test 1.0;\nQPlainTextEdit { width: 100 }", QUrl());

    QObject *object = component.create();
    QVERIFY(object != 0);

    delete object;
}

// Don't crash in createObject when the component has errors.
void tst_qdeclarativeecmascript::dynamicCreationCrash()
{
    QDeclarativeComponent component(&engine, testFileUrl("dynamicCreation.qml"));
    MyQmlObject *object = qobject_cast<MyQmlObject*>(component.create());
    QVERIFY(object != 0);

    QTest::ignoreMessage(QtWarningMsg, "QDeclarativeComponent: Component is not ready");
    QMetaObject::invokeMethod(object, "dontCrash");
    QObject *created = object->objectProperty();
    QVERIFY(created == 0);
}

//QTBUG-9367
void tst_qdeclarativeecmascript::regExpBug()
{
    QDeclarativeComponent component(&engine, testFileUrl("regExp.qml"));
    MyQmlObject *object = qobject_cast<MyQmlObject*>(component.create());
    QVERIFY(object != 0);
    QCOMPARE(object->regExp().pattern(), QLatin1String("[a-zA-z]"));
}

void tst_qdeclarativeecmascript::callQtInvokables()
{
    MyInvokableObject o;

    QDeclarativeEngine qmlengine;
    QDeclarativeEnginePrivate *ep = QDeclarativeEnginePrivate::get(&qmlengine);
    QScriptEngine *engine = &ep->scriptEngine;

    QStringList names; QList<QScriptValue> values;
    names << QLatin1String("object"); values << ep->objectClass->newQObject(&o);
    names << QLatin1String("undefined"); values << engine->undefinedValue();

    ep->globalClass->explicitSetProperty(names, values);

    // Non-existent methods
    o.reset();
    QCOMPARE(engine->evaluate("object.method_nonexistent()").isError(), true);
    QCOMPARE(o.error(), false);
    QCOMPARE(o.invoked(), -1);
    QCOMPARE(o.actuals().count(), 0);

    o.reset();
    QCOMPARE(engine->evaluate("object.method_nonexistent(10, 11)").isError(), true);
    QCOMPARE(o.error(), false);
    QCOMPARE(o.invoked(), -1);
    QCOMPARE(o.actuals().count(), 0);

    // Insufficient arguments
    o.reset();
    QCOMPARE(engine->evaluate("object.method_int()").isError(), true);
    QCOMPARE(o.error(), false);
    QCOMPARE(o.invoked(), -1);
    QCOMPARE(o.actuals().count(), 0);

    o.reset();
    QCOMPARE(engine->evaluate("object.method_intint(10)").isError(), true);
    QCOMPARE(o.error(), false);
    QCOMPARE(o.invoked(), -1);
    QCOMPARE(o.actuals().count(), 0);

    // Excessive arguments
    o.reset();
    QCOMPARE(engine->evaluate("object.method_int(10, 11)").isUndefined(), true);
    QCOMPARE(o.error(), false);
    QCOMPARE(o.invoked(), 8);
    QCOMPARE(o.actuals().count(), 1);
    QCOMPARE(o.actuals().at(0), QVariant(10));

    o.reset();
    QCOMPARE(engine->evaluate("object.method_intint(10, 11, 12)").isUndefined(), true);
    QCOMPARE(o.error(), false);
    QCOMPARE(o.invoked(), 9);
    QCOMPARE(o.actuals().count(), 2);
    QCOMPARE(o.actuals().at(0), QVariant(10));
    QCOMPARE(o.actuals().at(1), QVariant(11));

    // Test return types
    o.reset();
    QCOMPARE(engine->evaluate("object.method_NoArgs()").isUndefined(), true);
    QCOMPARE(o.error(), false);
    QCOMPARE(o.invoked(), 0);
    QCOMPARE(o.actuals().count(), 0);

    o.reset();
    QVERIFY(engine->evaluate("object.method_NoArgs_int()").strictlyEquals(QScriptValue(engine, 6)));
    QCOMPARE(o.error(), false);
    QCOMPARE(o.invoked(), 1);
    QCOMPARE(o.actuals().count(), 0);

    o.reset();
    QVERIFY(engine->evaluate("object.method_NoArgs_real()").strictlyEquals(QScriptValue(engine, 19.75)));
    QCOMPARE(o.error(), false);
    QCOMPARE(o.invoked(), 2);
    QCOMPARE(o.actuals().count(), 0);

    o.reset();
    {
    QScriptValue ret = engine->evaluate("object.method_NoArgs_QPointF()");
    QCOMPARE(ret.toVariant(), QVariant(QPointF(123, 4.5)));
    QCOMPARE(o.error(), false);
    QCOMPARE(o.invoked(), 3);
    QCOMPARE(o.actuals().count(), 0);
    }

    o.reset();
    {
    QScriptValue ret = engine->evaluate("object.method_NoArgs_QObject()");
    QVERIFY(ret.isQObject());
    QCOMPARE(ret.toQObject(), (QObject *)&o);
    QCOMPARE(o.error(), false);
    QCOMPARE(o.invoked(), 4);
    QCOMPARE(o.actuals().count(), 0);
    }

    o.reset();
    QCOMPARE(engine->evaluate("object.method_NoArgs_unknown()").isUndefined(), true);
    QCOMPARE(o.error(), false);
    QCOMPARE(o.invoked(), 5);
    QCOMPARE(o.actuals().count(), 0);

    o.reset();
    {
    QScriptValue ret = engine->evaluate("object.method_NoArgs_QScriptValue()");
    QVERIFY(ret.isString());
    QCOMPARE(ret.toString(), QString("Hello world"));
    QCOMPARE(o.error(), false);
    QCOMPARE(o.invoked(), 6);
    QCOMPARE(o.actuals().count(), 0);
    }

    o.reset();
    QVERIFY(engine->evaluate("object.method_NoArgs_QVariant()").strictlyEquals(QScriptValue(engine, "QML rocks")));
    QCOMPARE(o.error(), false);
    QCOMPARE(o.invoked(), 7);
    QCOMPARE(o.actuals().count(), 0);

    // Test arg types
    o.reset();
    QCOMPARE(engine->evaluate("object.method_int(94)").isUndefined(), true);
    QCOMPARE(o.error(), false);
    QCOMPARE(o.invoked(), 8);
    QCOMPARE(o.actuals().count(), 1);
    QCOMPARE(o.actuals().at(0), QVariant(94));

    o.reset();
    QCOMPARE(engine->evaluate("object.method_int(\"94\")").isUndefined(), true);
    QCOMPARE(o.error(), false);
    QCOMPARE(o.invoked(), 8);
    QCOMPARE(o.actuals().count(), 1);
    QCOMPARE(o.actuals().at(0), QVariant(94));

    o.reset();
    QCOMPARE(engine->evaluate("object.method_int(\"not a number\")").isUndefined(), true);
    QCOMPARE(o.error(), false);
    QCOMPARE(o.invoked(), 8);
    QCOMPARE(o.actuals().count(), 1);
    QCOMPARE(o.actuals().at(0), QVariant(0));

    o.reset();
    QCOMPARE(engine->evaluate("object.method_int(null)").isUndefined(), true);
    QCOMPARE(o.error(), false);
    QCOMPARE(o.invoked(), 8);
    QCOMPARE(o.actuals().count(), 1);
    QCOMPARE(o.actuals().at(0), QVariant(0));

    o.reset();
    QCOMPARE(engine->evaluate("object.method_int(undefined)").isUndefined(), true);
    QCOMPARE(o.error(), false);
    QCOMPARE(o.invoked(), 8);
    QCOMPARE(o.actuals().count(), 1);
    QCOMPARE(o.actuals().at(0), QVariant(0));

    o.reset();
    QCOMPARE(engine->evaluate("object.method_int(object)").isUndefined(), true);
    QCOMPARE(o.error(), false);
    QCOMPARE(o.invoked(), 8);
    QCOMPARE(o.actuals().count(), 1);
    QCOMPARE(o.actuals().at(0), QVariant(0));

    o.reset();
    QCOMPARE(engine->evaluate("object.method_intint(122, 9)").isUndefined(), true);
    QCOMPARE(o.error(), false);
    QCOMPARE(o.invoked(), 9);
    QCOMPARE(o.actuals().count(), 2);
    QCOMPARE(o.actuals().at(0), QVariant(122));
    QCOMPARE(o.actuals().at(1), QVariant(9));

    o.reset();
    QCOMPARE(engine->evaluate("object.method_real(94.3)").isUndefined(), true);
    QCOMPARE(o.error(), false);
    QCOMPARE(o.invoked(), 10);
    QCOMPARE(o.actuals().count(), 1);
    QCOMPARE(o.actuals().at(0), QVariant(94.3));

    o.reset();
    QCOMPARE(engine->evaluate("object.method_real(\"94.3\")").isUndefined(), true);
    QCOMPARE(o.error(), false);
    QCOMPARE(o.invoked(), 10);
    QCOMPARE(o.actuals().count(), 1);
    QCOMPARE(o.actuals().at(0), QVariant(94.3));

    o.reset();
    QCOMPARE(engine->evaluate("object.method_real(\"not a number\")").isUndefined(), true);
    QCOMPARE(o.error(), false);
    QCOMPARE(o.invoked(), 10);
    QCOMPARE(o.actuals().count(), 1);
    QVERIFY(qIsNaN(o.actuals().at(0).toDouble()));

    o.reset();
    QCOMPARE(engine->evaluate("object.method_real(null)").isUndefined(), true);
    QCOMPARE(o.error(), false);
    QCOMPARE(o.invoked(), 10);
    QCOMPARE(o.actuals().count(), 1);
    QCOMPARE(o.actuals().at(0), QVariant(0));

    o.reset();
    QCOMPARE(engine->evaluate("object.method_real(undefined)").isUndefined(), true);
    QCOMPARE(o.error(), false);
    QCOMPARE(o.invoked(), 10);
    QCOMPARE(o.actuals().count(), 1);
    QVERIFY(qIsNaN(o.actuals().at(0).toDouble()));

    o.reset();
    QCOMPARE(engine->evaluate("object.method_real(object)").isUndefined(), true);
    QCOMPARE(o.error(), false);
    QCOMPARE(o.invoked(), 10);
    QCOMPARE(o.actuals().count(), 1);
    QVERIFY(qIsNaN(o.actuals().at(0).toDouble()));

    o.reset();
    QCOMPARE(engine->evaluate("object.method_QString(\"Hello world\")").isUndefined(), true);
    QCOMPARE(o.error(), false);
    QCOMPARE(o.invoked(), 11);
    QCOMPARE(o.actuals().count(), 1);
    QCOMPARE(o.actuals().at(0), QVariant("Hello world"));

    o.reset();
    QCOMPARE(engine->evaluate("object.method_QString(19)").isUndefined(), true);
    QCOMPARE(o.error(), false);
    QCOMPARE(o.invoked(), 11);
    QCOMPARE(o.actuals().count(), 1);
    QCOMPARE(o.actuals().at(0), QVariant("19"));

    o.reset();
    {
    QString expected = "MyInvokableObject(0x" + QString::number((quintptr)&o, 16) + ")";
    QCOMPARE(engine->evaluate("object.method_QString(object)").isUndefined(), true);
    QCOMPARE(o.error(), false);
    QCOMPARE(o.invoked(), 11);
    QCOMPARE(o.actuals().count(), 1);
    QCOMPARE(o.actuals().at(0), QVariant(expected));
    }

    o.reset();
    QCOMPARE(engine->evaluate("object.method_QString(null)").isUndefined(), true);
    QCOMPARE(o.error(), false);
    QCOMPARE(o.invoked(), 11);
    QCOMPARE(o.actuals().count(), 1);
    QCOMPARE(o.actuals().at(0), QVariant(QString()));

    o.reset();
    QCOMPARE(engine->evaluate("object.method_QString(undefined)").isUndefined(), true);
    QCOMPARE(o.error(), false);
    QCOMPARE(o.invoked(), 11);
    QCOMPARE(o.actuals().count(), 1);
    QCOMPARE(o.actuals().at(0), QVariant(QString()));

    o.reset();
    QCOMPARE(engine->evaluate("object.method_QPointF(0)").isUndefined(), true);
    QCOMPARE(o.error(), false);
    QCOMPARE(o.invoked(), 12);
    QCOMPARE(o.actuals().count(), 1);
    QCOMPARE(o.actuals().at(0), QVariant(QPointF()));

    o.reset();
    QCOMPARE(engine->evaluate("object.method_QPointF(null)").isUndefined(), true);
    QCOMPARE(o.error(), false);
    QCOMPARE(o.invoked(), 12);
    QCOMPARE(o.actuals().count(), 1);
    QCOMPARE(o.actuals().at(0), QVariant(QPointF()));

    o.reset();
    QCOMPARE(engine->evaluate("object.method_QPointF(undefined)").isUndefined(), true);
    QCOMPARE(o.error(), false);
    QCOMPARE(o.invoked(), 12);
    QCOMPARE(o.actuals().count(), 1);
    QCOMPARE(o.actuals().at(0), QVariant(QPointF()));

    o.reset();
    QCOMPARE(engine->evaluate("object.method_QPointF(object)").isUndefined(), true);
    QCOMPARE(o.error(), false);
    QCOMPARE(o.invoked(), 12);
    QCOMPARE(o.actuals().count(), 1);
    QCOMPARE(o.actuals().at(0), QVariant(QPointF()));

    o.reset();
    QCOMPARE(engine->evaluate("object.method_QPointF(object.method_get_QPointF())").isUndefined(), true);
    QCOMPARE(o.error(), false);
    QCOMPARE(o.invoked(), 12);
    QCOMPARE(o.actuals().count(), 1);
    QCOMPARE(o.actuals().at(0), QVariant(QPointF(99.3, -10.2)));

    o.reset();
    QCOMPARE(engine->evaluate("object.method_QPointF(object.method_get_QPoint())").isUndefined(), true);
    QCOMPARE(o.error(), false);
    QCOMPARE(o.invoked(), 12);
    QCOMPARE(o.actuals().count(), 1);
    QCOMPARE(o.actuals().at(0), QVariant(QPointF(9, 12)));

    o.reset();
    QCOMPARE(engine->evaluate("object.method_QObject(0)").isUndefined(), true);
    QCOMPARE(o.error(), false);
    QCOMPARE(o.invoked(), 13);
    QCOMPARE(o.actuals().count(), 1);
    QCOMPARE(o.actuals().at(0), qVariantFromValue((QObject *)0));

    o.reset();
    QCOMPARE(engine->evaluate("object.method_QObject(\"Hello world\")").isUndefined(), true);
    QCOMPARE(o.error(), false);
    QCOMPARE(o.invoked(), 13);
    QCOMPARE(o.actuals().count(), 1);
    QCOMPARE(o.actuals().at(0), qVariantFromValue((QObject *)0));

    o.reset();
    QCOMPARE(engine->evaluate("object.method_QObject(null)").isUndefined(), true);
    QCOMPARE(o.error(), false);
    QCOMPARE(o.invoked(), 13);
    QCOMPARE(o.actuals().count(), 1);
    QCOMPARE(o.actuals().at(0), qVariantFromValue((QObject *)0));

    o.reset();
    QCOMPARE(engine->evaluate("object.method_QObject(undefined)").isUndefined(), true);
    QCOMPARE(o.error(), false);
    QCOMPARE(o.invoked(), 13);
    QCOMPARE(o.actuals().count(), 1);
    QCOMPARE(o.actuals().at(0), qVariantFromValue((QObject *)0));

    o.reset();
    QCOMPARE(engine->evaluate("object.method_QObject(object)").isUndefined(), true);
    QCOMPARE(o.error(), false);
    QCOMPARE(o.invoked(), 13);
    QCOMPARE(o.actuals().count(), 1);
    QCOMPARE(o.actuals().at(0), qVariantFromValue((QObject *)&o));

    o.reset();
    QCOMPARE(engine->evaluate("object.method_QScriptValue(null)").isUndefined(), true);
    QCOMPARE(o.error(), false);
    QCOMPARE(o.invoked(), 14);
    QCOMPARE(o.actuals().count(), 1);
    QVERIFY(qvariant_cast<QScriptValue>(o.actuals().at(0)).isNull());

    o.reset();
    QCOMPARE(engine->evaluate("object.method_QScriptValue(undefined)").isUndefined(), true);
    QCOMPARE(o.error(), false);
    QCOMPARE(o.invoked(), 14);
    QCOMPARE(o.actuals().count(), 1);
    QVERIFY(qvariant_cast<QScriptValue>(o.actuals().at(0)).isUndefined());

    o.reset();
    QCOMPARE(engine->evaluate("object.method_QScriptValue(19)").isUndefined(), true);
    QCOMPARE(o.error(), false);
    QCOMPARE(o.invoked(), 14);
    QCOMPARE(o.actuals().count(), 1);
    QVERIFY(qvariant_cast<QScriptValue>(o.actuals().at(0)).strictlyEquals(QScriptValue(engine, 19)));

    o.reset();
    QCOMPARE(engine->evaluate("object.method_QScriptValue([19, 20])").isUndefined(), true);
    QCOMPARE(o.error(), false);
    QCOMPARE(o.invoked(), 14);
    QCOMPARE(o.actuals().count(), 1);
    QVERIFY(qvariant_cast<QScriptValue>(o.actuals().at(0)).isArray());

    o.reset();
    QCOMPARE(engine->evaluate("object.method_intQScriptValue(4, null)").isUndefined(), true);
    QCOMPARE(o.error(), false);
    QCOMPARE(o.invoked(), 15);
    QCOMPARE(o.actuals().count(), 2);
    QCOMPARE(o.actuals().at(0), QVariant(4));
    QVERIFY(qvariant_cast<QScriptValue>(o.actuals().at(1)).isNull());

    o.reset();
    QCOMPARE(engine->evaluate("object.method_intQScriptValue(8, undefined)").isUndefined(), true);
    QCOMPARE(o.error(), false);
    QCOMPARE(o.invoked(), 15);
    QCOMPARE(o.actuals().count(), 2);
    QCOMPARE(o.actuals().at(0), QVariant(8));
    QVERIFY(qvariant_cast<QScriptValue>(o.actuals().at(1)).isUndefined());

    o.reset();
    QCOMPARE(engine->evaluate("object.method_intQScriptValue(3, 19)").isUndefined(), true);
    QCOMPARE(o.error(), false);
    QCOMPARE(o.invoked(), 15);
    QCOMPARE(o.actuals().count(), 2);
    QCOMPARE(o.actuals().at(0), QVariant(3));
    QVERIFY(qvariant_cast<QScriptValue>(o.actuals().at(1)).strictlyEquals(QScriptValue(engine, 19)));

    o.reset();
    QCOMPARE(engine->evaluate("object.method_intQScriptValue(44, [19, 20])").isUndefined(), true);
    QCOMPARE(o.error(), false);
    QCOMPARE(o.invoked(), 15);
    QCOMPARE(o.actuals().count(), 2);
    QCOMPARE(o.actuals().at(0), QVariant(44));
    QVERIFY(qvariant_cast<QScriptValue>(o.actuals().at(1)).isArray());

    o.reset();
    QCOMPARE(engine->evaluate("object.method_overload()").isError(), true);
    QCOMPARE(o.error(), false);
    QCOMPARE(o.invoked(), -1);
    QCOMPARE(o.actuals().count(), 0);

    o.reset();
    QCOMPARE(engine->evaluate("object.method_overload(10)").isUndefined(), true);
    QCOMPARE(o.error(), false);
    QCOMPARE(o.invoked(), 16);
    QCOMPARE(o.actuals().count(), 1);
    QCOMPARE(o.actuals().at(0), QVariant(10));

    o.reset();
    QCOMPARE(engine->evaluate("object.method_overload(10, 11)").isUndefined(), true);
    QCOMPARE(o.error(), false);
    QCOMPARE(o.invoked(), 17);
    QCOMPARE(o.actuals().count(), 2);
    QCOMPARE(o.actuals().at(0), QVariant(10));
    QCOMPARE(o.actuals().at(1), QVariant(11));

    o.reset();
    QCOMPARE(engine->evaluate("object.method_overload(\"Hello\")").isUndefined(), true);
    QCOMPARE(o.error(), false);
    QCOMPARE(o.invoked(), 18);
    QCOMPARE(o.actuals().count(), 1);
    QCOMPARE(o.actuals().at(0), QVariant(QString("Hello")));

    o.reset();
    QCOMPARE(engine->evaluate("object.method_with_enum(9)").isUndefined(), true);
    QCOMPARE(o.error(), false);
    QCOMPARE(o.invoked(), 19);
    QCOMPARE(o.actuals().count(), 1);
    QCOMPARE(o.actuals().at(0), QVariant(9));

    o.reset();
    QVERIFY(engine->evaluate("object.method_default(10)").strictlyEquals(QScriptValue(19)));
    QCOMPARE(o.error(), false);
    QCOMPARE(o.invoked(), 20);
    QCOMPARE(o.actuals().count(), 2);
    QCOMPARE(o.actuals().at(0), QVariant(10));
    QCOMPARE(o.actuals().at(1), QVariant(19));

    o.reset();
    QVERIFY(engine->evaluate("object.method_default(10, 13)").strictlyEquals(QScriptValue(13)));
    QCOMPARE(o.error(), false);
    QCOMPARE(o.invoked(), 20);
    QCOMPARE(o.actuals().count(), 2);
    QCOMPARE(o.actuals().at(0), QVariant(10));
    QCOMPARE(o.actuals().at(1), QVariant(13));

    o.reset();
    QCOMPARE(engine->evaluate("object.method_inherited(9)").isUndefined(), true);
    QCOMPARE(o.error(), false);
    QCOMPARE(o.invoked(), -3);
    QCOMPARE(o.actuals().count(), 1);
    QCOMPARE(o.actuals().at(0), QVariant(9));

    o.reset();
    QCOMPARE(engine->evaluate("object.method_QVariant(9)").isUndefined(), true);
    QCOMPARE(o.error(), false);
    QCOMPARE(o.invoked(), 21);
    QCOMPARE(o.actuals().count(), 2);
    QCOMPARE(o.actuals().at(0), QVariant(9));
    QCOMPARE(o.actuals().at(1), QVariant());

    o.reset();
    QCOMPARE(engine->evaluate("object.method_QVariant(\"Hello\", \"World\")").isUndefined(), true);
    QCOMPARE(o.error(), false);
    QCOMPARE(o.invoked(), 21);
    QCOMPARE(o.actuals().count(), 2);
    QCOMPARE(o.actuals().at(0), QVariant(QString("Hello")));
    QCOMPARE(o.actuals().at(1), QVariant(QString("World")));
}

// QTBUG-13047 (check that you can pass registered object types as args)
void tst_qdeclarativeecmascript::invokableObjectArg()
{
    QDeclarativeComponent component(&engine, testFileUrl("invokableObjectArg.qml"));

    QObject *o = component.create();
    QVERIFY(o);
    MyQmlObject *qmlobject = qobject_cast<MyQmlObject *>(o);
    QVERIFY(qmlobject);
    QCOMPARE(qmlobject->myinvokableObject, qmlobject);

    delete o;
}

// QTBUG-13047 (check that you can return registered object types from methods)
void tst_qdeclarativeecmascript::invokableObjectRet()
{
    QDeclarativeComponent component(&engine, testFileUrl("invokableObjectRet.qml"));

    QObject *o = component.create();
    QVERIFY(o);
    QCOMPARE(o->property("test").toBool(), true);
    delete o;
}

// QTBUG-5675
void tst_qdeclarativeecmascript::listToVariant()
{
    QDeclarativeComponent component(&engine, testFileUrl("listToVariant.qml"));

    MyQmlContainer container;

    QDeclarativeContext context(engine.rootContext());
    context.setContextObject(&container);

    QObject *object = component.create(&context);
    QVERIFY(object != 0);

    QVariant v = object->property("test");
    QCOMPARE(v.userType(), qMetaTypeId<QDeclarativeListReference>());
    QVERIFY(qvariant_cast<QDeclarativeListReference>(v).object() == &container);

    delete object;
}

// QTBUG-7957
void tst_qdeclarativeecmascript::multiEngineObject()
{
    MyQmlObject obj;
    obj.setStringProperty("Howdy planet");

    QDeclarativeEngine e1;
    e1.rootContext()->setContextProperty("thing", &obj);
    QDeclarativeComponent c1(&e1, testFileUrl("multiEngineObject.qml"));

    QDeclarativeEngine e2;
    e2.rootContext()->setContextProperty("thing", &obj);
    QDeclarativeComponent c2(&e2, testFileUrl("multiEngineObject.qml"));

    QObject *o1 = c1.create();
    QObject *o2 = c2.create();

    QCOMPARE(o1->property("test").toString(), QString("Howdy planet"));
    QCOMPARE(o2->property("test").toString(), QString("Howdy planet"));

    delete o2;
    delete o1;
}

// Test that references to QObjects are cleanup when the object is destroyed
void tst_qdeclarativeecmascript::deletedObject()
{
    QDeclarativeComponent component(&engine, testFileUrl("deletedObject.qml"));

    QObject *object = component.create();

    QCOMPARE(object->property("test1").toBool(), true);
    QCOMPARE(object->property("test2").toBool(), true);
    QCOMPARE(object->property("test3").toBool(), true);
    QCOMPARE(object->property("test4").toBool(), true);

    delete object;
}

void tst_qdeclarativeecmascript::attachedPropertyScope()
{
    QDeclarativeComponent component(&engine, testFileUrl("attachedPropertyScope.qml"));

    QObject *object = component.create();
    QVERIFY(object != 0);

    MyQmlAttachedObject *attached =
        qobject_cast<MyQmlAttachedObject *>(qmlAttachedPropertiesObject<MyQmlObject>(object));
    QVERIFY(attached != 0);

    QCOMPARE(object->property("value2").toInt(), 0);

    attached->emitMySignal();

    QCOMPARE(object->property("value2").toInt(), 9);

    delete object;
}

void tst_qdeclarativeecmascript::scriptConnect()
{
    {
        QDeclarativeComponent component(&engine, testFileUrl("scriptConnect.1.qml"));

        MyQmlObject *object = qobject_cast<MyQmlObject *>(component.create());
        QVERIFY(object != 0);

        QCOMPARE(object->property("test").toBool(), false);
        emit object->argumentSignal(19, "Hello world!", 10.25, MyQmlObject::EnumValue4, Qt::RightButton);
        QCOMPARE(object->property("test").toBool(), true);

        delete object;
    }

    {
        QDeclarativeComponent component(&engine, testFileUrl("scriptConnect.2.qml"));

        MyQmlObject *object = qobject_cast<MyQmlObject *>(component.create());
        QVERIFY(object != 0);

        QCOMPARE(object->property("test").toBool(), false);
        emit object->argumentSignal(19, "Hello world!", 10.25, MyQmlObject::EnumValue4, Qt::RightButton);
        QCOMPARE(object->property("test").toBool(), true);

        delete object;
    }

    {
        QDeclarativeComponent component(&engine, testFileUrl("scriptConnect.3.qml"));

        MyQmlObject *object = qobject_cast<MyQmlObject *>(component.create());
        QVERIFY(object != 0);

        QCOMPARE(object->property("test").toBool(), false);
        emit object->argumentSignal(19, "Hello world!", 10.25, MyQmlObject::EnumValue4, Qt::RightButton);
        QCOMPARE(object->property("test").toBool(), true);

        delete object;
    }

    {
        QDeclarativeComponent component(&engine, testFileUrl("scriptConnect.4.qml"));

        MyQmlObject *object = qobject_cast<MyQmlObject *>(component.create());
        QVERIFY(object != 0);

        QCOMPARE(object->methodCalled(), false);
        emit object->argumentSignal(19, "Hello world!", 10.25, MyQmlObject::EnumValue4, Qt::RightButton);
        QCOMPARE(object->methodCalled(), true);

        delete object;
    }

    {
        QDeclarativeComponent component(&engine, testFileUrl("scriptConnect.5.qml"));

        MyQmlObject *object = qobject_cast<MyQmlObject *>(component.create());
        QVERIFY(object != 0);

        QCOMPARE(object->methodCalled(), false);
        emit object->argumentSignal(19, "Hello world!", 10.25, MyQmlObject::EnumValue4, Qt::RightButton);
        QCOMPARE(object->methodCalled(), true);

        delete object;
    }

    {
        QDeclarativeComponent component(&engine, testFileUrl("scriptConnect.6.qml"));

        MyQmlObject *object = qobject_cast<MyQmlObject *>(component.create());
        QVERIFY(object != 0);

        QCOMPARE(object->property("test").toInt(), 0);
        emit object->argumentSignal(19, "Hello world!", 10.25, MyQmlObject::EnumValue4, Qt::RightButton);
        QCOMPARE(object->property("test").toInt(), 2);

        delete object;
    }
}

void tst_qdeclarativeecmascript::scriptDisconnect()
{
    {
        QDeclarativeComponent component(&engine, testFileUrl("scriptDisconnect.1.qml"));

        MyQmlObject *object = qobject_cast<MyQmlObject *>(component.create());
        QVERIFY(object != 0);

        QCOMPARE(object->property("test").toInt(), 0);
        emit object->argumentSignal(19, "Hello world!", 10.25, MyQmlObject::EnumValue4, Qt::RightButton);
        QCOMPARE(object->property("test").toInt(), 1);
        emit object->argumentSignal(19, "Hello world!", 10.25, MyQmlObject::EnumValue4, Qt::RightButton);
        QCOMPARE(object->property("test").toInt(), 2);
        emit object->basicSignal();
        QCOMPARE(object->property("test").toInt(), 2);
        emit object->argumentSignal(19, "Hello world!", 10.25, MyQmlObject::EnumValue4, Qt::RightButton);
        QCOMPARE(object->property("test").toInt(), 2);

        delete object;
    }

    {
        QDeclarativeComponent component(&engine, testFileUrl("scriptDisconnect.2.qml"));

        MyQmlObject *object = qobject_cast<MyQmlObject *>(component.create());
        QVERIFY(object != 0);

        QCOMPARE(object->property("test").toInt(), 0);
        emit object->argumentSignal(19, "Hello world!", 10.25, MyQmlObject::EnumValue4, Qt::RightButton);
        QCOMPARE(object->property("test").toInt(), 1);
        emit object->argumentSignal(19, "Hello world!", 10.25, MyQmlObject::EnumValue4, Qt::RightButton);
        QCOMPARE(object->property("test").toInt(), 2);
        emit object->basicSignal();
        QCOMPARE(object->property("test").toInt(), 2);
        emit object->argumentSignal(19, "Hello world!", 10.25, MyQmlObject::EnumValue4, Qt::RightButton);
        QCOMPARE(object->property("test").toInt(), 2);

        delete object;
    }

    {
        QDeclarativeComponent component(&engine, testFileUrl("scriptDisconnect.3.qml"));

        MyQmlObject *object = qobject_cast<MyQmlObject *>(component.create());
        QVERIFY(object != 0);

        QCOMPARE(object->property("test").toInt(), 0);
        emit object->argumentSignal(19, "Hello world!", 10.25, MyQmlObject::EnumValue4, Qt::RightButton);
        QCOMPARE(object->property("test").toInt(), 1);
        emit object->argumentSignal(19, "Hello world!", 10.25, MyQmlObject::EnumValue4, Qt::RightButton);
        QCOMPARE(object->property("test").toInt(), 2);
        emit object->basicSignal();
        QCOMPARE(object->property("test").toInt(), 2);
        emit object->argumentSignal(19, "Hello world!", 10.25, MyQmlObject::EnumValue4, Qt::RightButton);
        QCOMPARE(object->property("test").toInt(), 3);

        delete object;
    }
    {
        QDeclarativeComponent component(&engine, testFileUrl("scriptDisconnect.4.qml"));

        MyQmlObject *object = qobject_cast<MyQmlObject *>(component.create());
        QVERIFY(object != 0);

        QCOMPARE(object->property("test").toInt(), 0);
        emit object->argumentSignal(19, "Hello world!", 10.25, MyQmlObject::EnumValue4, Qt::RightButton);
        QCOMPARE(object->property("test").toInt(), 1);
        emit object->argumentSignal(19, "Hello world!", 10.25, MyQmlObject::EnumValue4, Qt::RightButton);
        QCOMPARE(object->property("test").toInt(), 2);
        emit object->basicSignal();
        QCOMPARE(object->property("test").toInt(), 2);
        emit object->argumentSignal(19, "Hello world!", 10.25, MyQmlObject::EnumValue4, Qt::RightButton);
        QCOMPARE(object->property("test").toInt(), 3);

        delete object;
    }
}

class OwnershipObject : public QObject
{
    Q_OBJECT
public:
    OwnershipObject() { object = new QObject; }

    QPointer<QObject> object;

public slots:
    QObject *getObject() { return object; }
};

void tst_qdeclarativeecmascript::ownership()
{
    OwnershipObject own;
    QDeclarativeContext *context = new QDeclarativeContext(engine.rootContext());
    context->setContextObject(&own);

    {
        QDeclarativeComponent component(&engine, testFileUrl("ownership.qml"));

        QVERIFY(own.object != 0);

        QObject *object = component.create(context);
        QDeclarativeEnginePrivate::getScriptEngine(&engine)->collectGarbage();

        QCoreApplication::sendPostedEvents(0, QEvent::DeferredDelete);

        QVERIFY(own.object == 0);

        delete object;
    }

    own.object = new QObject(&own);

    {
        QDeclarativeComponent component(&engine, testFileUrl("ownership.qml"));

        QVERIFY(own.object != 0);

        QObject *object = component.create(context);
        QDeclarativeEnginePrivate::getScriptEngine(&engine)->collectGarbage();

        QCoreApplication::sendPostedEvents(0, QEvent::DeferredDelete);

        QVERIFY(own.object != 0);

        delete object;
    }
}

class CppOwnershipReturnValue : public QObject
{
    Q_OBJECT
public:
    CppOwnershipReturnValue() : value(0) {}
    ~CppOwnershipReturnValue() { delete value; }

    Q_INVOKABLE QObject *create() {
        value = new QObject;
        QDeclarativeEngine::setObjectOwnership(value, QDeclarativeEngine::CppOwnership);
        return value;
    }

    Q_INVOKABLE MyQmlObject *createQmlObject() {
        MyQmlObject *rv = new MyQmlObject;
        value = rv;
        return rv;
    }

    QPointer<QObject> value;
};

// QTBUG-15695.
// Test setObjectOwnership(CppOwnership) works even when there is no QDeclarativeData
void tst_qdeclarativeecmascript::cppOwnershipReturnValue()
{
    CppOwnershipReturnValue source;

    {
    QDeclarativeEngine engine;
    engine.rootContext()->setContextProperty("source", &source);

    QVERIFY(source.value == 0);

    QDeclarativeComponent component(&engine);
    component.setData("import QtQuick 1.0\nQtObject {\nComponent.onCompleted: { var a = source.create(); }\n}\n", QUrl());

    QObject *object = component.create();

    QVERIFY(object != 0);
    QVERIFY(source.value != 0);

    delete object;
    }

    QCoreApplication::sendPostedEvents(0, QEvent::DeferredDelete);

    QVERIFY(source.value != 0);
}

// QTBUG-15697
void tst_qdeclarativeecmascript::ownershipCustomReturnValue()
{
    CppOwnershipReturnValue source;

    {
    QDeclarativeEngine engine;
    engine.rootContext()->setContextProperty("source", &source);

    QVERIFY(source.value == 0);

    QDeclarativeComponent component(&engine);
    component.setData("import QtQuick 1.0\nQtObject {\nComponent.onCompleted: { var a = source.createQmlObject(); }\n}\n", QUrl());

    QObject *object = component.create();

    QVERIFY(object != 0);
    QVERIFY(source.value != 0);

    delete object;
    }

    QCoreApplication::sendPostedEvents(0, QEvent::DeferredDelete);

    QVERIFY(source.value == 0);
}

class QListQObjectMethodsObject : public QObject
{
    Q_OBJECT
public:
    QListQObjectMethodsObject() {
        m_objects.append(new MyQmlObject());
        m_objects.append(new MyQmlObject());
    }

    ~QListQObjectMethodsObject() {
        qDeleteAll(m_objects);
    }

public slots:
    QList<QObject *> getObjects() { return m_objects; }

private:
    QList<QObject *> m_objects;
};

// Tests that returning a QList<QObject*> from a method works
void tst_qdeclarativeecmascript::qlistqobjectMethods()
{
    QListQObjectMethodsObject obj;
    QDeclarativeContext *context = new QDeclarativeContext(engine.rootContext());
    context->setContextObject(&obj);

    QDeclarativeComponent component(&engine, testFileUrl("qlistqobjectMethods.qml"));

    QObject *object = component.create(context);

    QCOMPARE(object->property("test").toInt(), 2);
    QCOMPARE(object->property("test2").toBool(), true);

    delete object;
}

// QTBUG-9205
void tst_qdeclarativeecmascript::strictlyEquals()
{
    QDeclarativeComponent component(&engine, testFileUrl("strictlyEquals.qml"));

    QObject *object = component.create();
    QVERIFY(object != 0);

    QCOMPARE(object->property("test1").toBool(), true);
    QCOMPARE(object->property("test2").toBool(), true);
    QCOMPARE(object->property("test3").toBool(), true);
    QCOMPARE(object->property("test4").toBool(), true);
    QCOMPARE(object->property("test5").toBool(), true);
    QCOMPARE(object->property("test6").toBool(), true);
    QCOMPARE(object->property("test7").toBool(), true);
    QCOMPARE(object->property("test8").toBool(), true);

    delete object;
}

void tst_qdeclarativeecmascript::compiled()
{
    QDeclarativeComponent component(&engine, testFileUrl("compiled.qml"));

    QObject *object = component.create();
    QVERIFY(object != 0);

    QCOMPARE(object->property("test1").toReal(), qreal(15.7));
    QCOMPARE(object->property("test2").toReal(), qreal(-6.7));
    QCOMPARE(object->property("test3").toBool(), true);
    QCOMPARE(object->property("test4").toBool(), false);
    QCOMPARE(object->property("test5").toBool(), false);
    QCOMPARE(object->property("test6").toBool(), true);

    QCOMPARE(object->property("test7").toInt(), 185);
    QCOMPARE(object->property("test8").toInt(), 167);
    QCOMPARE(object->property("test9").toBool(), true);
    QCOMPARE(object->property("test10").toBool(), false);
    QCOMPARE(object->property("test11").toBool(), false);
    QCOMPARE(object->property("test12").toBool(), true);

    QCOMPARE(object->property("test13").toString(), QLatin1String("HelloWorld"));
    QCOMPARE(object->property("test14").toString(), QLatin1String("Hello World"));
    QCOMPARE(object->property("test15").toBool(), false);
    QCOMPARE(object->property("test16").toBool(), true);

    QCOMPARE(object->property("test17").toInt(), 5);
    QCOMPARE(object->property("test18").toReal(), qreal(176));
    QCOMPARE(object->property("test19").toInt(), 7);
    QCOMPARE(object->property("test20").toReal(), qreal(6.7));
    QCOMPARE(object->property("test21").toString(), QLatin1String("6.7"));
    QCOMPARE(object->property("test22").toString(), QLatin1String("!"));
    QCOMPARE(object->property("test23").toBool(), true);
    QCOMPARE(qvariant_cast<QColor>(object->property("test24")), QColor(0x11,0x22,0x33));
    QCOMPARE(qvariant_cast<QColor>(object->property("test25")), QColor(0x11,0x22,0x33,0xAA));

    delete object;
}

// Test that numbers assigned in bindings as strings work consistently
void tst_qdeclarativeecmascript::numberAssignment()
{
    QDeclarativeComponent component(&engine, testFileUrl("numberAssignment.qml"));

    QObject *object = component.create();
    QVERIFY(object != 0);

    QCOMPARE(object->property("test1"), QVariant((qreal)6.7));
    QCOMPARE(object->property("test2"), QVariant((qreal)6.7));
    QCOMPARE(object->property("test2"), QVariant((qreal)6.7));
    QCOMPARE(object->property("test3"), QVariant((qreal)6));
    QCOMPARE(object->property("test4"), QVariant((qreal)6));

    QCOMPARE(object->property("test5"), QVariant((int)7));
    QCOMPARE(object->property("test6"), QVariant((int)7));
    QCOMPARE(object->property("test7"), QVariant((int)6));
    QCOMPARE(object->property("test8"), QVariant((int)6));

    QCOMPARE(object->property("test9"), QVariant((unsigned int)7));
    QCOMPARE(object->property("test10"), QVariant((unsigned int)7));
    QCOMPARE(object->property("test11"), QVariant((unsigned int)6));
    QCOMPARE(object->property("test12"), QVariant((unsigned int)6));

    delete object;
}

void tst_qdeclarativeecmascript::propertySplicing()
{
    QDeclarativeComponent component(&engine, testFileUrl("propertySplicing.qml"));

    QObject *object = component.create();
    QVERIFY(object != 0);

    QCOMPARE(object->property("test").toBool(), true);

    delete object;
}

// QTBUG-16683
void tst_qdeclarativeecmascript::signalWithUnknownTypes()
{
    QDeclarativeComponent component(&engine, testFileUrl("signalWithUnknownTypes.qml"));

    MyQmlObject *object = qobject_cast<MyQmlObject *>(component.create());
    QVERIFY(object != 0);

    MyQmlObject::MyType type;
    type.value = 0x8971123;
    emit object->signalWithUnknownType(type);

    MyQmlObject::MyType result = qvariant_cast<MyQmlObject::MyType>(object->variant());

    QCOMPARE(result.value, type.value);


    delete object;
}

// Test that assigning a null object works
// Regressed with: df1788b4dbbb2826ae63f26bdf166342595343f4
void tst_qdeclarativeecmascript::nullObjectBinding()
{
    QDeclarativeComponent component(&engine, testFileUrl("nullObjectBinding.qml"));

    QObject *object = component.create();
    QVERIFY(object != 0);

    QVERIFY(object->property("test") == QVariant::fromValue((QObject *)0));

    delete object;
}

// Test that bindings don't evaluate once the engine has been destroyed
void tst_qdeclarativeecmascript::deletedEngine()
{
    QDeclarativeEngine *engine = new QDeclarativeEngine;
    QDeclarativeComponent component(engine, testFileUrl("deletedEngine.qml"));

    QObject *object = component.create();
    QVERIFY(object != 0);

    QCOMPARE(object->property("a").toInt(), 39);
    object->setProperty("b", QVariant(9));
    QCOMPARE(object->property("a").toInt(), 117);

    delete engine;

    QCOMPARE(object->property("a").toInt(), 117);
    object->setProperty("b", QVariant(10));
    QCOMPARE(object->property("a").toInt(), 117);

    delete object;
}

// Test the crashing part of QTBUG-9705
void tst_qdeclarativeecmascript::libraryScriptAssert()
{
    QDeclarativeComponent component(&engine, testFileUrl("libraryScriptAssert.qml"));

    QObject *object = component.create();
    QVERIFY(object != 0);

    delete object;
}

void tst_qdeclarativeecmascript::variantsAssignedUndefined()
{
    QDeclarativeComponent component(&engine, testFileUrl("variantsAssignedUndefined.qml"));

    QObject *object = component.create();
    QVERIFY(object != 0);

    QCOMPARE(object->property("test1").toInt(), 10);
    QCOMPARE(object->property("test2").toInt(), 11);

    object->setProperty("runTest", true);

    QCOMPARE(object->property("test1"), QVariant());
    QCOMPARE(object->property("test2"), QVariant());


    delete object;
}

void tst_qdeclarativeecmascript::qtbug_9792()
{
    QDeclarativeComponent component(&engine, testFileUrl("qtbug_9792.qml"));

    QDeclarativeContext *context = new QDeclarativeContext(engine.rootContext());

    MyQmlObject *object = qobject_cast<MyQmlObject*>(component.create(context));
    QVERIFY(object != 0);

    QTest::ignoreMessage(QtDebugMsg, "Hello world!");
    object->basicSignal();

    delete context;

    transientErrorsMsgCount = 0;
    QtMessageHandler old = qInstallMessageHandler(transientErrorsMsgHandler);

    object->basicSignal();

    qInstallMessageHandler(old);

    QCOMPARE(transientErrorsMsgCount, 0);

    delete object;
}

// Verifies that QDeclarativeGuard<>s used in the vmemetaobject are cleaned correctly
void tst_qdeclarativeecmascript::qtcreatorbug_1289()
{
    QDeclarativeComponent component(&engine, testFileUrl("qtcreatorbug_1289.qml"));

    QObject *o = component.create();
    QVERIFY(o != 0);

    QObject *nested = qvariant_cast<QObject *>(o->property("object"));
    QVERIFY(nested != 0);

    QVERIFY(qvariant_cast<QObject *>(nested->property("nestedObject")) == o);

    delete nested;
    nested = qvariant_cast<QObject *>(o->property("object"));
    QVERIFY(nested == 0);

    // If the bug is present, the next line will crash
    delete o;
}

// Test that we shut down without stupid warnings
void tst_qdeclarativeecmascript::noSpuriousWarningsAtShutdown()
{
    {
    QDeclarativeComponent component(&engine, testFileUrl("noSpuriousWarningsAtShutdown.qml"));

    QObject *o = component.create();

    transientErrorsMsgCount = 0;
    QtMessageHandler old = qInstallMessageHandler(transientErrorsMsgHandler);

    delete o;

    qInstallMessageHandler(old);

    QCOMPARE(transientErrorsMsgCount, 0);
    }


    {
    QDeclarativeComponent component(&engine, testFileUrl("noSpuriousWarningsAtShutdown.2.qml"));

    QObject *o = component.create();

    transientErrorsMsgCount = 0;
    QtMessageHandler old = qInstallMessageHandler(transientErrorsMsgHandler);

    delete o;

    qInstallMessageHandler(old);

    QCOMPARE(transientErrorsMsgCount, 0);
    }
}

void tst_qdeclarativeecmascript::canAssignNullToQObject()
{
    {
    QDeclarativeComponent component(&engine, testFileUrl("canAssignNullToQObject.1.qml"));

    MyQmlObject *o = qobject_cast<MyQmlObject *>(component.create());
    QVERIFY(o != 0);

    QVERIFY(o->objectProperty() != 0);

    o->setProperty("runTest", true);

    QVERIFY(o->objectProperty() == 0);

    delete o;
    }

    {
    QDeclarativeComponent component(&engine, testFileUrl("canAssignNullToQObject.2.qml"));

    MyQmlObject *o = qobject_cast<MyQmlObject *>(component.create());
    QVERIFY(o != 0);

    QVERIFY(o->objectProperty() == 0);

    delete o;
    }
}

void tst_qdeclarativeecmascript::functionAssignment_fromBinding()
{
    QDeclarativeComponent component(&engine, testFileUrl("functionAssignment.1.qml"));

    QString url = component.url().toString();
    QString warning = url + ":4: Unable to assign a function to a property.";
    QTest::ignoreMessage(QtWarningMsg, warning.toLatin1().constData());

    MyQmlObject *o = qobject_cast<MyQmlObject *>(component.create());
    QVERIFY(o != 0);

    QVERIFY(!o->property("a").isValid());

    delete o;
}

void tst_qdeclarativeecmascript::functionAssignment_fromJS()
{
    QFETCH(QString, triggerProperty);

    QDeclarativeComponent component(&engine, testFileUrl("functionAssignment.2.qml"));
    QVERIFY2(component.errorString().isEmpty(), qPrintable(component.errorString()));

    MyQmlObject *o = qobject_cast<MyQmlObject *>(component.create());
    QVERIFY(o != 0);
    QVERIFY(!o->property("a").isValid());

    o->setProperty("aNumber", QVariant(5));
    o->setProperty(triggerProperty.toUtf8().constData(), true);
    QCOMPARE(o->property("a"), QVariant(50));

    o->setProperty("aNumber", QVariant(10));
    QCOMPARE(o->property("a"), QVariant(100));

    delete o;
}

void tst_qdeclarativeecmascript::functionAssignment_fromJS_data()
{
    QTest::addColumn<QString>("triggerProperty");

    QTest::newRow("assign to property") << "assignToProperty";
    QTest::newRow("assign to property, from JS file") << "assignToPropertyFromJsFile";

    QTest::newRow("assign to value type") << "assignToValueType";

    QTest::newRow("use 'this'") << "assignWithThis";
    QTest::newRow("use 'this' from JS file") << "assignWithThisFromJsFile";
}

void tst_qdeclarativeecmascript::functionAssignmentfromJS_invalid()
{
    QDeclarativeComponent component(&engine, testFileUrl("functionAssignment.2.qml"));
    QVERIFY2(component.errorString().isEmpty(), qPrintable(component.errorString()));

    MyQmlObject *o = qobject_cast<MyQmlObject *>(component.create());
    QVERIFY(o != 0);
    QVERIFY(!o->property("a").isValid());

    o->setProperty("assignFuncWithoutReturn", true);
    QVERIFY(!o->property("a").isValid());

    QString url = component.url().toString();
    QString warning = url + ":63: Unable to assign QString to int";
    QTest::ignoreMessage(QtWarningMsg, warning.toLatin1().constData());
    o->setProperty("assignWrongType", true);

    warning = url + ":70: Unable to assign QString to int";
    QTest::ignoreMessage(QtWarningMsg, warning.toLatin1().constData());
    o->setProperty("assignWrongTypeToValueType", true);

    delete o;
}

void tst_qdeclarativeecmascript::eval()
{
    QDeclarativeComponent component(&engine, testFileUrl("eval.qml"));

    QObject *o = component.create();
    QVERIFY(o != 0);

    QCOMPARE(o->property("test1").toBool(), true);
    QCOMPARE(o->property("test2").toBool(), true);
    QCOMPARE(o->property("test3").toBool(), true);
    QCOMPARE(o->property("test4").toBool(), true);
    QCOMPARE(o->property("test5").toBool(), true);

    delete o;
}

void tst_qdeclarativeecmascript::function()
{
    QDeclarativeComponent component(&engine, testFileUrl("function.qml"));

    QObject *o = component.create();
    QVERIFY(o != 0);

    QCOMPARE(o->property("test1").toBool(), true);
    QCOMPARE(o->property("test2").toBool(), true);
    QCOMPARE(o->property("test3").toBool(), true);

    delete o;
}

// Test the "Qt.include" method
void tst_qdeclarativeecmascript::include()
{
    // Non-library relative include
    {
    QDeclarativeComponent component(&engine, testFileUrl("include.qml"));
    QObject *o = component.create();
    QVERIFY(o != 0);

    QCOMPARE(o->property("test0").toInt(), 99);
    QCOMPARE(o->property("test1").toBool(), true);
    QCOMPARE(o->property("test2").toBool(), true);
    QCOMPARE(o->property("test2_1").toBool(), true);
    QCOMPARE(o->property("test3").toBool(), true);
    QCOMPARE(o->property("test3_1").toBool(), true);

    delete o;
    }

    // Library relative include
    {
    QDeclarativeComponent component(&engine, testFileUrl("include_shared.qml"));
    QObject *o = component.create();
    QVERIFY(o != 0);

    QCOMPARE(o->property("test0").toInt(), 99);
    QCOMPARE(o->property("test1").toBool(), true);
    QCOMPARE(o->property("test2").toBool(), true);
    QCOMPARE(o->property("test2_1").toBool(), true);
    QCOMPARE(o->property("test3").toBool(), true);
    QCOMPARE(o->property("test3_1").toBool(), true);

    delete o;
    }

    // Callback
    {
    QDeclarativeComponent component(&engine, testFileUrl("include_callback.qml"));
    QObject *o = component.create();
    QVERIFY(o != 0);

    QCOMPARE(o->property("test1").toBool(), true);
    QCOMPARE(o->property("test2").toBool(), true);
    QCOMPARE(o->property("test3").toBool(), true);
    QCOMPARE(o->property("test4").toBool(), true);
    QCOMPARE(o->property("test5").toBool(), true);
    QCOMPARE(o->property("test6").toBool(), true);

    delete o;
    }

    // Including file with ".pragma library"
    {
    QDeclarativeComponent component(&engine, testFileUrl("include_pragma.qml"));
    QObject *o = component.create();
    QVERIFY(o != 0);
    QCOMPARE(o->property("test1").toInt(), 100);

    delete o;
    }

    // Remote - success
    {
    TestHTTPServer server(8111);
    QVERIFY(server.isValid());
    server.serveDirectory(dataDirectory());

    QDeclarativeComponent component(&engine, testFileUrl("include_remote.qml"));
    QObject *o = component.create();
    QVERIFY(o != 0);

    QTRY_VERIFY(o->property("done").toBool() == true);
    QTRY_VERIFY(o->property("done2").toBool() == true);

    QCOMPARE(o->property("test1").toBool(), true);
    QCOMPARE(o->property("test2").toBool(), true);
    QCOMPARE(o->property("test3").toBool(), true);
    QCOMPARE(o->property("test4").toBool(), true);
    QCOMPARE(o->property("test5").toBool(), true);

    QCOMPARE(o->property("test6").toBool(), true);
    QCOMPARE(o->property("test7").toBool(), true);
    QCOMPARE(o->property("test8").toBool(), true);
    QCOMPARE(o->property("test9").toBool(), true);
    QCOMPARE(o->property("test10").toBool(), true);

    delete o;
    }

    // Remote - error
    {
    TestHTTPServer server(8111);
    QVERIFY(server.isValid());
    server.serveDirectory(dataDirectory());

    QDeclarativeComponent component(&engine, testFileUrl("include_remote_missing.qml"));
    QObject *o = component.create();
    QVERIFY(o != 0);

    QTRY_VERIFY(o->property("done").toBool() == true);

    QCOMPARE(o->property("test1").toBool(), true);
    QCOMPARE(o->property("test2").toBool(), true);
    QCOMPARE(o->property("test3").toBool(), true);

    delete o;
    }
}

void tst_qdeclarativeecmascript::qtbug_10696()
{
    QDeclarativeComponent component(&engine, testFileUrl("qtbug_10696.qml"));
    QObject *o = component.create();
    QVERIFY(o != 0);
    delete o;
}

void tst_qdeclarativeecmascript::qtbug_11606()
{
    QDeclarativeComponent component(&engine, testFileUrl("qtbug_11606.qml"));
    QObject *o = component.create();
    QVERIFY(o != 0);
    QCOMPARE(o->property("test").toBool(), true);
    delete o;
}

void tst_qdeclarativeecmascript::qtbug_11600()
{
    QDeclarativeComponent component(&engine, testFileUrl("qtbug_11600.qml"));
    QObject *o = component.create();
    QVERIFY(o != 0);
    QCOMPARE(o->property("test").toBool(), true);
    delete o;
}

// Reading and writing non-scriptable properties should fail
void tst_qdeclarativeecmascript::nonscriptable()
{
    QDeclarativeComponent component(&engine, testFileUrl("nonscriptable.qml"));
    QObject *o = component.create();
    QVERIFY(o != 0);
    QCOMPARE(o->property("readOk").toBool(), true);
    QCOMPARE(o->property("writeOk").toBool(), true);
    delete o;
}

// deleteLater() should not be callable from QML
void tst_qdeclarativeecmascript::deleteLater()
{
    QDeclarativeComponent component(&engine, testFileUrl("deleteLater.qml"));
    QObject *o = component.create();
    QVERIFY(o != 0);
    QCOMPARE(o->property("test").toBool(), true);
    delete o;
}

void tst_qdeclarativeecmascript::in()
{
    QDeclarativeComponent component(&engine, testFileUrl("in.qml"));
    QObject *o = component.create();
    QVERIFY(o != 0);
    QCOMPARE(o->property("test1").toBool(), true);
    QCOMPARE(o->property("test2").toBool(), true);
    delete o;
}

void tst_qdeclarativeecmascript::sharedAttachedObject()
{
    QDeclarativeComponent component(&engine, testFileUrl("sharedAttachedObject.qml"));
    QObject *o = component.create();
    QVERIFY(o != 0);
    QCOMPARE(o->property("test1").toBool(), true);
    QCOMPARE(o->property("test2").toBool(), true);
    delete o;
}

// QTBUG-13999
void tst_qdeclarativeecmascript::objectName()
{
    QDeclarativeComponent component(&engine, testFileUrl("objectName.qml"));
    QObject *o = component.create();
    QVERIFY(o != 0);

    QCOMPARE(o->property("test1").toString(), QString("hello"));
    QCOMPARE(o->property("test2").toString(), QString("ell"));

    o->setObjectName("world");

    QCOMPARE(o->property("test1").toString(), QString("world"));
    QCOMPARE(o->property("test2").toString(), QString("orl"));

    delete o;
}

void tst_qdeclarativeecmascript::writeRemovesBinding()
{
    QDeclarativeComponent component(&engine, testFileUrl("writeRemovesBinding.qml"));
    QObject *o = component.create();
    QVERIFY(o != 0);

    QCOMPARE(o->property("test").toBool(), true);

    delete o;
}

// Test bindings assigned to alias properties actually assign to the alias' target
void tst_qdeclarativeecmascript::aliasBindingsAssignCorrectly()
{
    QDeclarativeComponent component(&engine, testFileUrl("aliasBindingsAssignCorrectly.qml"));
    QObject *o = component.create();
    QVERIFY(o != 0);

    QCOMPARE(o->property("test").toBool(), true);

    delete o;
}

// Test bindings assigned to alias properties override a binding on the target (QTBUG-13719)
void tst_qdeclarativeecmascript::aliasBindingsOverrideTarget()
{
    {
    QDeclarativeComponent component(&engine, testFileUrl("aliasBindingsOverrideTarget.qml"));
    QObject *o = component.create();
    QVERIFY(o != 0);

    QCOMPARE(o->property("test").toBool(), true);

    delete o;
    }

    {
    QDeclarativeComponent component(&engine, testFileUrl("aliasBindingsOverrideTarget.2.qml"));
    QObject *o = component.create();
    QVERIFY(o != 0);

    QCOMPARE(o->property("test").toBool(), true);

    delete o;
    }

    {
    QDeclarativeComponent component(&engine, testFileUrl("aliasBindingsOverrideTarget.3.qml"));
    QObject *o = component.create();
    QVERIFY(o != 0);

    QCOMPARE(o->property("test").toBool(), true);

    delete o;
    }
}

// Test that writes to alias properties override bindings on the alias target (QTBUG-13719)
void tst_qdeclarativeecmascript::aliasWritesOverrideBindings()
{
    {
    QDeclarativeComponent component(&engine, testFileUrl("aliasWritesOverrideBindings.qml"));
    QObject *o = component.create();
    QVERIFY(o != 0);

    QCOMPARE(o->property("test").toBool(), true);

    delete o;
    }

    {
    QDeclarativeComponent component(&engine, testFileUrl("aliasWritesOverrideBindings.2.qml"));
    QObject *o = component.create();
    QVERIFY(o != 0);

    QCOMPARE(o->property("test").toBool(), true);

    delete o;
    }

    {
    QDeclarativeComponent component(&engine, testFileUrl("aliasWritesOverrideBindings.3.qml"));
    QObject *o = component.create();
    QVERIFY(o != 0);

    QCOMPARE(o->property("test").toBool(), true);

    delete o;
    }
}

void tst_qdeclarativeecmascript::revisionErrors()
{
    {
        QDeclarativeComponent component(&engine, testFileUrl("metaobjectRevisionErrors.qml"));
        QString url = component.url().toString();

        QString warning1 = url + ":8: ReferenceError: Can't find variable: prop2";
        QString warning2 = url + ":11: ReferenceError: Can't find variable: prop2";
        QString warning3 = url + ":13: ReferenceError: Can't find variable: method2";

        QTest::ignoreMessage(QtWarningMsg, warning1.toLatin1().constData());
        QTest::ignoreMessage(QtWarningMsg, warning2.toLatin1().constData());
        QTest::ignoreMessage(QtWarningMsg, warning3.toLatin1().constData());
        MyRevisionedClass *object = qobject_cast<MyRevisionedClass *>(component.create());
        QVERIFY(object != 0);
    }
    {
        QDeclarativeComponent component(&engine, testFileUrl("metaobjectRevisionErrors2.qml"));
        QString url = component.url().toString();

        // MyRevisionedSubclass 1.0 uses MyRevisionedClass revision 0
        // method2, prop2 from MyRevisionedClass not available
        // method4, prop4 from MyRevisionedSubclass not available
        QString warning1 = url + ":8: ReferenceError: Can't find variable: prop2";
        QString warning2 = url + ":14: ReferenceError: Can't find variable: prop2";
        QString warning3 = url + ":10: ReferenceError: Can't find variable: prop4";
        QString warning4 = url + ":16: ReferenceError: Can't find variable: prop4";
        QString warning5 = url + ":20: ReferenceError: Can't find variable: method2";

        QTest::ignoreMessage(QtWarningMsg, warning1.toLatin1().constData());
        QTest::ignoreMessage(QtWarningMsg, warning2.toLatin1().constData());
        QTest::ignoreMessage(QtWarningMsg, warning3.toLatin1().constData());
        QTest::ignoreMessage(QtWarningMsg, warning4.toLatin1().constData());
        QTest::ignoreMessage(QtWarningMsg, warning5.toLatin1().constData());
        MyRevisionedClass *object = qobject_cast<MyRevisionedClass *>(component.create());
        QVERIFY(object != 0);
    }
    {
        QDeclarativeComponent component(&engine, testFileUrl("metaobjectRevisionErrors3.qml"));
        QString url = component.url().toString();

        // MyRevisionedSubclass 1.1 uses MyRevisionedClass revision 1
        // All properties/methods available, except MyRevisionedBaseClassUnregistered rev 1
        QString warning1 = url + ":30: ReferenceError: Can't find variable: methodD";
        QString warning2 = url + ":10: ReferenceError: Can't find variable: propD";
        QString warning3 = url + ":20: ReferenceError: Can't find variable: propD";
        QTest::ignoreMessage(QtWarningMsg, warning1.toLatin1().constData());
        QTest::ignoreMessage(QtWarningMsg, warning2.toLatin1().constData());
        QTest::ignoreMessage(QtWarningMsg, warning3.toLatin1().constData());
        MyRevisionedClass *object = qobject_cast<MyRevisionedClass *>(component.create());
        QVERIFY(object != 0);
    }
}

void tst_qdeclarativeecmascript::revision()
{
    {
        QDeclarativeComponent component(&engine, testFileUrl("metaobjectRevision.qml"));
        QString url = component.url().toString();

        MyRevisionedClass *object = qobject_cast<MyRevisionedClass *>(component.create());
        QVERIFY(object != 0);
    }
    {
        QDeclarativeComponent component(&engine, testFileUrl("metaobjectRevision2.qml"));
        QString url = component.url().toString();

        MyRevisionedClass *object = qobject_cast<MyRevisionedClass *>(component.create());
        QVERIFY(object != 0);
    }
    {
        QDeclarativeComponent component(&engine, testFileUrl("metaobjectRevision3.qml"));
        QString url = component.url().toString();

        MyRevisionedClass *object = qobject_cast<MyRevisionedClass *>(component.create());
        QVERIFY(object != 0);
    }
    // Test that non-root classes can resolve revisioned methods
    {
        QDeclarativeComponent component(&engine, testFileUrl("metaobjectRevision4.qml"));

        QObject *object = component.create();
        QVERIFY(object != 0);
        QCOMPARE(object->property("test").toReal(), 11.);
        delete object;
    }
}

// Test for QScriptDeclarativeClass::pushCleanContext()
void tst_qdeclarativeecmascript::pushCleanContext()
{
    QScriptEngine engine;
    engine.globalObject().setProperty("a", 6);
    QCOMPARE(engine.evaluate("a").toInt32(), 6);

    // First confirm pushContext() behaves as we expect
    QScriptValue object = engine.newObject();
    object.setProperty("a", 15);
    QScriptContext *context1 = engine.pushContext();
    context1->pushScope(object);
    QCOMPARE(engine.evaluate("a").toInt32(), 15);
    QScriptValue func0 = engine.evaluate("(function() { return a; })");

    QScriptContext *context2 = engine.pushContext();
    Q_UNUSED(context2);
    QCOMPARE(engine.evaluate("a").toInt32(), 6);
    QScriptValue func1 = engine.evaluate("(function() { return a; })");

    // Now check that pushCleanContext() works
    QScriptDeclarativeClass::pushCleanContext(&engine);
    QCOMPARE(engine.evaluate("a").toInt32(), 6);
    QScriptValue func2 = engine.evaluate("(function() { return a; })");

    engine.popContext();
    QCOMPARE(engine.evaluate("a").toInt32(), 6);

    engine.popContext();
    QCOMPARE(engine.evaluate("a").toInt32(), 15);

    engine.popContext();
    QCOMPARE(engine.evaluate("a").toInt32(), 6);

    // Check that function objects created in these contexts work
    QCOMPARE(func0.call().toInt32(), 15);
    QCOMPARE(func1.call().toInt32(), 6);
    QCOMPARE(func2.call().toInt32(), 6);
}

void tst_qdeclarativeecmascript::realToInt()
{
    QDeclarativeComponent component(&engine, testFileUrl("realToInt.qml"));
    MyQmlObject *object = qobject_cast<MyQmlObject*>(component.create());
    QVERIFY(object != 0);

    QMetaObject::invokeMethod(object, "test1");
    QCOMPARE(object->value(), int(4));
    QMetaObject::invokeMethod(object, "test2");
    QCOMPARE(object->value(), int(8));
}

void tst_qdeclarativeecmascript::qtbug_20648()
{
    QDeclarativeComponent component(&engine, testFileUrl("qtbug_20648.qml"));
    QObject *o = component.create();
    QVERIFY(o != 0);
    QCOMPARE(o->property("test").toInt(), 100);
    delete o;
}

QTEST_MAIN(tst_qdeclarativeecmascript)

#include "tst_qdeclarativeecmascript.moc"
