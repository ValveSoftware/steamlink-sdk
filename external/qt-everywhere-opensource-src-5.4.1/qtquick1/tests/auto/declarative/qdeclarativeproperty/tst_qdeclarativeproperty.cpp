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
#include <QtDeclarative/qdeclarativeengine.h>
#include <QtDeclarative/qdeclarativecomponent.h>
#include <QtDeclarative/qdeclarativeproperty.h>
#include <QtDeclarative/private/qdeclarativeproperty_p.h>
#include <private/qdeclarativebinding_p.h>
#include <QtWidgets/QLineEdit>
#include <QtCore/qfileinfo.h>
#include <QtCore/qdir.h>

class MyQmlObject : public QObject
{
    Q_OBJECT
public:
    MyQmlObject() {}
};

QML_DECLARE_TYPE(MyQmlObject)

class MyAttached : public QObject
{
    Q_OBJECT
    Q_PROPERTY(int foo READ foo WRITE setFoo)
public:
    MyAttached(QObject *parent) : QObject(parent), m_foo(13) {}

    int foo() const { return m_foo; }
    void setFoo(int f) { m_foo = f; }

private:
    int m_foo;
};

class MyContainer : public QObject
{
    Q_OBJECT
    Q_PROPERTY(QDeclarativeListProperty<MyQmlObject> children READ children)
public:
    MyContainer() {}

    QDeclarativeListProperty<MyQmlObject> children() { return QDeclarativeListProperty<MyQmlObject>(this, m_children); }

    static MyAttached *qmlAttachedProperties(QObject *o) {
        return new MyAttached(o);
    }

private:
    QList<MyQmlObject*> m_children;
};

QML_DECLARE_TYPE(MyContainer)
QML_DECLARE_TYPEINFO(MyContainer, QML_HAS_ATTACHED_PROPERTIES)

class tst_qdeclarativeproperty : public QDeclarativeDataTest
{
    Q_OBJECT
public:
    tst_qdeclarativeproperty() {}

private slots:
    void initTestCase();

    // Constructors
    void qmlmetaproperty();
    void qmlmetaproperty_object();
    void qmlmetaproperty_object_string();
    void qmlmetaproperty_object_context();
    void qmlmetaproperty_object_string_context();

    // Methods
    void name();
    void read();
    void write();
    void reset();

    // Functionality
    void writeObjectToList();
    void writeListToList();

    //writeToReadOnly();

    // Bugs
    void crashOnValueProperty();
    void aliasPropertyBindings();

    void copy();
private:
    QDeclarativeEngine engine;
};

void tst_qdeclarativeproperty::qmlmetaproperty()
{
    QDeclarativeProperty prop;

    QWeakPointer<QDeclarativeBinding> binding(new QDeclarativeBinding(QLatin1String("null"), 0, engine.rootContext()));
    QVERIFY(binding != 0);
    QWeakPointer<QDeclarativeExpression> expression(new QDeclarativeExpression());
    QVERIFY(expression != 0);

    QObject *obj = new QObject;

    QCOMPARE(prop.name(), QString());
    QCOMPARE(prop.read(), QVariant());
    QCOMPARE(prop.write(QVariant()), false);
    QCOMPARE(prop.hasNotifySignal(), false);
    QCOMPARE(prop.needsNotifySignal(), false);
    QCOMPARE(prop.connectNotifySignal(0, SLOT(deleteLater())), false);
    QCOMPARE(prop.connectNotifySignal(obj, SLOT(deleteLater())), false);
    QCOMPARE(prop.connectNotifySignal(obj, 0), false);
    QCOMPARE(prop.connectNotifySignal(0, obj->metaObject()->indexOfMethod("deleteLater()")), false);
    QCOMPARE(prop.connectNotifySignal(obj, obj->metaObject()->indexOfMethod("deleteLater()")), false);
    QCOMPARE(prop.connectNotifySignal(obj, -1), false);
    QVERIFY(!prop.method().isValid());
    QCOMPARE(prop.type(), QDeclarativeProperty::Invalid);
    QCOMPARE(prop.isProperty(), false);
    QCOMPARE(prop.isWritable(), false);
    QCOMPARE(prop.isDesignable(), false);
    QCOMPARE(prop.isResettable(), false);
    QCOMPARE(prop.isSignalProperty(), false);
    QCOMPARE(prop.isValid(), false);
    QCOMPARE(prop.object(), (QObject *)0);
    QCOMPARE(prop.propertyTypeCategory(), QDeclarativeProperty::InvalidCategory);
    QCOMPARE(prop.propertyType(), 0);
    QCOMPARE(prop.propertyTypeName(), (const char *)0);
    QVERIFY(prop.property().name() == 0);
    QVERIFY(QDeclarativePropertyPrivate::binding(prop) == 0);
    QVERIFY(QDeclarativePropertyPrivate::setBinding(prop, binding.data()) == 0);
    QVERIFY(binding == 0);
    QVERIFY(QDeclarativePropertyPrivate::signalExpression(prop) == 0);
    QVERIFY(QDeclarativePropertyPrivate::setSignalExpression(prop, expression.data()) == 0);
    QVERIFY(expression == 0);
    QCOMPARE(prop.index(), -1);
    QCOMPARE(QDeclarativePropertyPrivate::valueTypeCoreIndex(prop), -1);

    delete obj;
}

class PropertyObject : public QObject
{
    Q_OBJECT
    Q_PROPERTY(int defaultProperty READ defaultProperty)
    Q_PROPERTY(QRect rectProperty READ rectProperty)
    Q_PROPERTY(QRect wrectProperty READ wrectProperty WRITE setWRectProperty)
    Q_PROPERTY(QUrl url READ url WRITE setUrl)
    Q_PROPERTY(int resettableProperty READ resettableProperty WRITE setResettableProperty RESET resetProperty)
    Q_PROPERTY(int propertyWithNotify READ propertyWithNotify WRITE setPropertyWithNotify NOTIFY oddlyNamedNotifySignal)
    Q_PROPERTY(MyQmlObject *qmlObject READ qmlObject)

    Q_CLASSINFO("DefaultProperty", "defaultProperty")
public:
    PropertyObject() : m_resetProperty(9) {}

    int defaultProperty() { return 10; }
    QRect rectProperty() { return QRect(10, 10, 1, 209); }

    QRect wrectProperty() { return m_rect; }
    void setWRectProperty(const QRect &r) { m_rect = r; }

    QUrl url() { return m_url; }
    void setUrl(const QUrl &u) { m_url = u; }

    int resettableProperty() const { return m_resetProperty; }
    void setResettableProperty(int r) { m_resetProperty = r; }
    void resetProperty() { m_resetProperty = 9; }

    int propertyWithNotify() const { return m_propertyWithNotify; }
    void setPropertyWithNotify(int i) { m_propertyWithNotify = i; emit oddlyNamedNotifySignal(); }

    MyQmlObject *qmlObject() { return &m_qmlObject; }
signals:
    void clicked();
    void oddlyNamedNotifySignal();

private:
    int m_resetProperty;
    QRect m_rect;
    QUrl m_url;
    int m_propertyWithNotify;
    MyQmlObject m_qmlObject;
};

QML_DECLARE_TYPE(PropertyObject);

