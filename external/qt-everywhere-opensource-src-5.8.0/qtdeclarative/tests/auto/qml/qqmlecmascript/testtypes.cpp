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
#ifndef QT_NO_WIDGETS
# include <QWidget>
# include <QPlainTextEdit>
#endif
#include <QQmlEngine>
#include <QJSEngine>
#include <QThread>

class BaseExtensionObject : public QObject
{
    Q_OBJECT
    Q_PROPERTY(int baseExtendedProperty READ extendedProperty WRITE setExtendedProperty NOTIFY extendedPropertyChanged)
public:
    BaseExtensionObject(QObject *parent) : QObject(parent), m_value(0) {}

    int extendedProperty() const { return m_value; }
    void setExtendedProperty(int v) { m_value = v; emit extendedPropertyChanged(); }

signals:
    void extendedPropertyChanged();
private:
    int m_value;
};

class AbstractExtensionObject : public QObject
{
    Q_OBJECT
    Q_PROPERTY(int abstractProperty READ abstractProperty WRITE setAbstractProperty NOTIFY abstractPropertyChanged)
public:

    AbstractExtensionObject(QObject *parent = 0) : QObject(parent), m_abstractProperty(-1) {}

    void setAbstractProperty(int abstractProperty) { m_abstractProperty = abstractProperty; emit abstractPropertyChanged(); }
    int abstractProperty() const { return m_abstractProperty; }

    virtual void shouldBeImplemented() = 0;

signals:
    void abstractPropertyChanged();

private:
    int m_abstractProperty;
};

class ImplementedExtensionObject : public AbstractExtensionObject
{
    Q_OBJECT
    Q_PROPERTY(int implementedProperty READ implementedProperty WRITE setImplementedProperty NOTIFY implementedPropertyChanged)
public:
    ImplementedExtensionObject(QObject *parent = 0) : AbstractExtensionObject(parent), m_implementedProperty(883) {}
    void shouldBeImplemented() {}

    void setImplementedProperty(int implementedProperty) { m_implementedProperty = implementedProperty; emit implementedPropertyChanged(); }
    int implementedProperty() const { return m_implementedProperty; }

signals:
    void implementedPropertyChanged();

private:
    int m_implementedProperty;
};

class ExtensionObject : public QObject
{
    Q_OBJECT
    Q_PROPERTY(int extendedProperty READ extendedProperty WRITE setExtendedProperty NOTIFY extendedPropertyChanged)
public:
    ExtensionObject(QObject *parent) : QObject(parent), m_value(0) {}

    int extendedProperty() const { return m_value; }
    void setExtendedProperty(int v) { m_value = v; emit extendedPropertyChanged(); }

signals:
    void extendedPropertyChanged();
private:
    int m_value;
};

class DefaultPropertyExtensionObject : public QObject
{
    Q_OBJECT
    Q_CLASSINFO("DefaultProperty", "firstProperty")
public:
    DefaultPropertyExtensionObject(QObject *parent) : QObject(parent) {}
};

class QWidgetDeclarativeUI : public QObject
{
    Q_OBJECT

    Q_PROPERTY(int width READ width WRITE setWidth NOTIFY widthChanged)

signals:
    void widthChanged();

public:
    QWidgetDeclarativeUI(QObject *other) : QObject(other) { }

public:
    int width() const { return 0; }
    void setWidth(int) { }
};

void MyQmlObject::v8function(QQmlV4Function *function)
{
    function->v4engine()->throwError(QStringLiteral("Exception thrown from within QObject slot"));
}

static QJSValue script_api(QQmlEngine *engine, QJSEngine *scriptEngine)
{
    Q_UNUSED(engine)

    static int testProperty = 13;
    QJSValue v = scriptEngine->newObject();
    v.setProperty("scriptTestProperty", testProperty++);
    return v;
}

static QJSValue readonly_script_api(QQmlEngine *engine, QJSEngine *scriptEngine)
{
    Q_UNUSED(engine)

    static int testProperty = 42;
    QJSValue v = scriptEngine->newObject();
    v.setProperty("scriptTestProperty", testProperty++);

    // now freeze it so that it's read-only
    QJSValue freezeFunction = scriptEngine->evaluate("(function(obj) { return Object.freeze(obj); })");
    v = freezeFunction.call(QJSValueList() << v);

    return v;
}

