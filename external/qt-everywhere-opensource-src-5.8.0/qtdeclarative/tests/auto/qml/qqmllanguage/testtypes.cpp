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
#include "testtypes.h"

static QObject *myTypeObjectSingleton(QQmlEngine *engine, QJSEngine *scriptEngine)
{
    Q_UNUSED(engine)
    Q_UNUSED(scriptEngine)

    return new MyTypeObject();
}

void registerTypes()
{
    qmlRegisterInterface<MyInterface>("MyInterface");
    qmlRegisterType<MyQmlObject>("Test",1,0,"MyQmlObject");
    qmlRegisterType<MyTypeObject>("Test",1,0,"MyTypeObject");
    qmlRegisterType<MyContainer>("Test",1,0,"MyContainer");
    qmlRegisterType<MyPropertyValueSource>("Test",1,0,"MyPropertyValueSource");
    qmlRegisterType<MyDotPropertyObject>("Test",1,0,"MyDotPropertyObject");
    qmlRegisterType<MyNamespace::MyNamespacedType>("Test",1,0,"MyNamespacedType");
    qmlRegisterType<MyNamespace::MySecondNamespacedType>("Test",1,0,"MySecondNamespacedType");
    qmlRegisterUncreatableMetaObject(MyNamespace::staticMetaObject, "Test", 1, 0, "MyNamespace", "Access to enums & flags only");
    qmlRegisterType<MyParserStatus>("Test",1,0,"MyParserStatus");
    qmlRegisterType<MyGroupedObject>();
    qmlRegisterType<MyRevisionedClass>("Test",1,0,"MyRevisionedClass");
    qmlRegisterType<MyRevisionedClass,1>("Test",1,1,"MyRevisionedClass");
    qmlRegisterType<MyRevisionedIllegalOverload>("Test",1,0,"MyRevisionedIllegalOverload");
    qmlRegisterType<MyRevisionedLegalOverload>("Test",1,0,"MyRevisionedLegalOverload");

    // Register the uncreatable base class
    qmlRegisterRevision<MyRevisionedBaseClassRegistered,1>("Test",1,1);
    // MyRevisionedSubclass 1.0 uses MyRevisionedClass revision 0
    qmlRegisterType<MyRevisionedSubclass>("Test",1,0,"MyRevisionedSubclass");
    // MyRevisionedSubclass 1.1 uses MyRevisionedClass revision 1
    qmlRegisterType<MyRevisionedSubclass,1>("Test",1,1,"MyRevisionedSubclass");

    // Only version 1.0, but its super class is registered in version 1.1 also
    qmlRegisterType<MySubclass>("Test",1,0,"MySubclass");

    qmlRegisterCustomType<MyCustomParserType>("Test", 1, 0, "MyCustomParserType", new MyCustomParserTypeParser);
    qmlRegisterCustomType<MyCustomParserType>("Test", 1, 0, "MyCustomParserWithEnumType", new EnumSupportingCustomParser);

    qmlRegisterTypeNotAvailable("Test",1,0,"UnavailableType", "UnavailableType is unavailable for testing");

    qmlRegisterType<MyQmlObject>("Test.Version",1,0,"MyQmlObject");
    qmlRegisterType<MyTypeObject>("Test.Version",1,0,"MyTypeObject");
    qmlRegisterType<MyTypeObject>("Test.Version",2,0,"MyTypeObject");

    qmlRegisterType<MyVersion2Class>("Test.VersionOrder", 2,0, "MyQmlObject");
    qmlRegisterType<MyQmlObject>("Test.VersionOrder", 1,0, "MyQmlObject");

    qmlRegisterType<MyEnum1Class>("Test",1,0,"MyEnum1Class");
    qmlRegisterType<MyEnum2Class>("Test",1,0,"MyEnum2Class");
    qmlRegisterType<MyEnumDerivedClass>("Test",1,0,"MyEnumDerivedClass");

    qmlRegisterType<MyReceiversTestObject>("Test",1,0,"MyReceiversTestObject");

    qmlRegisterUncreatableType<MyUncreateableBaseClass>("Test", 1, 0, "MyUncreateableBaseClass", "Cannot create MyUncreateableBaseClass");
    qmlRegisterType<MyCreateableDerivedClass>("Test", 1, 0, "MyCreateableDerivedClass");

    qmlRegisterUncreatableType<MyUncreateableBaseClass,1>("Test", 1, 1, "MyUncreateableBaseClass", "Cannot create MyUncreateableBaseClass");
    qmlRegisterType<MyCreateableDerivedClass,1>("Test", 1, 1, "MyCreateableDerivedClass");

    qmlRegisterCustomType<CustomBinding>("Test", 1, 0, "CustomBinding", new CustomBindingParser);
    qmlRegisterCustomType<SimpleObjectWithCustomParser>("Test", 1, 0, "SimpleObjectWithCustomParser", new SimpleObjectCustomParser);

    qmlRegisterCustomExtendedType<SimpleObjectWithCustomParser, SimpleObjectExtension>("Test", 1, 0, "SimpleExtendedObjectWithCustomParser", new SimpleObjectCustomParser);

    qmlRegisterType<RootObjectInCreationTester>("Test", 1, 0, "RootObjectInCreationTester");

    qmlRegisterType<MyCompositeBaseType>("Test", 1, 0, "MyCompositeBaseType");

    qmlRegisterSingletonType<MyTypeObjectSingleton>("Test", 1, 0, "MyTypeObjectSingleton", myTypeObjectSingleton);

    qmlRegisterType<MyArrayBufferTestClass>("Test", 1, 0, "MyArrayBufferTestClass");
}