void tst_qdeclarativeproperty::qmlmetaproperty_object()
{
    QObject object; // Has no default property
    PropertyObject dobject; // Has default property

    {
        QDeclarativeProperty prop(&object);

        QWeakPointer<QDeclarativeBinding> binding(new QDeclarativeBinding(QLatin1String("null"), 0, engine.rootContext()));
        QVERIFY(binding != 0);
        QWeakPointer<QDeclarativeExpression> expression(new QDeclarativeExpression());
        QVERIFY(expression != 0);

        QObject *obj = new QObject;

        QCOMPARE(prop.name(), QString());
        QCOMPARE(prop.read(), QVariant());
        QCOMPARE(prop.write(QVariant()), false);
        QCOMPARE(prop.hasNotifySignal(), false);
        QCOMPARE(prop.needsNotifySignal(), false);
        QCOMPARE(prop.connectNotifySignal(0, SLOT(deleteLater())), false);
        QCOMPARE(prop.connectNotifySignal(obj, SLOT(deleteLater())), false);
        QCOMPARE(prop.connectNotifySignal(obj, 0), false);
        QCOMPARE(prop.connectNotifySignal(0, obj->metaObject()->indexOfMethod("deleteLater()")), false);
        QCOMPARE(prop.connectNotifySignal(obj, obj->metaObject()->indexOfMethod("deleteLater()")), false);
        QCOMPARE(prop.connectNotifySignal(obj, -1), false);
        QVERIFY(!prop.method().isValid());
        QCOMPARE(prop.type(), QDeclarativeProperty::Invalid);
        QCOMPARE(prop.isProperty(), false);
        QCOMPARE(prop.isWritable(), false);
        QCOMPARE(prop.isDesignable(), false);
        QCOMPARE(prop.isResettable(), false);
        QCOMPARE(prop.isSignalProperty(), false);
        QCOMPARE(prop.isValid(), false);
        QCOMPARE(prop.object(), (QObject *)0);
        QCOMPARE(prop.propertyTypeCategory(), QDeclarativeProperty::InvalidCategory);
        QCOMPARE(prop.propertyType(), 0);
        QCOMPARE(prop.propertyTypeName(), (const char *)0);
        QVERIFY(prop.property().name() == 0);
        QVERIFY(QDeclarativePropertyPrivate::binding(prop) == 0);
        QVERIFY(QDeclarativePropertyPrivate::setBinding(prop, binding.data()) == 0);
        QVERIFY(binding == 0);
        QVERIFY(QDeclarativePropertyPrivate::signalExpression(prop) == 0);
        QVERIFY(QDeclarativePropertyPrivate::setSignalExpression(prop, expression.data()) == 0);
        QVERIFY(expression == 0);
        QCOMPARE(prop.index(), -1);
        QCOMPARE(QDeclarativePropertyPrivate::valueTypeCoreIndex(prop), -1);

        delete obj;
    }

    {
        QDeclarativeProperty prop(&dobject);

        QWeakPointer<QDeclarativeBinding> binding(new QDeclarativeBinding(QLatin1String("null"), 0, engine.rootContext()));
        binding.data()->setTarget(prop);
        QVERIFY(binding != 0);
        QWeakPointer<QDeclarativeExpression> expression(new QDeclarativeExpression());
        QVERIFY(expression != 0);

        QObject *obj = new QObject;

        QCOMPARE(prop.name(), QString("defaultProperty"));
        QCOMPARE(prop.read(), QVariant(10));
        QCOMPARE(prop.write(QVariant()), false);
        QCOMPARE(prop.hasNotifySignal(), false);
        QCOMPARE(prop.needsNotifySignal(), true);
        QCOMPARE(prop.connectNotifySignal(0, SLOT(deleteLater())), false);
        QCOMPARE(prop.connectNotifySignal(obj, SLOT(deleteLater())), false);
        QCOMPARE(prop.connectNotifySignal(obj, 0), false);
        QCOMPARE(prop.connectNotifySignal(0, obj->metaObject()->indexOfMethod("deleteLater()")), false);
        QCOMPARE(prop.connectNotifySignal(obj, obj->metaObject()->indexOfMethod("deleteLater()")), false);
        QCOMPARE(prop.connectNotifySignal(obj, -1), false);
        QVERIFY(!prop.method().isValid());
        QCOMPARE(prop.type(), QDeclarativeProperty::Property);
        QCOMPARE(prop.isProperty(), true);
        QCOMPARE(prop.isWritable(), false);
        QCOMPARE(prop.isDesignable(), true);
        QCOMPARE(prop.isResettable(), false);
        QCOMPARE(prop.isSignalProperty(), false);
        QCOMPARE(prop.isValid(), true);
        QCOMPARE(prop.object(), qobject_cast<QObject*>(&dobject));
        QCOMPARE(prop.propertyTypeCategory(), QDeclarativeProperty::Normal);
        QCOMPARE(prop.propertyType(), (int)QVariant::Int);
        QCOMPARE(prop.propertyTypeName(), "int");
        QCOMPARE(QString(prop.property().name()), QString("defaultProperty"));
        QVERIFY(QDeclarativePropertyPrivate::binding(prop) == 0);
        QTest::ignoreMessage(QtWarningMsg, "<Unknown File>: Unable to assign null to int");
        QVERIFY(QDeclarativePropertyPrivate::setBinding(prop, binding.data()) == 0);
        QVERIFY(binding != 0);
        QVERIFY(QDeclarativePropertyPrivate::binding(prop) == binding.data());
        QVERIFY(QDeclarativePropertyPrivate::signalExpression(prop) == 0);
        QVERIFY(QDeclarativePropertyPrivate::setSignalExpression(prop, expression.data()) == 0);
        QVERIFY(expression == 0);
        QCOMPARE(prop.index(), dobject.metaObject()->indexOfProperty("defaultProperty"));
        QCOMPARE(QDeclarativePropertyPrivate::valueTypeCoreIndex(prop), -1);

        delete obj;
    }
}