static QObject *testImportOrder_api(QQmlEngine *engine, QJSEngine *scriptEngine)
{
    Q_UNUSED(engine)
    Q_UNUSED(scriptEngine)

    testImportOrderApi *o = new testImportOrderApi(37);
    return o;
}

static QObject *testImportOrder_api1(QQmlEngine *engine, QJSEngine *scriptEngine)
{
    Q_UNUSED(engine)
    Q_UNUSED(scriptEngine)

    testImportOrderApi *o = new testImportOrderApi(1);
    return o;
}

static QObject *testImportOrder_api2(QQmlEngine *engine, QJSEngine *scriptEngine)
{
    Q_UNUSED(engine)
    Q_UNUSED(scriptEngine)

    testImportOrderApi *o = new testImportOrderApi(2);
    return o;
}

static QObject *qobject_api(QQmlEngine *engine, QJSEngine *scriptEngine)
{
    Q_UNUSED(engine)
    Q_UNUSED(scriptEngine)

    testQObjectApi *o = new testQObjectApi();
    o->setQObjectTestProperty(20);
    o->setQObjectTestWritableProperty(50);
    o->setQObjectTestWritableFinalProperty(10);
    return o;
}

static QObject *qobject_api_two(QQmlEngine *engine, QJSEngine *scriptEngine)
{
    Q_UNUSED(engine)
    Q_UNUSED(scriptEngine)

    testQObjectApiTwo *o = new testQObjectApiTwo;
    return o;
}

static QObject *qobject_api_engine_parent(QQmlEngine *engine, QJSEngine *scriptEngine)
{
    Q_UNUSED(scriptEngine)

    static int testProperty = 26;
    testQObjectApi *o = new testQObjectApi(engine);
    o->setQObjectTestProperty(testProperty++);
    return o;
}

static QObject *fallback_bindings_object(QQmlEngine *engine, QJSEngine *scriptEngine)
{
    Q_UNUSED(engine)
    Q_UNUSED(scriptEngine)

    return new FallbackBindingsObject();
}

static QObject *fallback_bindings_derived(QQmlEngine *engine, QJSEngine *scriptEngine)
{
    Q_UNUSED(engine)
    Q_UNUSED(scriptEngine)

    return new FallbackBindingsDerived();
}

class MyWorkerObjectThread : public QThread
{
public:
    MyWorkerObjectThread(MyWorkerObject *o) : QThread(o), o(o) { start(); }

    virtual void run() {
        emit o->done(QLatin1String("good"));
    }

    MyWorkerObject *o;
};

void MyWorkerObject::doIt()
{
    new MyWorkerObjectThread(this);
}

class MyDateClass : public QObject
{
    Q_OBJECT
public:
    Q_INVOKABLE QDateTime invalidDate()
    {
        return QDateTime();
    }
};

class MiscTypeTestClass : public QObject
{
    Q_OBJECT
public:
    Q_INVOKABLE QUrl invalidUrl()
    {
        return QUrl();
    }

    Q_INVOKABLE QUrl validUrl()
    {
        return QUrl("http://wwww.qt-project.org");
    }
};

class MyStringClass : public QObject
{
    Q_OBJECT
public:
    Q_INVOKABLE QStringList strings(QStringList stringList) const
    {
        return stringList;
    }
    Q_INVOKABLE QList<int> integers(QList<int> v) const
    {
        return v;
    }
    Q_INVOKABLE QList<qreal> reals(QList<qreal> v) const
    {
        return v;
    }
    Q_INVOKABLE QList<bool> bools(QList<bool> v) const
    {
        return v;
    }
    Q_INVOKABLE QVector<int> integerVector(QVector<int> v) const
    {
        return v;
    }
    Q_INVOKABLE QVector<qreal> realVector(QVector<qreal> v) const
    {
        return v;
    }
    Q_INVOKABLE QVector<bool> boolVector(QVector<bool> v) const
    {
        return v;
    }
};

static MyInheritedQmlObject *theSingletonObject = 0;