QVariant myCustomVariantTypeConverter(const QString &data)
{
    MyCustomVariantType rv;
    rv.a = data.toInt();
    return QVariant::fromValue(rv);
}


void CustomBindingParser::applyBindings(QObject *object, QV4::CompiledData::CompilationUnit *compilationUnit, const QList<const QV4::CompiledData::Binding *> &bindings)
{
    CustomBinding *customBinding = qobject_cast<CustomBinding*>(object);
    Q_ASSERT(customBinding);
    customBinding->compilationUnit = compilationUnit;
    customBinding->bindings = bindings;
}

void CustomBinding::componentComplete()
{
    Q_ASSERT(m_target);

    foreach (const QV4::CompiledData::Binding *binding, bindings) {
        QString name = compilationUnit->data->stringAt(binding->propertyNameIndex);

        int bindingId = binding->value.compiledScriptIndex;

        QQmlContextData *context = QQmlContextData::get(qmlContext(this));

        QV4::Scope scope(QQmlEnginePrivate::getV4Engine(qmlEngine(this)));
        QV4::ScopedValue function(scope, QV4::FunctionObject::createQmlFunction(context, m_target, compilationUnit->runtimeFunctions[bindingId]));

        QQmlProperty property(m_target, name, qmlContext(this));
        QQmlBinding *qmlBinding = QQmlBinding::create(&QQmlPropertyPrivate::get(property)->core,
                                                      function, m_target, context);
        qmlBinding->setTarget(property);
        QQmlPropertyPrivate::setBinding(property, qmlBinding);
    }
}

void EnumSupportingCustomParser::verifyBindings(const QV4::CompiledData::Unit *qmlUnit, const QList<const QV4::CompiledData::Binding *> &bindings)
{
    if (bindings.count() != 1) {
        error(bindings.first(), QStringLiteral("Custom parser invoked incorrectly for unit test"));
        return;
    }

    const QV4::CompiledData::Binding *binding = bindings.first();
    if (qmlUnit->stringAt(binding->propertyNameIndex) != QStringLiteral("foo")) {
        error(binding, QStringLiteral("Custom parser invoked with the wrong property name"));
        return;
    }

    if (binding->type != QV4::CompiledData::Binding::Type_Script) {
        error(binding, QStringLiteral("Custom parser invoked with the wrong property value. Expected script that evaluates to enum"));
        return;
    }
    QByteArray script = qmlUnit->stringAt(binding->stringIndex).toUtf8();
    bool ok;
    int v = evaluateEnum(script, &ok);
    if (!ok) {
        error(binding, QStringLiteral("Custom parser invoked with the wrong property value. Script did not evaluate to enum"));
        return;
    }
    if (v != MyEnum1Class::A_13) {
        error(binding, QStringLiteral("Custom parser invoked with the wrong property value. Enum value is not the expected value."));
        return;
    }
}

void SimpleObjectCustomParser::applyBindings(QObject *object, QV4::CompiledData::CompilationUnit *, const QList<const QV4::CompiledData::Binding *> &bindings)
{
    SimpleObjectWithCustomParser *o = qobject_cast<SimpleObjectWithCustomParser*>(object);
    Q_ASSERT(o);
    o->setCustomBindingsCount(bindings.count());
}


MyQmlObject::MyQmlObject()
    : m_value(-1)
    , m_interface(0)
    , m_qmlobject(0)
    , m_childAddedEventCount(0)
{
    qRegisterMetaType<MyCustomVariantType>("MyCustomVariantType");
}

bool MyQmlObject::event(QEvent *event)
{
    if (event->type() == QEvent::ChildAdded)
        m_childAddedEventCount++;
    return QObject::event(event);
}