void tst_qdeclarativeproperty::qmlmetaproperty_object_string()
{
    QObject object;
    PropertyObject dobject;

    {
        QDeclarativeProperty prop(&object, QString("defaultProperty"));

        QWeakPointer<QDeclarativeBinding> binding(new QDeclarativeBinding(QLatin1String("null"), 0, engine.rootContext()));
        QVERIFY(binding != 0);
        QWeakPointer<QDeclarativeExpression> expression(new QDeclarativeExpression());
        QVERIFY(expression != 0);

        QObject *obj = new QObject;

        QCOMPARE(prop.name(), QString());
        QCOMPARE(prop.read(), QVariant());
        QCOMPARE(prop.write(QVariant()), false);
        QCOMPARE(prop.hasNotifySignal(), false);
        QCOMPARE(prop.needsNotifySignal(), false);
        QCOMPARE(prop.connectNotifySignal(0, SLOT(deleteLater())), false);
        QCOMPARE(prop.connectNotifySignal(obj, SLOT(deleteLater())), false);
        QCOMPARE(prop.connectNotifySignal(obj, 0), false);
        QCOMPARE(prop.connectNotifySignal(0, obj->metaObject()->indexOfMethod("deleteLater()")), false);
        QCOMPARE(prop.connectNotifySignal(obj, obj->metaObject()->indexOfMethod("deleteLater()")), false);
        QCOMPARE(prop.connectNotifySignal(obj, -1), false);
        QVERIFY(!prop.method().isValid());
        QCOMPARE(prop.type(), QDeclarativeProperty::Invalid);
        QCOMPARE(prop.isProperty(), false);
        QCOMPARE(prop.isWritable(), false);
        QCOMPARE(prop.isDesignable(), false);
        QCOMPARE(prop.isResettable(), false);
        QCOMPARE(prop.isSignalProperty(), false);
        QCOMPARE(prop.isValid(), false);
        QCOMPARE(prop.object(), (QObject *)0);
        QCOMPARE(prop.propertyTypeCategory(), QDeclarativeProperty::InvalidCategory);
        QCOMPARE(prop.propertyType(), 0);
        QCOMPARE(prop.propertyTypeName(), (const char *)0);
        QVERIFY(prop.property().name() == 0);
        QVERIFY(QDeclarativePropertyPrivate::binding(prop) == 0);
        QVERIFY(QDeclarativePropertyPrivate::setBinding(prop, binding.data()) == 0);
        QVERIFY(binding == 0);
        QVERIFY(QDeclarativePropertyPrivate::signalExpression(prop) == 0);
        QVERIFY(QDeclarativePropertyPrivate::setSignalExpression(prop, expression.data()) == 0);
        QVERIFY(expression == 0);
        QCOMPARE(prop.index(), -1);
        QCOMPARE(QDeclarativePropertyPrivate::valueTypeCoreIndex(prop), -1);

        delete obj;
    }

    {
        QDeclarativeProperty prop(&dobject, QString("defaultProperty"));

        QWeakPointer<QDeclarativeBinding> binding(new QDeclarativeBinding(QLatin1String("null"), 0, engine.rootContext()));
        binding.data()->setTarget(prop);
        QVERIFY(binding != 0);
        QWeakPointer<QDeclarativeExpression> expression(new QDeclarativeExpression());
        QVERIFY(expression != 0);

        QObject *obj = new QObject;

        QCOMPARE(prop.name(), QString("defaultProperty"));
        QCOMPARE(prop.read(), QVariant(10));
        QCOMPARE(prop.write(QVariant()), false);
        QCOMPARE(prop.hasNotifySignal(), false);
        QCOMPARE(prop.needsNotifySignal(), true);
        QCOMPARE(prop.connectNotifySignal(0, SLOT(deleteLater())), false);
        QCOMPARE(prop.connectNotifySignal(obj, SLOT(deleteLater())), false);
        QCOMPARE(prop.connectNotifySignal(obj, 0), false);
        QCOMPARE(prop.connectNotifySignal(0, obj->metaObject()->indexOfMethod("deleteLater()")), false);
        QCOMPARE(prop.connectNotifySignal(obj, obj->metaObject()->indexOfMethod("deleteLater()")), false);
        QCOMPARE(prop.connectNotifySignal(obj, -1), false);
        QVERIFY(!prop.method().isValid());
        QCOMPARE(prop.type(), QDeclarativeProperty::Property);
        QCOMPARE(prop.isProperty(), true);
        QCOMPARE(prop.isWritable(), false);
        QCOMPARE(prop.isDesignable(), true);
        QCOMPARE(prop.isResettable(), false);
        QCOMPARE(prop.isSignalProperty(), false);
        QCOMPARE(prop.isValid(), true);
        QCOMPARE(prop.object(), qobject_cast<QObject*>(&dobject));
        QCOMPARE(prop.propertyTypeCategory(), QDeclarativeProperty::Normal);
        QCOMPARE(prop.propertyType(), (int)QVariant::Int);
        QCOMPARE(prop.propertyTypeName(), "int");
        QCOMPARE(QString(prop.property().name()), QString("defaultProperty"));
        QVERIFY(QDeclarativePropertyPrivate::binding(prop) == 0);
        QTest::ignoreMessage(QtWarningMsg, "<Unknown File>: Unable to assign null to int");
        QVERIFY(QDeclarativePropertyPrivate::setBinding(prop, binding.data()) == 0);
        QVERIFY(binding != 0);
        QVERIFY(QDeclarativePropertyPrivate::binding(prop) == binding.data());
        QVERIFY(QDeclarativePropertyPrivate::signalExpression(prop) == 0);
        QVERIFY(QDeclarativePropertyPrivate::setSignalExpression(prop, expression.data()) == 0);
        QVERIFY(expression == 0);
        QCOMPARE(prop.index(), dobject.metaObject()->indexOfProperty("defaultProperty"));
        QCOMPARE(QDeclarativePropertyPrivate::valueTypeCoreIndex(prop), -1);

        delete obj;
    }

    {
        QDeclarativeProperty prop(&dobject, QString("onClicked"));

        QWeakPointer<QDeclarativeBinding> binding(new QDeclarativeBinding(QLatin1String("null"), 0, engine.rootContext()));
        binding.data()->setTarget(prop);
        QVERIFY(binding != 0);
        QWeakPointer<QDeclarativeExpression> expression(new QDeclarativeExpression());
        QVERIFY(expression != 0);

        QObject *obj = new QObject;

        QCOMPARE(prop.name(), QString("onClicked"));
        QCOMPARE(prop.read(), QVariant());
        QCOMPARE(prop.write(QVariant("Hello")), false);
        QCOMPARE(prop.hasNotifySignal(), false);
        QCOMPARE(prop.needsNotifySignal(), false);
        QCOMPARE(prop.connectNotifySignal(0, SLOT(deleteLater())), false);
        QCOMPARE(prop.connectNotifySignal(obj, SLOT(deleteLater())), false);
        QCOMPARE(prop.connectNotifySignal(obj, 0), false);
        QCOMPARE(prop.connectNotifySignal(0, obj->metaObject()->indexOfMethod("deleteLater()")), false);
        QCOMPARE(prop.connectNotifySignal(obj, obj->metaObject()->indexOfMethod("deleteLater()")), false);
        QCOMPARE(prop.connectNotifySignal(obj, -1), false);
        QCOMPARE(QString(prop.method().methodSignature()), QString("clicked()"));
        QCOMPARE(prop.type(), QDeclarativeProperty::SignalProperty);
        QCOMPARE(prop.isProperty(), false);
        QCOMPARE(prop.isWritable(), false);
        QCOMPARE(prop.isDesignable(), false);
        QCOMPARE(prop.isResettable(), false);
        QCOMPARE(prop.isSignalProperty(), true);
        QCOMPARE(prop.isValid(), true);
        QCOMPARE(prop.object(), qobject_cast<QObject*>(&dobject));
        QCOMPARE(prop.propertyTypeCategory(), QDeclarativeProperty::InvalidCategory);
        QCOMPARE(prop.propertyType(), 0);
        QCOMPARE(prop.propertyTypeName(), (const char *)0);
        QCOMPARE(prop.property().name(), (const char *)0);
        QVERIFY(QDeclarativePropertyPrivate::binding(prop) == 0);
        QVERIFY(QDeclarativePropertyPrivate::setBinding(prop, binding.data()) == 0);
        QVERIFY(binding == 0);
        QVERIFY(QDeclarativePropertyPrivate::signalExpression(prop) == 0);
        QVERIFY(QDeclarativePropertyPrivate::setSignalExpression(prop, expression.data()) == 0);
        QVERIFY(expression != 0);
        QVERIFY(QDeclarativePropertyPrivate::signalExpression(prop) == expression.data());
        QCOMPARE(prop.index(), dobject.metaObject()->indexOfMethod("clicked()"));
        QCOMPARE(QDeclarativePropertyPrivate::valueTypeCoreIndex(prop), -1);

        delete obj;
    }

    {
        QDeclarativeProperty prop(&dobject, QString("onPropertyWithNotifyChanged"));

        QWeakPointer<QDeclarativeBinding> binding(new QDeclarativeBinding(QLatin1String("null"), 0, engine.rootContext()));
        binding.data()->setTarget(prop);
        QVERIFY(binding != 0);
        QWeakPointer<QDeclarativeExpression> expression(new QDeclarativeExpression());
        QVERIFY(expression != 0);

        QObject *obj = new QObject;

        QCOMPARE(prop.name(), QString("onOddlyNamedNotifySignal"));
        QCOMPARE(prop.read(), QVariant());
        QCOMPARE(prop.write(QVariant("Hello")), false);
        QCOMPARE(prop.hasNotifySignal(), false);
        QCOMPARE(prop.needsNotifySignal(), false);
        QCOMPARE(prop.connectNotifySignal(0, SLOT(deleteLater())), false);
        QCOMPARE(prop.connectNotifySignal(obj, SLOT(deleteLater())), false);
        QCOMPARE(prop.connectNotifySignal(obj, 0), false);
        QCOMPARE(prop.connectNotifySignal(0, obj->metaObject()->indexOfMethod("deleteLater()")), false);
        QCOMPARE(prop.connectNotifySignal(obj, obj->metaObject()->indexOfMethod("deleteLater()")), false);
        QCOMPARE(prop.connectNotifySignal(obj, -1), false);
        QCOMPARE(QString(prop.method().methodSignature()), QString("oddlyNamedNotifySignal()"));
        QCOMPARE(prop.type(), QDeclarativeProperty::SignalProperty);
        QCOMPARE(prop.isProperty(), false);
        QCOMPARE(prop.isWritable(), false);
        QCOMPARE(prop.isDesignable(), false);
        QCOMPARE(prop.isResettable(), false);
        QCOMPARE(prop.isSignalProperty(), true);
        QCOMPARE(prop.isValid(), true);
        QCOMPARE(prop.object(), qobject_cast<QObject*>(&dobject));
        QCOMPARE(prop.propertyTypeCategory(), QDeclarativeProperty::InvalidCategory);
        QCOMPARE(prop.propertyType(), 0);
        QCOMPARE(prop.propertyTypeName(), (const char *)0);
        QCOMPARE(prop.property().name(), (const char *)0);
        QVERIFY(QDeclarativePropertyPrivate::binding(prop) == 0);
        QVERIFY(QDeclarativePropertyPrivate::setBinding(prop, binding.data()) == 0);
        QVERIFY(binding == 0);
        QVERIFY(QDeclarativePropertyPrivate::signalExpression(prop) == 0);
        QVERIFY(QDeclarativePropertyPrivate::setSignalExpression(prop, expression.data()) == 0);
        QVERIFY(expression != 0);
        QVERIFY(QDeclarativePropertyPrivate::signalExpression(prop) == expression.data());
        QCOMPARE(prop.index(), dobject.metaObject()->indexOfMethod("oddlyNamedNotifySignal()"));
        QCOMPARE(QDeclarativePropertyPrivate::valueTypeCoreIndex(prop), -1);

        delete obj;
    }
}