static QObject *inheritedQmlObject_provider(QQmlEngine* /* engine */, QJSEngine* /* scriptEngine */)
{
    theSingletonObject = new MyInheritedQmlObject();
    return theSingletonObject;
}

bool MyInheritedQmlObject::isItYouQObject(QObject *o)
{
    return o && o == theSingletonObject;
}

bool MyInheritedQmlObject::isItYouMyQmlObject(MyQmlObject *o)
{
    return o && o == theSingletonObject;
}

bool MyInheritedQmlObject::isItYouMyInheritedQmlObject(MyInheritedQmlObject *o)
{
    return o && o == theSingletonObject;
}

class TestTypeCppSingleton : public QObject
{
    Q_OBJECT
    Q_PROPERTY(int foo READ foo)

    Q_CLASSINFO("DefaultProperty", "foo")
public:
    int foo() { return 0; }
    static TestTypeCppSingleton *instance() {
        static TestTypeCppSingleton cppSingleton;
        return &cppSingleton;
    }
private:
    TestTypeCppSingleton(){}
    ~TestTypeCppSingleton() {
        // just to make sure it crashes on double delete
        static int a = 0;
        static int *ptr = &a;
        *ptr = 1;
        ptr = 0;
    }
};

QObject *testTypeCppProvider(QQmlEngine *engine, QJSEngine *scriptEngine)
{
    Q_UNUSED(engine);
    Q_UNUSED(scriptEngine);
    TestTypeCppSingleton *o = TestTypeCppSingleton::instance();
    QQmlEngine::setObjectOwnership(o, QQmlEngine::CppOwnership);
    return o;
}

static QObject *create_singletonWithEnum(QQmlEngine *, QJSEngine *)
{
    return new SingletonWithEnum;
}

QObjectContainer::QObjectContainer()
    : widgetParent(0)
    , gcOnAppend(false)
{}

QQmlListProperty<QObject> QObjectContainer::data()
{
    return QQmlListProperty<QObject>(this, 0, children_append, children_count, children_at, children_clear);
}

void QObjectContainer::children_append(QQmlListProperty<QObject> *prop, QObject *o)
{
    QObjectContainer *that = static_cast<QObjectContainer*>(prop->object);
    that->dataChildren.append(o);
    QObject::connect(o, SIGNAL(destroyed(QObject*)), prop->object, SLOT(childDestroyed(QObject*)));

    if (that->gcOnAppend) {
        QQmlEngine *engine = qmlEngine(that);
        engine->collectGarbage();
        QCoreApplication::sendPostedEvents(0, QEvent::DeferredDelete);
        QCoreApplication::processEvents();
    }
}

int QObjectContainer::children_count(QQmlListProperty<QObject> *prop)
{
    return static_cast<QObjectContainer*>(prop->object)->dataChildren.count();
}

QObject *QObjectContainer::children_at(QQmlListProperty<QObject> *prop, int index)
{
    return static_cast<QObjectContainer*>(prop->object)->dataChildren.at(index);
}

void QObjectContainer::children_clear(QQmlListProperty<QObject> *prop)
{
    QObjectContainer *that = static_cast<QObjectContainer*>(prop->object);
    foreach (QObject *c, that->dataChildren)
        QObject::disconnect(c, SIGNAL(destroyed(QObject*)), that, SLOT(childDestroyed(QObject*)));
    that->dataChildren.clear();
}

void QObjectContainer::childDestroyed(QObject *child) {
    dataChildren.removeAll(child);
}

void FloatingQObject::classBegin()
{
    setParent(0);
}

void FloatingQObject::componentComplete()
{
    Q_ASSERT(!parent());
}