void tst_qdeclarativeproperty::qmlmetaproperty_object_context()
{
    QObject object; // Has no default property
    PropertyObject dobject; // Has default property

    {
        QDeclarativeProperty prop(&object, engine.rootContext());

        QWeakPointer<QDeclarativeBinding> binding(new QDeclarativeBinding(QLatin1String("null"), 0, engine.rootContext()));
        QVERIFY(binding != 0);
        QWeakPointer<QDeclarativeExpression> expression(new QDeclarativeExpression());
        QVERIFY(expression != 0);

        QObject *obj = new QObject;

        QCOMPARE(prop.name(), QString());
        QCOMPARE(prop.read(), QVariant());
        QCOMPARE(prop.write(QVariant()), false);
        QCOMPARE(prop.hasNotifySignal(), false);
        QCOMPARE(prop.needsNotifySignal(), false);
        QCOMPARE(prop.connectNotifySignal(0, SLOT(deleteLater())), false);
        QCOMPARE(prop.connectNotifySignal(obj, SLOT(deleteLater())), false);
        QCOMPARE(prop.connectNotifySignal(obj, 0), false);
        QCOMPARE(prop.connectNotifySignal(0, obj->metaObject()->indexOfMethod("deleteLater()")), false);
        QCOMPARE(prop.connectNotifySignal(obj, obj->metaObject()->indexOfMethod("deleteLater()")), false);
        QCOMPARE(prop.connectNotifySignal(obj, -1), false);
        QVERIFY(!prop.method().isValid());
        QCOMPARE(prop.type(), QDeclarativeProperty::Invalid);
        QCOMPARE(prop.isProperty(), false);
        QCOMPARE(prop.isWritable(), false);
        QCOMPARE(prop.isDesignable(), false);
        QCOMPARE(prop.isResettable(), false);
        QCOMPARE(prop.isSignalProperty(), false);
        QCOMPARE(prop.isValid(), false);
        QCOMPARE(prop.object(), (QObject *)0);
        QCOMPARE(prop.propertyTypeCategory(), QDeclarativeProperty::InvalidCategory);
        QCOMPARE(prop.propertyType(), 0);
        QCOMPARE(prop.propertyTypeName(), (const char *)0);
        QVERIFY(prop.property().name() == 0);
        QVERIFY(QDeclarativePropertyPrivate::binding(prop) == 0);
        QVERIFY(QDeclarativePropertyPrivate::setBinding(prop, binding.data()) == 0);
        QVERIFY(binding == 0);
        QVERIFY(QDeclarativePropertyPrivate::signalExpression(prop) == 0);
        QVERIFY(QDeclarativePropertyPrivate::setSignalExpression(prop, expression.data()) == 0);
        QVERIFY(expression == 0);
        QCOMPARE(prop.index(), -1);
        QCOMPARE(QDeclarativePropertyPrivate::valueTypeCoreIndex(prop), -1);

        delete obj;
    }

    {
        QDeclarativeProperty prop(&dobject, engine.rootContext());

        QWeakPointer<QDeclarativeBinding> binding(new QDeclarativeBinding(QLatin1String("null"), 0, engine.rootContext()));
        binding.data()->setTarget(prop);
        QVERIFY(binding != 0);
        QWeakPointer<QDeclarativeExpression> expression(new QDeclarativeExpression());
        QVERIFY(expression != 0);

        QObject *obj = new QObject;

        QCOMPARE(prop.name(), QString("defaultProperty"));
        QCOMPARE(prop.read(), QVariant(10));
        QCOMPARE(prop.write(QVariant()), false);
        QCOMPARE(prop.hasNotifySignal(), false);
        QCOMPARE(prop.needsNotifySignal(), true);
        QCOMPARE(prop.connectNotifySignal(0, SLOT(deleteLater())), false);
        QCOMPARE(prop.connectNotifySignal(obj, SLOT(deleteLater())), false);
        QCOMPARE(prop.connectNotifySignal(obj, 0), false);
        QCOMPARE(prop.connectNotifySignal(0, obj->metaObject()->indexOfMethod("deleteLater()")), false);
        QCOMPARE(prop.connectNotifySignal(obj, obj->metaObject()->indexOfMethod("deleteLater()")), false);
        QCOMPARE(prop.connectNotifySignal(obj, -1), false);
        QVERIFY(!prop.method().isValid());
        QCOMPARE(prop.type(), QDeclarativeProperty::Property);
        QCOMPARE(prop.isProperty(), true);
        QCOMPARE(prop.isWritable(), false);
        QCOMPARE(prop.isDesignable(), true);
        QCOMPARE(prop.isResettable(), false);
        QCOMPARE(prop.isSignalProperty(), false);
        QCOMPARE(prop.isValid(), true);
        QCOMPARE(prop.object(), qobject_cast<QObject*>(&dobject));
        QCOMPARE(prop.propertyTypeCategory(), QDeclarativeProperty::Normal);
        QCOMPARE(prop.propertyType(), (int)QVariant::Int);
        QCOMPARE(prop.propertyTypeName(), "int");
        QCOMPARE(QString(prop.property().name()), QString("defaultProperty"));
        QVERIFY(QDeclarativePropertyPrivate::binding(prop) == 0);
        QTest::ignoreMessage(QtWarningMsg, "<Unknown File>: Unable to assign null to int");
        QVERIFY(QDeclarativePropertyPrivate::setBinding(prop, binding.data()) == 0);
        QVERIFY(binding != 0);
        QVERIFY(QDeclarativePropertyPrivate::binding(prop) == binding.data());
        QVERIFY(QDeclarativePropertyPrivate::signalExpression(prop) == 0);
        QVERIFY(QDeclarativePropertyPrivate::setSignalExpression(prop, expression.data()) == 0);
        QVERIFY(expression == 0);
        QCOMPARE(prop.index(), dobject.metaObject()->indexOfProperty("defaultProperty"));
        QCOMPARE(QDeclarativePropertyPrivate::valueTypeCoreIndex(prop), -1);

        delete obj;
    }
}

void tst_qdeclarativeproperty::qmlmetaproperty_object_string_context()
{
    QObject object;
    PropertyObject dobject;

    {
        QDeclarativeProperty prop(&object, QString("defaultProperty"), engine.rootContext());

        QWeakPointer<QDeclarativeBinding> binding(new QDeclarativeBinding(QLatin1String("null"), 0, engine.rootContext()));
        QVERIFY(binding != 0);
        QWeakPointer<QDeclarativeExpression> expression(new QDeclarativeExpression());
        QVERIFY(expression != 0);

        QObject *obj = new QObject;

        QCOMPARE(prop.name(), QString());
        QCOMPARE(prop.read(), QVariant());
        QCOMPARE(prop.write(QVariant()), false);
        QCOMPARE(prop.hasNotifySignal(), false);
        QCOMPARE(prop.needsNotifySignal(), false);
        QCOMPARE(prop.connectNotifySignal(0, SLOT(deleteLater())), false);
        QCOMPARE(prop.connectNotifySignal(obj, SLOT(deleteLater())), false);
        QCOMPARE(prop.connectNotifySignal(obj, 0), false);
        QCOMPARE(prop.connectNotifySignal(0, obj->metaObject()->indexOfMethod("deleteLater()")), false);
        QCOMPARE(prop.connectNotifySignal(obj, obj->metaObject()->indexOfMethod("deleteLater()")), false);
        QCOMPARE(prop.connectNotifySignal(obj, -1), false);
        QVERIFY(!prop.method().isValid());
        QCOMPARE(prop.type(), QDeclarativeProperty::Invalid);
        QCOMPARE(prop.isProperty(), false);
        QCOMPARE(prop.isWritable(), false);
        QCOMPARE(prop.isDesignable(), false);
        QCOMPARE(prop.isResettable(), false);
        QCOMPARE(prop.isSignalProperty(), false);
        QCOMPARE(prop.isValid(), false);
        QCOMPARE(prop.object(), (QObject *)0);
        QCOMPARE(prop.propertyTypeCategory(), QDeclarativeProperty::InvalidCategory);
        QCOMPARE(prop.propertyType(), 0);
        QCOMPARE(prop.propertyTypeName(), (const char *)0);
        QVERIFY(prop.property().name() == 0);
        QVERIFY(QDeclarativePropertyPrivate::binding(prop) == 0);
        QVERIFY(QDeclarativePropertyPrivate::setBinding(prop, binding.data()) == 0);
        QVERIFY(binding == 0);
        QVERIFY(QDeclarativePropertyPrivate::signalExpression(prop) == 0);
        QVERIFY(QDeclarativePropertyPrivate::setSignalExpression(prop, expression.data()) == 0);
        QVERIFY(expression == 0);
        QCOMPARE(prop.index(), -1);
        QCOMPARE(QDeclarativePropertyPrivate::valueTypeCoreIndex(prop), -1);

        delete obj;
    }

    {
        QDeclarativeProperty prop(&dobject, QString("defaultProperty"), engine.rootContext());

        QWeakPointer<QDeclarativeBinding> binding(new QDeclarativeBinding(QLatin1String("null"), 0, engine.rootContext()));
        binding.data()->setTarget(prop);
        QVERIFY(binding != 0);
        QWeakPointer<QDeclarativeExpression> expression(new QDeclarativeExpression());
        QVERIFY(expression != 0);

        QObject *obj = new QObject;

        QCOMPARE(prop.name(), QString("defaultProperty"));
        QCOMPARE(prop.read(), QVariant(10));
        QCOMPARE(prop.write(QVariant()), false);
        QCOMPARE(prop.hasNotifySignal(), false);
        QCOMPARE(prop.needsNotifySignal(), true);
        QCOMPARE(prop.connectNotifySignal(0, SLOT(deleteLater())), false);
        QCOMPARE(prop.connectNotifySignal(obj, SLOT(deleteLater())), false);
        QCOMPARE(prop.connectNotifySignal(obj, 0), false);
        QCOMPARE(prop.connectNotifySignal(0, obj->metaObject()->indexOfMethod("deleteLater()")), false);
        QCOMPARE(prop.connectNotifySignal(obj, obj->metaObject()->indexOfMethod("deleteLater()")), false);
        QCOMPARE(prop.connectNotifySignal(obj, -1), false);
        QVERIFY(!prop.method().isValid());
        QCOMPARE(prop.type(), QDeclarativeProperty::Property);
        QCOMPARE(prop.isProperty(), true);
        QCOMPARE(prop.isWritable(), false);
        QCOMPARE(prop.isDesignable(), true);
        QCOMPARE(prop.isResettable(), false);
        QCOMPARE(prop.isSignalProperty(), false);
        QCOMPARE(prop.isValid(), true);
        QCOMPARE(prop.object(), qobject_cast<QObject*>(&dobject));
        QCOMPARE(prop.propertyTypeCategory(), QDeclarativeProperty::Normal);
        QCOMPARE(prop.propertyType(), (int)QVariant::Int);
        QCOMPARE(prop.propertyTypeName(), "int");
        QCOMPARE(QString(prop.property().name()), QString("defaultProperty"));
        QVERIFY(QDeclarativePropertyPrivate::binding(prop) == 0);
        QTest::ignoreMessage(QtWarningMsg, "<Unknown File>: Unable to assign null to int");
        QVERIFY(QDeclarativePropertyPrivate::setBinding(prop, binding.data()) == 0);
        QVERIFY(binding != 0);
        QVERIFY(QDeclarativePropertyPrivate::binding(prop) == binding.data());
        QVERIFY(QDeclarativePropertyPrivate::signalExpression(prop) == 0);
        QVERIFY(QDeclarativePropertyPrivate::setSignalExpression(prop, expression.data()) == 0);
        QVERIFY(expression == 0);
        QCOMPARE(prop.index(), dobject.metaObject()->indexOfProperty("defaultProperty"));
        QCOMPARE(QDeclarativePropertyPrivate::valueTypeCoreIndex(prop), -1);

        delete obj;
    }

    {
        QDeclarativeProperty prop(&dobject, QString("onClicked"), engine.rootContext());

        QWeakPointer<QDeclarativeBinding> binding(new QDeclarativeBinding(QLatin1String("null"), 0, engine.rootContext()));
        binding.data()->setTarget(prop);
        QVERIFY(binding != 0);
        QWeakPointer<QDeclarativeExpression> expression(new QDeclarativeExpression());
        QVERIFY(expression != 0);

        QObject *obj = new QObject;

        QCOMPARE(prop.name(), QString("onClicked"));
        QCOMPARE(prop.read(), QVariant());
        QCOMPARE(prop.write(QVariant("Hello")), false);
        QCOMPARE(prop.hasNotifySignal(), false);
        QCOMPARE(prop.needsNotifySignal(), false);
        QCOMPARE(prop.connectNotifySignal(0, SLOT(deleteLater())), false);
        QCOMPARE(prop.connectNotifySignal(obj, SLOT(deleteLater())), false);
        QCOMPARE(prop.connectNotifySignal(obj, 0), false);
        QCOMPARE(prop.connectNotifySignal(0, obj->metaObject()->indexOfMethod("deleteLater()")), false);
        QCOMPARE(prop.connectNotifySignal(obj, obj->metaObject()->indexOfMethod("deleteLater()")), false);
        QCOMPARE(prop.connectNotifySignal(obj, -1), false);
        QCOMPARE(QString(prop.method().methodSignature()), QString("clicked()"));
        QCOMPARE(prop.type(), QDeclarativeProperty::SignalProperty);
        QCOMPARE(prop.isProperty(), false);
        QCOMPARE(prop.isWritable(), false);
        QCOMPARE(prop.isDesignable(), false);
        QCOMPARE(prop.isResettable(), false);
        QCOMPARE(prop.isSignalProperty(), true);
        QCOMPARE(prop.isValid(), true);
        QCOMPARE(prop.object(), qobject_cast<QObject*>(&dobject));
        QCOMPARE(prop.propertyTypeCategory(), QDeclarativeProperty::InvalidCategory);
        QCOMPARE(prop.propertyType(), 0);
        QCOMPARE(prop.propertyTypeName(), (const char *)0);
        QCOMPARE(prop.property().name(), (const char *)0);
        QVERIFY(QDeclarativePropertyPrivate::binding(prop) == 0);
        QVERIFY(QDeclarativePropertyPrivate::setBinding(prop, binding.data()) == 0);
        QVERIFY(binding == 0);
        QVERIFY(QDeclarativePropertyPrivate::signalExpression(prop) == 0);
        QVERIFY(QDeclarativePropertyPrivate::setSignalExpression(prop, expression.data()) == 0);
        QVERIFY(expression != 0);
        QVERIFY(QDeclarativePropertyPrivate::signalExpression(prop) == expression.data());
        QCOMPARE(prop.index(), dobject.metaObject()->indexOfMethod("clicked()"));
        QCOMPARE(QDeclarativePropertyPrivate::valueTypeCoreIndex(prop), -1);

        delete obj;
    }

    {
        QDeclarativeProperty prop(&dobject, QString("onPropertyWithNotifyChanged"), engine.rootContext());

        QWeakPointer<QDeclarativeBinding> binding(new QDeclarativeBinding(QLatin1String("null"), 0, engine.rootContext()));
        binding.data()->setTarget(prop);
        QVERIFY(binding != 0);
        QWeakPointer<QDeclarativeExpression> expression(new QDeclarativeExpression());
        QVERIFY(expression != 0);

        QObject *obj = new QObject;

        QCOMPARE(prop.name(), QString("onOddlyNamedNotifySignal"));
        QCOMPARE(prop.read(), QVariant());
        QCOMPARE(prop.write(QVariant("Hello")), false);
        QCOMPARE(prop.hasNotifySignal(), false);
        QCOMPARE(prop.needsNotifySignal(), false);
        QCOMPARE(prop.connectNotifySignal(0, SLOT(deleteLater())), false);
        QCOMPARE(prop.connectNotifySignal(obj, SLOT(deleteLater())), false);
        QCOMPARE(prop.connectNotifySignal(obj, 0), false);
        QCOMPARE(prop.connectNotifySignal(0, obj->metaObject()->indexOfMethod("deleteLater()")), false);
        QCOMPARE(prop.connectNotifySignal(obj, obj->metaObject()->indexOfMethod("deleteLater()")), false);
        QCOMPARE(prop.connectNotifySignal(obj, -1), false);
        QCOMPARE(QString(prop.method().methodSignature()), QString("oddlyNamedNotifySignal()"));
        QCOMPARE(prop.type(), QDeclarativeProperty::SignalProperty);
        QCOMPARE(prop.isProperty(), false);
        QCOMPARE(prop.isWritable(), false);
        QCOMPARE(prop.isDesignable(), false);
        QCOMPARE(prop.isResettable(), false);
        QCOMPARE(prop.isSignalProperty(), true);
        QCOMPARE(prop.isValid(), true);
        QCOMPARE(prop.object(), qobject_cast<QObject*>(&dobject));
        QCOMPARE(prop.propertyTypeCategory(), QDeclarativeProperty::InvalidCategory);
        QCOMPARE(prop.propertyType(), 0);
        QCOMPARE(prop.propertyTypeName(), (const char *)0);
        QCOMPARE(prop.property().name(), (const char *)0);
        QVERIFY(QDeclarativePropertyPrivate::binding(prop) == 0);
        QVERIFY(QDeclarativePropertyPrivate::setBinding(prop, binding.data()) == 0);
        QVERIFY(binding == 0);
        QVERIFY(QDeclarativePropertyPrivate::signalExpression(prop) == 0);
        QVERIFY(QDeclarativePropertyPrivate::setSignalExpression(prop, expression.data()) == 0);
        QVERIFY(expression != 0);
        QVERIFY(QDeclarativePropertyPrivate::signalExpression(prop) == expression.data());
        QCOMPARE(prop.index(), dobject.metaObject()->indexOfMethod("oddlyNamedNotifySignal()"));
        QCOMPARE(QDeclarativePropertyPrivate::valueTypeCoreIndex(prop), -1);

        delete obj;
    }
}