void registerTypes()
{
    qmlRegisterType<MyQmlObject>("Qt.test", 1,0, "MyQmlObjectAlias");
    qmlRegisterType<MyQmlObject>("Qt.test", 1,0, "MyQmlObject");
    qmlRegisterSingletonType<MyInheritedQmlObject>("Test", 1, 0, "MyInheritedQmlObjectSingleton", inheritedQmlObject_provider);
    qmlRegisterSingletonType<TestTypeCppSingleton>("Test", 1, 0, "TestTypeCppSingleton", testTypeCppProvider);
    qmlRegisterType<MyDeferredObject>("Qt.test", 1,0, "MyDeferredObject");
    qmlRegisterType<MyVeryDeferredObject>("Qt.test", 1,0, "MyVeryDeferredObject");
    qmlRegisterType<MyQmlContainer>("Qt.test", 1,0, "MyQmlContainer");
    qmlRegisterExtendedType<MyBaseExtendedObject, BaseExtensionObject>("Qt.test", 1,0, "MyBaseExtendedObject");
    qmlRegisterExtendedType<MyExtendedObject, ExtensionObject>("Qt.test", 1,0, "MyExtendedObject");
    qmlRegisterType<MyTypeObject>("Qt.test", 1,0, "MyTypeObject");
    qmlRegisterType<MyDerivedObject>("Qt.test", 1,0, "MyDerivedObject");
    qmlRegisterType<NumberAssignment>("Qt.test", 1,0, "NumberAssignment");
    qmlRegisterExtendedType<DefaultPropertyExtendedObject, DefaultPropertyExtensionObject>("Qt.test", 1,0, "DefaultPropertyExtendedObject");
    qmlRegisterType<OverrideDefaultPropertyObject>("Qt.test", 1,0, "OverrideDefaultPropertyObject");
    qmlRegisterType<MyRevisionedClass>("Qt.test",1,0,"MyRevisionedClass");
    qmlRegisterType<MyDeleteObject>("Qt.test", 1,0, "MyDeleteObject");
    qmlRegisterType<MyRevisionedClass,1>("Qt.test",1,1,"MyRevisionedClass");
    qmlRegisterType<MyWorkerObject>("Qt.test", 1,0, "MyWorkerObject");

    qmlRegisterExtendedUncreatableType<AbstractExtensionObject, ExtensionObject>("Qt.test", 1,0, "AbstractExtensionObject", QStringLiteral("Abstracts are uncreatable"));
    qmlRegisterType<ImplementedExtensionObject>("Qt.test", 1,0, "ImplementedExtensionObject");

    // test scarce resource property binding post-evaluation optimization
    // and for testing memory usage in property var circular reference test
    qmlRegisterType<ScarceResourceObject>("Qt.test", 1,0, "MyScarceResourceObject");

    // Register the uncreatable base class
    qmlRegisterRevision<MyRevisionedBaseClassRegistered,1>("Qt.test",1,1);
    // MyRevisionedSubclass 1.0 uses MyRevisionedClass revision 0
    qmlRegisterType<MyRevisionedSubclass>("Qt.test",1,0,"MyRevisionedSubclass");
    // MyRevisionedSubclass 1.1 uses MyRevisionedClass revision 1
    qmlRegisterType<MyRevisionedSubclass,1>("Qt.test",1,1,"MyRevisionedSubclass");
    qmlRegisterType<MyItemUsingRevisionedObject>("Qt.test", 1, 0, "MyItemUsingRevisionedObject");

#ifndef QT_NO_WIDGETS
    qmlRegisterExtendedType<QWidget,QWidgetDeclarativeUI>("Qt.test",1,0,"QWidget");
    qmlRegisterType<QPlainTextEdit>("Qt.test",1,0,"QPlainTextEdit");
#endif

    qRegisterMetaType<MyQmlObject::MyType>("MyQmlObject::MyType");

    qmlRegisterSingletonType("Qt.test",1,0,"Script",script_api);                             // register (script) singleton Type for an existing uri which contains elements
    qmlRegisterSingletonType<testQObjectApi>("Qt.test",1,0,"QObject",qobject_api);            // register (qobject) for an existing uri for which another singleton Type was previously regd.  Should replace!
    qmlRegisterSingletonType("Qt.test.scriptApi",1,0,"Script",script_api);                   // register (script) singleton Type for a uri which doesn't contain elements
    qmlRegisterSingletonType("Qt.test.scriptApi",2,0,"Script",readonly_script_api);          // register (script) singleton Type for a uri which doesn't contain elements - will be made read-only
    qmlRegisterSingletonType<testQObjectApi>("Qt.test.qobjectApi",1,0,"QObject",qobject_api); // register (qobject) singleton Type for a uri which doesn't contain elements
    qmlRegisterSingletonType<testQObjectApi>("Qt.test.qobjectApi",1,3,"QObject",qobject_api); // register (qobject) singleton Type for a uri which doesn't contain elements, minor version set
    qmlRegisterSingletonType<testQObjectApi>("Qt.test.qobjectApi",2,0,"QObject",qobject_api); // register (qobject) singleton Type for a uri which doesn't contain elements, major version set
    qmlRegisterSingletonType<testQObjectApi>("Qt.test.qobjectApiParented",1,0,"QObject",qobject_api_engine_parent); // register (parented qobject) singleton Type for a uri which doesn't contain elements

    qmlRegisterSingletonType<testQObjectApi>("Qt.test.qobjectApis",1,0,"One",qobject_api); // register multiple qobject singleton types in a single namespace
    qmlRegisterSingletonType<testQObjectApiTwo>("Qt.test.qobjectApis",1,0,"Two",qobject_api_two); // register multiple qobject singleton types in a single namespace

    qRegisterMetaType<MyQmlObject::MyEnum2>("MyEnum2");
    qRegisterMetaType<Qt::MouseButtons>("Qt::MouseButtons");

    qmlRegisterType<CircularReferenceObject>("Qt.test", 1, 0, "CircularReferenceObject");

    qmlRegisterType<MyDynamicCreationDestructionObject>("Qt.test", 1, 0, "MyDynamicCreationDestructionObject");
    qmlRegisterType<WriteCounter>("Qt.test", 1, 0, "WriteCounter");

    qmlRegisterType<MySequenceConversionObject>("Qt.test", 1, 0, "MySequenceConversionObject");

    qmlRegisterType<MyUnregisteredEnumTypeObject>("Qt.test", 1, 0, "MyUnregisteredEnumTypeObject");

    qmlRegisterSingletonType<FallbackBindingsObject>("Qt.test.fallbackBindingsObject", 1, 0, "Fallback", fallback_bindings_object);
    qmlRegisterSingletonType<FallbackBindingsObject>("Qt.test.fallbackBindingsDerived", 1, 0, "Fallback", fallback_bindings_derived);

    qmlRegisterType<FallbackBindingsObject>("Qt.test.fallbackBindingsItem", 1, 0, "FallbackBindingsType");
    qmlRegisterType<FallbackBindingsDerived>("Qt.test.fallbackBindingsItem", 1, 0, "FallbackBindingsDerivedType");

    qmlRegisterType<FallbackBindingsTypeObject>("Qt.test.fallbackBindingsObject", 1, 0, "FallbackBindingsType");
    qmlRegisterType<FallbackBindingsTypeDerived>("Qt.test.fallbackBindingsDerived", 1, 0, "FallbackBindingsType");

    qmlRegisterType<MyDateClass>("Qt.test", 1, 0, "MyDateClass");
    qmlRegisterType<MyStringClass>("Qt.test", 1, 0, "MyStringClass");
    qmlRegisterType<MiscTypeTestClass>("Qt.test", 1, 0, "MiscTypeTest");

    qmlRegisterSingletonType<testImportOrderApi>("Qt.test.importOrderApi",1,0,"Data",testImportOrder_api);
    qmlRegisterSingletonType<testImportOrderApi>("NamespaceAndType",1,0,"NamespaceAndType",testImportOrder_api);
    qmlRegisterSingletonType<testImportOrderApi>("Qt.test.importOrderApi1",1,0,"Data",testImportOrder_api1);
    qmlRegisterSingletonType<testImportOrderApi>("Qt.test.importOrderApi2",1,0,"Data",testImportOrder_api2);

    qmlRegisterSingletonType<SingletonWithEnum>("Qt.test.singletonWithEnum", 1, 0, "SingletonWithEnum", create_singletonWithEnum);

    qmlRegisterType<QObjectContainer>("Qt.test", 1, 0, "QObjectContainer");
    qmlRegisterType<QObjectContainerWithGCOnAppend>("Qt.test", 1, 0, "QObjectContainerWithGCOnAppend");
    qmlRegisterType<FloatingQObject>("Qt.test", 1, 0, "FloatingQObject");
}

#include "testtypes.moc"