void tst_qdeclarativeproperty::name()
{
    {
        QDeclarativeProperty p;
        QCOMPARE(p.name(), QString());
    }

    {
        PropertyObject o;
        QDeclarativeProperty p(&o);
        QCOMPARE(p.name(), QString("defaultProperty"));
    }

    {
        QObject o;
        QDeclarativeProperty p(&o, QString("objectName"));
        QCOMPARE(p.name(), QString("objectName"));
    }

    {
        PropertyObject o;
        QDeclarativeProperty p(&o, "onClicked");
        QCOMPARE(p.name(), QString("onClicked"));
    }

    {
        QObject o;
        QDeclarativeProperty p(&o, "onClicked");
        QCOMPARE(p.name(), QString());
    }

    {
        PropertyObject o;
        QDeclarativeProperty p(&o, "onPropertyWithNotifyChanged");
        QCOMPARE(p.name(), QString("onOddlyNamedNotifySignal"));
    }

    {
        QObject o;
        QDeclarativeProperty p(&o, "onPropertyWithNotifyChanged");
        QCOMPARE(p.name(), QString());
    }

    {
        QObject o;
        QDeclarativeProperty p(&o, "foo");
        QCOMPARE(p.name(), QString());
    }

    {
        QDeclarativeProperty p(0, "foo");
        QCOMPARE(p.name(), QString());
    }

    {
        PropertyObject o;
        QDeclarativeProperty p(&o, "rectProperty");
        QCOMPARE(p.name(), QString("rectProperty"));
    }

    {
        PropertyObject o;
        QDeclarativeProperty p(&o, "rectProperty.x");
        QCOMPARE(p.name(), QString("rectProperty.x"));
    }

    {
        PropertyObject o;
        QDeclarativeProperty p(&o, "rectProperty.foo");
        QCOMPARE(p.name(), QString());
    }
}

void tst_qdeclarativeproperty::read()
{
    // Invalid
    {
        QDeclarativeProperty p;
        QCOMPARE(p.read(), QVariant());
    }

    // Default prop
    {
        PropertyObject o;
        QDeclarativeProperty p(&o);
        QCOMPARE(p.read(), QVariant(10));
    }

    // Invalid default prop
    {
        QObject o;
        QDeclarativeProperty p(&o);
        QCOMPARE(p.read(), QVariant());
    }

    // Value prop by name
    {
        QObject o;

        QDeclarativeProperty p(&o, "objectName");
        QCOMPARE(p.read(), QVariant(QString()));

        o.setObjectName("myName");

        QCOMPARE(p.read(), QVariant("myName"));
    }

    // Value prop by name (static)
    {
        QObject o;

        QCOMPARE(QDeclarativeProperty::read(&o, "objectName"), QVariant(QString()));

        o.setObjectName("myName");

        QCOMPARE(QDeclarativeProperty::read(&o, "objectName"), QVariant("myName"));
    }

    // Value-type prop
    {
        PropertyObject o;
        QDeclarativeProperty p(&o, "rectProperty.x");
        QCOMPARE(p.read(), QVariant(10));
    }

    // Invalid value-type prop
    {
        PropertyObject o;
        QDeclarativeProperty p(&o, "rectProperty.foo");
        QCOMPARE(p.read(), QVariant());
    }

    // Signal property
    {
        PropertyObject o;
        QDeclarativeProperty p(&o, "onClicked");
        QCOMPARE(p.read(), QVariant());

        QVERIFY(0 == QDeclarativePropertyPrivate::setSignalExpression(p, new QDeclarativeExpression()));
        QVERIFY(0 != QDeclarativePropertyPrivate::signalExpression(p));

        QCOMPARE(p.read(), QVariant());
    }

    // Automatic signal property
    {
        PropertyObject o;
        QDeclarativeProperty p(&o, "onPropertyWithNotifyChanged");
        QCOMPARE(p.read(), QVariant());

        QVERIFY(0 == QDeclarativePropertyPrivate::setSignalExpression(p, new QDeclarativeExpression()));
        QVERIFY(0 != QDeclarativePropertyPrivate::signalExpression(p));

        QCOMPARE(p.read(), QVariant());
    }

    // Deleted object
    {
        PropertyObject *o = new PropertyObject;
        QDeclarativeProperty p(o, "rectProperty.x");
        QCOMPARE(p.read(), QVariant(10));
        delete o;
        QCOMPARE(p.read(), QVariant());
    }

    // Object property
    {
        PropertyObject o;
        QDeclarativeProperty p(&o, "qmlObject");
        QCOMPARE(p.propertyTypeCategory(), QDeclarativeProperty::Object);
        QCOMPARE(p.propertyType(), qMetaTypeId<MyQmlObject*>());
        QVariant v = p.read();
        QVERIFY(v.userType() == QMetaType::QObjectStar);
        QVERIFY(qvariant_cast<QObject *>(v) == o.qmlObject());
    }
    {
        QDeclarativeComponent component(&engine, testFileUrl("readSynthesizedObject.qml"));
        QObject *object = component.create();
        QVERIFY(object != 0);

        QDeclarativeProperty p(object, "test", &engine);

        QCOMPARE(p.propertyTypeCategory(), QDeclarativeProperty::Object);
        QVERIFY(p.propertyType() != QMetaType::QObjectStar);

        QVariant v = p.read();
        QVERIFY(v.userType() == QMetaType::QObjectStar);
        QCOMPARE(qvariant_cast<QObject *>(v)->property("a").toInt(), 10);
        QCOMPARE(qvariant_cast<QObject *>(v)->property("b").toInt(), 19);
    }
    {   // static
        QDeclarativeComponent component(&engine, testFileUrl("readSynthesizedObject.qml"));
        QObject *object = component.create();
        QVERIFY(object != 0);

        QVariant v = QDeclarativeProperty::read(object, "test", &engine);
        QVERIFY(v.userType() == QMetaType::QObjectStar);
        QCOMPARE(qvariant_cast<QObject *>(v)->property("a").toInt(), 10);
        QCOMPARE(qvariant_cast<QObject *>(v)->property("b").toInt(), 19);
    }

    // Attached property
    {
        QDeclarativeComponent component(&engine);
        component.setData("import Test 1.0\nMyContainer { }", QUrl());
        QObject *object = component.create();
        QVERIFY(object != 0);

        QDeclarativeProperty p(object, "MyContainer.foo", qmlContext(object));
        QCOMPARE(p.read(), QVariant(13));
        delete object;
    }
    {
        QDeclarativeComponent component(&engine);
        component.setData("import Test 1.0\nMyContainer { MyContainer.foo: 10 }", QUrl());
        QObject *object = component.create();
        QVERIFY(object != 0);

        QDeclarativeProperty p(object, "MyContainer.foo", qmlContext(object));
        QCOMPARE(p.read(), QVariant(10));
        delete object;
    }
    {
        QDeclarativeComponent component(&engine);
        component.setData("import Test 1.0 as Foo\nFoo.MyContainer { Foo.MyContainer.foo: 10 }", QUrl());
        QObject *object = component.create();
        QVERIFY(object != 0);

        QDeclarativeProperty p(object, "Foo.MyContainer.foo", qmlContext(object));
        QCOMPARE(p.read(), QVariant(10));
        delete object;
    }
    {   // static
        QDeclarativeComponent component(&engine);
        component.setData("import Test 1.0 as Foo\nFoo.MyContainer { Foo.MyContainer.foo: 10 }", QUrl());
        QObject *object = component.create();
        QVERIFY(object != 0);

        QCOMPARE(QDeclarativeProperty::read(object, "Foo.MyContainer.foo", qmlContext(object)), QVariant(10));
        delete object;
    }
}

void tst_qdeclarativeproperty::write()
{
    // Invalid
    {
        QDeclarativeProperty p;
        QCOMPARE(p.write(QVariant(10)), false);
    }

    // Read-only default prop
    {
        PropertyObject o;
        QDeclarativeProperty p(&o);
        QCOMPARE(p.write(QVariant(10)), false);
    }

    // Invalid default prop
    {
        QObject o;
        QDeclarativeProperty p(&o);
        QCOMPARE(p.write(QVariant(10)), false);
    }

    // Read-only prop by name
    {
        PropertyObject o;
        QDeclarativeProperty p(&o, QString("defaultProperty"));
        QCOMPARE(p.write(QVariant(10)), false);
    }

    // Writable prop by name
    {
        PropertyObject o;
        QDeclarativeProperty p(&o, QString("objectName"));
        QCOMPARE(o.objectName(), QString());
        QCOMPARE(p.write(QVariant(QString("myName"))), true);
        QCOMPARE(o.objectName(), QString("myName"));
    }

    // Writable prop by name (static)
    {
        PropertyObject o;
        QCOMPARE(QDeclarativeProperty::write(&o, QString("objectName"), QVariant(QString("myName"))), true);
        QCOMPARE(o.objectName(), QString("myName"));
    }

    // Deleted object
    {
        PropertyObject *o = new PropertyObject;
        QDeclarativeProperty p(o, QString("objectName"));
        QCOMPARE(p.write(QVariant(QString("myName"))), true);
        QCOMPARE(o->objectName(), QString("myName"));

        delete o;

        QCOMPARE(p.write(QVariant(QString("myName"))), false);
    }

    // Signal property
    {
        PropertyObject o;
        QDeclarativeProperty p(&o, "onClicked");
        QCOMPARE(p.write(QVariant("console.log(1921)")), false);

        QVERIFY(0 == QDeclarativePropertyPrivate::setSignalExpression(p, new QDeclarativeExpression()));
        QVERIFY(0 != QDeclarativePropertyPrivate::signalExpression(p));

        QCOMPARE(p.write(QVariant("console.log(1921)")), false);

        QVERIFY(0 != QDeclarativePropertyPrivate::signalExpression(p));
    }

    // Automatic signal property
    {
        PropertyObject o;
        QDeclarativeProperty p(&o, "onPropertyWithNotifyChanged");
        QCOMPARE(p.write(QVariant("console.log(1921)")), false);

        QVERIFY(0 == QDeclarativePropertyPrivate::setSignalExpression(p, new QDeclarativeExpression()));
        QVERIFY(0 != QDeclarativePropertyPrivate::signalExpression(p));

        QCOMPARE(p.write(QVariant("console.log(1921)")), false);

        QVERIFY(0 != QDeclarativePropertyPrivate::signalExpression(p));
    }

    // Value-type property
    {
        PropertyObject o;
        QDeclarativeProperty p(&o, "wrectProperty");

        QCOMPARE(o.wrectProperty(), QRect());
        QCOMPARE(p.write(QRect(1, 13, 99, 8)), true);
        QCOMPARE(o.wrectProperty(), QRect(1, 13, 99, 8));

        QDeclarativeProperty p2(&o, "wrectProperty.x");
        QCOMPARE(p2.read(), QVariant(1));
        QCOMPARE(p2.write(QVariant(6)), true);
        QCOMPARE(p2.read(), QVariant(6));
        QCOMPARE(o.wrectProperty(), QRect(6, 13, 99, 8));
    }

    // URL-property
    {
        PropertyObject o;
        QDeclarativeProperty p(&o, "url");

        QCOMPARE(p.write(QUrl("main.qml")), true);
        QCOMPARE(o.url(), QUrl("main.qml"));

        QDeclarativeProperty p2(&o, "url", engine.rootContext());

        QUrl result = engine.baseUrl().resolved(QUrl("main.qml"));
        QVERIFY(result != QUrl("main.qml"));

        QCOMPARE(p2.write(QUrl("main.qml")), true);
        QCOMPARE(o.url(), result);
    }
    {   // static
        PropertyObject o;

        QCOMPARE(QDeclarativeProperty::write(&o, "url", QUrl("main.qml")), true);
        QCOMPARE(o.url(), QUrl("main.qml"));

        QUrl result = engine.baseUrl().resolved(QUrl("main.qml"));
        QVERIFY(result != QUrl("main.qml"));

        QCOMPARE(QDeclarativeProperty::write(&o, "url", QUrl("main.qml"), engine.rootContext()), true);
        QCOMPARE(o.url(), result);
    }

    // Attached property
    {
        QDeclarativeComponent component(&engine);
        component.setData("import Test 1.0\nMyContainer { }", QUrl());
        QObject *object = component.create();
        QVERIFY(object != 0);

        QDeclarativeProperty p(object, "MyContainer.foo", qmlContext(object));
        p.write(QVariant(99));
        QCOMPARE(p.read(), QVariant(99));
        delete object;
    }
    {
        QDeclarativeComponent component(&engine);
        component.setData("import Test 1.0 as Foo\nFoo.MyContainer { Foo.MyContainer.foo: 10 }", QUrl());
        QObject *object = component.create();
        QVERIFY(object != 0);

        QDeclarativeProperty p(object, "Foo.MyContainer.foo", qmlContext(object));
        p.write(QVariant(99));
        QCOMPARE(p.read(), QVariant(99));
        delete object;
    }
}

void tst_qdeclarativeproperty::reset()
{
    // Invalid
    {
        QDeclarativeProperty p;
        QCOMPARE(p.isResettable(), false);
        QCOMPARE(p.reset(), false);
    }

    // Read-only default prop
    {
        PropertyObject o;
        QDeclarativeProperty p(&o);
        QCOMPARE(p.isResettable(), false);
        QCOMPARE(p.reset(), false);
    }

    // Invalid default prop
    {
        QObject o;
        QDeclarativeProperty p(&o);
        QCOMPARE(p.isResettable(), false);
        QCOMPARE(p.reset(), false);
    }

    // Non-resettable-only prop by name
    {
        PropertyObject o;
        QDeclarativeProperty p(&o, QString("defaultProperty"));
        QCOMPARE(p.isResettable(), false);
        QCOMPARE(p.reset(), false);
    }

    // Resettable prop by name
    {
        PropertyObject o;
        QDeclarativeProperty p(&o, QString("resettableProperty"));

        QCOMPARE(p.read(), QVariant(9));
        QCOMPARE(p.write(QVariant(11)), true);
        QCOMPARE(p.read(), QVariant(11));

        QCOMPARE(p.isResettable(), true);
        QCOMPARE(p.reset(), true);

        QCOMPARE(p.read(), QVariant(9));
    }

    // Deleted object
    {
        PropertyObject *o = new PropertyObject;

        QDeclarativeProperty p(o, QString("resettableProperty"));

        QCOMPARE(p.isResettable(), true);
        QCOMPARE(p.reset(), true);

        delete o;

        QCOMPARE(p.isResettable(), false);
        QCOMPARE(p.reset(), false);
    }

    // Signal property
    {
        PropertyObject o;
        QDeclarativeProperty p(&o, "onClicked");

        QCOMPARE(p.isResettable(), false);
        QCOMPARE(p.reset(), false);
    }

    // Automatic signal property
    {
        PropertyObject o;
        QDeclarativeProperty p(&o, "onPropertyWithNotifyChanged");

        QCOMPARE(p.isResettable(), false);
        QCOMPARE(p.reset(), false);
    }
}

void tst_qdeclarativeproperty::writeObjectToList()
{
    QDeclarativeComponent containerComponent(&engine);
    containerComponent.setData("import Test 1.0\nMyContainer { children: MyQmlObject {} }", QUrl());
    MyContainer *container = qobject_cast<MyContainer*>(containerComponent.create());
    QVERIFY(container != 0);
    QDeclarativeListReference list(container, "children");
    QVERIFY(list.count() == 1);

    MyQmlObject *object = new MyQmlObject;
    QDeclarativeProperty prop(container, "children");
    prop.write(qVariantFromValue(object));
    QCOMPARE(list.count(), 1);
    QCOMPARE(list.at(0), qobject_cast<QObject*>(object));
}

Q_DECLARE_METATYPE(QList<QObject *>);
void tst_qdeclarativeproperty::writeListToList()
{
    QDeclarativeComponent containerComponent(&engine);
    containerComponent.setData("import Test 1.0\nMyContainer { children: MyQmlObject {} }", QUrl());
    MyContainer *container = qobject_cast<MyContainer*>(containerComponent.create());
    QVERIFY(container != 0);
    QDeclarativeListReference list(container, "children");
    QVERIFY(list.count() == 1);

    QList<QObject*> objList;
    objList << new MyQmlObject() << new MyQmlObject() << new MyQmlObject() << new MyQmlObject();
    QDeclarativeProperty prop(container, "children");
    prop.write(qVariantFromValue(objList));
    QCOMPARE(list.count(), 4);

    //XXX need to try this with read/write prop (for read-only it correctly doesn't write)
    /*QList<MyQmlObject*> typedObjList;
    typedObjList << new MyQmlObject();
    prop.write(qVariantFromValue(&typedObjList));
    QCOMPARE(container->children()->size(), 1);*/
}

void tst_qdeclarativeproperty::crashOnValueProperty()
{
    QDeclarativeEngine *engine = new QDeclarativeEngine;
    QDeclarativeComponent component(engine);

    component.setData("import Test 1.0\nPropertyObject { wrectProperty.x: 10 }", QUrl());
    PropertyObject *obj = qobject_cast<PropertyObject*>(component.create());
    QVERIFY(obj != 0);

    QDeclarativeProperty p(obj, "wrectProperty.x", qmlContext(obj));
    QCOMPARE(p.name(), QString("wrectProperty.x"));

    QCOMPARE(p.read(), QVariant(10));

    //don't crash once the engine is deleted
    delete engine;
    engine = 0;

    QCOMPARE(p.propertyTypeName(), "int");
    QCOMPARE(p.read(), QVariant(10));
    p.write(QVariant(20));
    QCOMPARE(p.read(), QVariant(20));
}

// QTBUG-13719
void tst_qdeclarativeproperty::aliasPropertyBindings()
{
    QDeclarativeComponent component(&engine, testFileUrl("aliasPropertyBindings.qml"));

    QObject *object = component.create();
    QVERIFY(object != 0);

    QCOMPARE(object->property("realProperty").toReal(), 90.);
    QCOMPARE(object->property("aliasProperty").toReal(), 90.);

    object->setProperty("test", 10);

    QCOMPARE(object->property("realProperty").toReal(), 110.);
    QCOMPARE(object->property("aliasProperty").toReal(), 110.);

    QDeclarativeProperty realProperty(object, QLatin1String("realProperty"));
    QDeclarativeProperty aliasProperty(object, QLatin1String("aliasProperty"));

    // Check there is a binding on these two properties
    QVERIFY(QDeclarativePropertyPrivate::binding(realProperty) != 0);
    QVERIFY(QDeclarativePropertyPrivate::binding(aliasProperty) != 0);

    // Check that its the same binding on these two properties
    QCOMPARE(QDeclarativePropertyPrivate::binding(realProperty),
             QDeclarativePropertyPrivate::binding(aliasProperty));

    // Change the binding
    object->setProperty("state", QString("switch"));

    QVERIFY(QDeclarativePropertyPrivate::binding(realProperty) != 0);
    QVERIFY(QDeclarativePropertyPrivate::binding(aliasProperty) != 0);
    QCOMPARE(QDeclarativePropertyPrivate::binding(realProperty),
             QDeclarativePropertyPrivate::binding(aliasProperty));

    QCOMPARE(object->property("realProperty").toReal(), 96.);
    QCOMPARE(object->property("aliasProperty").toReal(), 96.);

    // Check the old binding really has not effect any more
    object->setProperty("test", 4);

    QCOMPARE(object->property("realProperty").toReal(), 96.);
    QCOMPARE(object->property("aliasProperty").toReal(), 96.);

    object->setProperty("test2", 9);

    QCOMPARE(object->property("realProperty").toReal(), 288.);
    QCOMPARE(object->property("aliasProperty").toReal(), 288.);

    // Revert
    object->setProperty("state", QString(""));

    QVERIFY(QDeclarativePropertyPrivate::binding(realProperty) != 0);
    QVERIFY(QDeclarativePropertyPrivate::binding(aliasProperty) != 0);
    QCOMPARE(QDeclarativePropertyPrivate::binding(realProperty),
             QDeclarativePropertyPrivate::binding(aliasProperty));

    QCOMPARE(object->property("realProperty").toReal(), 20.);
    QCOMPARE(object->property("aliasProperty").toReal(), 20.);

    object->setProperty("test2", 3);

    QCOMPARE(object->property("realProperty").toReal(), 20.);
    QCOMPARE(object->property("aliasProperty").toReal(), 20.);

    delete object;
}

void tst_qdeclarativeproperty::copy()
{
    PropertyObject object;

    QDeclarativeProperty *property = new QDeclarativeProperty(&object, QLatin1String("defaultProperty"));
    QCOMPARE(property->name(), QString("defaultProperty"));
    QCOMPARE(property->read(), QVariant(10));
    QCOMPARE(property->type(), QDeclarativeProperty::Property);
    QCOMPARE(property->propertyTypeCategory(), QDeclarativeProperty::Normal);
    QCOMPARE(property->propertyType(), (int)QVariant::Int);

    QDeclarativeProperty p1(*property);
    QCOMPARE(p1.name(), QString("defaultProperty"));
    QCOMPARE(p1.read(), QVariant(10));
    QCOMPARE(p1.type(), QDeclarativeProperty::Property);
    QCOMPARE(p1.propertyTypeCategory(), QDeclarativeProperty::Normal);
    QCOMPARE(p1.propertyType(), (int)QVariant::Int);

    QDeclarativeProperty p2(&object, QLatin1String("url"));
    QCOMPARE(p2.name(), QString("url"));
    p2 = *property;
    QCOMPARE(p2.name(), QString("defaultProperty"));
    QCOMPARE(p2.read(), QVariant(10));
    QCOMPARE(p2.type(), QDeclarativeProperty::Property);
    QCOMPARE(p2.propertyTypeCategory(), QDeclarativeProperty::Normal);
    QCOMPARE(p2.propertyType(), (int)QVariant::Int);

    delete property; property = 0;

    QCOMPARE(p1.name(), QString("defaultProperty"));
    QCOMPARE(p1.read(), QVariant(10));
    QCOMPARE(p1.type(), QDeclarativeProperty::Property);
    QCOMPARE(p1.propertyTypeCategory(), QDeclarativeProperty::Normal);
    QCOMPARE(p1.propertyType(), (int)QVariant::Int);

    QCOMPARE(p2.name(), QString("defaultProperty"));
    QCOMPARE(p2.read(), QVariant(10));
    QCOMPARE(p2.type(), QDeclarativeProperty::Property);
    QCOMPARE(p2.propertyTypeCategory(), QDeclarativeProperty::Normal);
    QCOMPARE(p2.propertyType(), (int)QVariant::Int);
}

void tst_qdeclarativeproperty::initTestCase()
{
    QDeclarativeDataTest::initTestCase();
    qmlRegisterType<MyQmlObject>("Test",1,0,"MyQmlObject");
    qmlRegisterType<PropertyObject>("Test",1,0,"PropertyObject");
    qmlRegisterType<MyContainer>("Test",1,0,"MyContainer");
}


QTEST_MAIN(tst_qdeclarativeproperty)

#include "tst_qdeclarativeproperty.moc"
