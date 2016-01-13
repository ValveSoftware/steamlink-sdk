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
#include <QtDeclarative/qdeclarativeengine.h>
#include <QtDeclarative/qdeclarativecomponent.h>
#include <QtDeclarative/private/qdeclarativedom_p.h>

#include <QtCore/QDebug>
#include <QtCore/QFile>

class tst_qdeclarativedom : public QObject
{
    Q_OBJECT
public:
    tst_qdeclarativedom() {}

private slots:
    void loadSimple();
    void loadProperties();
    void loadGroupedProperties();
    void loadChildObject();
    void loadComposite();
    void loadImports();
    void loadErrors();
    void loadSyntaxErrors();
    void loadRemoteErrors();
    void loadDynamicProperty();
    void loadComponent();

    void testValueSource();
    void testValueInterceptor();

    void object_dynamicProperty();
    void object_property();
    void object_url();

    void copy();
    void position();
private:
    QDeclarativeEngine engine;
};


void tst_qdeclarativedom::loadSimple()
{
    QByteArray qml = "import QtQuick 1.0\n"
                      "Item {}";

    QDeclarativeDomDocument document;
    QVERIFY(document.load(&engine, qml));
    QVERIFY(document.errors().isEmpty());

    QDeclarativeDomObject rootObject = document.rootObject();
    QVERIFY(rootObject.isValid());
    QVERIFY(!rootObject.isComponent());
    QVERIFY(!rootObject.isCustomType());
    QVERIFY(rootObject.objectType() == "QtQuick/Item");
    QVERIFY(rootObject.objectTypeMajorVersion() == 1);
    QVERIFY(rootObject.objectTypeMinorVersion() == 0);
}

// Test regular properties
void tst_qdeclarativedom::loadProperties()
{
    QByteArray qml = "import QtQuick 1.0\n"
                     "Item { id : item; x : 300; visible : true }";

    QDeclarativeDomDocument document;
    QVERIFY(document.load(&engine, qml));

    QDeclarativeDomObject rootObject = document.rootObject();
    QVERIFY(rootObject.isValid());
    QVERIFY(rootObject.objectId() == "item");
    QCOMPARE(rootObject.properties().size(), 3);

    QDeclarativeDomProperty xProperty = rootObject.property("x");
    QVERIFY(xProperty.propertyName() == "x");
    QCOMPARE(xProperty.propertyNameParts().count(), 1);
    QVERIFY(xProperty.propertyNameParts().at(0) == "x");
    QCOMPARE(xProperty.position(), 37);
    QCOMPARE(xProperty.length(), 1);
    QVERIFY(xProperty.value().isLiteral());
    QVERIFY(xProperty.value().toLiteral().literal() == "300");

    QDeclarativeDomProperty visibleProperty = rootObject.property("visible");
    QVERIFY(visibleProperty.propertyName() == "visible");
    QCOMPARE(visibleProperty.propertyNameParts().count(), 1);
    QVERIFY(visibleProperty.propertyNameParts().at(0) == "visible");
    QCOMPARE(visibleProperty.position(), 46);
    QCOMPARE(visibleProperty.length(), 7);
    QVERIFY(visibleProperty.value().isLiteral());
    QVERIFY(visibleProperty.value().toLiteral().literal() == "true");
}

// Test grouped properties
void tst_qdeclarativedom::loadGroupedProperties()
{
    {
        QByteArray qml = "import QtQuick 1.0\n"
                         "Item { anchors.left: parent.left; anchors.right: parent.right }";

        QDeclarativeDomDocument document;
        QVERIFY(document.load(&engine, qml));

        QDeclarativeDomObject rootItem = document.rootObject();
        QVERIFY(rootItem.isValid());
        QVERIFY(rootItem.properties().size() == 2);

        // Order is not deterministic
        QDeclarativeDomProperty p0 = rootItem.properties().at(0);
        QDeclarativeDomProperty p1 = rootItem.properties().at(1);
        QDeclarativeDomProperty leftProperty;
        QDeclarativeDomProperty rightProperty;
        if (p0.propertyName() == "anchors.left") {
            leftProperty = p0;
            rightProperty = p1;
        } else {
            leftProperty = p1;
            rightProperty = p0;
        }

        QVERIFY(leftProperty.propertyName() == "anchors.left");
        QCOMPARE(leftProperty.propertyNameParts().count(), 2);
        QVERIFY(leftProperty.propertyNameParts().at(0) == "anchors");
        QVERIFY(leftProperty.propertyNameParts().at(1) == "left");
        QCOMPARE(leftProperty.position(), 26);
        QCOMPARE(leftProperty.length(), 12);
        QVERIFY(leftProperty.value().isBinding());
        QVERIFY(leftProperty.value().toBinding().binding() == "parent.left");

        QVERIFY(rightProperty.propertyName() == "anchors.right");
        QCOMPARE(rightProperty.propertyNameParts().count(), 2);
        QVERIFY(rightProperty.propertyNameParts().at(0) == "anchors");
        QVERIFY(rightProperty.propertyNameParts().at(1) == "right");
        QCOMPARE(rightProperty.position(), 53);
        QCOMPARE(rightProperty.length(), 13);
        QVERIFY(rightProperty.value().isBinding());
        QVERIFY(rightProperty.value().toBinding().binding() == "parent.right");
    }

    {
        QByteArray qml = "import QtQuick 1.0\n"
                         "Item { \n"
                         "    anchors {\n"
                         "        left: parent.left\n"
                         "        right: parent.right\n"
                         "    }\n"
                         "}";

        QDeclarativeDomDocument document;
        QVERIFY(document.load(&engine, qml));

        QDeclarativeDomObject rootItem = document.rootObject();
        QVERIFY(rootItem.isValid());
        QVERIFY(rootItem.properties().size() == 2);

        // Order is not deterministic
        QDeclarativeDomProperty p0 = rootItem.properties().at(0);
        QDeclarativeDomProperty p1 = rootItem.properties().at(1);
        QDeclarativeDomProperty leftProperty;
        QDeclarativeDomProperty rightProperty;
        if (p0.propertyName() == "anchors.left") {
            leftProperty = p0;
            rightProperty = p1;
        } else {
            leftProperty = p1;
            rightProperty = p0;
        }

        QVERIFY(leftProperty.propertyName() == "anchors.left");
        QCOMPARE(leftProperty.propertyNameParts().count(), 2);
        QVERIFY(leftProperty.propertyNameParts().at(0) == "anchors");
        QVERIFY(leftProperty.propertyNameParts().at(1) == "left");
        QCOMPARE(leftProperty.position(), 49);
        QCOMPARE(leftProperty.length(), 4);
        QVERIFY(leftProperty.value().isBinding());
        QVERIFY(leftProperty.value().toBinding().binding() == "parent.left");

        QVERIFY(rightProperty.propertyName() == "anchors.right");
        QCOMPARE(rightProperty.propertyNameParts().count(), 2);
        QVERIFY(rightProperty.propertyNameParts().at(0) == "anchors");
        QVERIFY(rightProperty.propertyNameParts().at(1) == "right");
        QCOMPARE(rightProperty.position(), 75);
        QCOMPARE(rightProperty.length(), 5);
        QVERIFY(rightProperty.value().isBinding());
        QVERIFY(rightProperty.value().toBinding().binding() == "parent.right");
    }

}

void tst_qdeclarativedom::loadChildObject()
{
    QByteArray qml = "import QtQuick 1.0\n"
                     "Item { Item {} }";

    QDeclarativeDomDocument document;
    QVERIFY(document.load(&engine, qml));

    QDeclarativeDomObject rootItem = document.rootObject();
    QVERIFY(rootItem.isValid());
    QVERIFY(rootItem.properties().size() == 1);

    QDeclarativeDomProperty listProperty = rootItem.properties().at(0);
    QVERIFY(listProperty.isDefaultProperty());
    QVERIFY(listProperty.value().isList());

    QDeclarativeDomList list = listProperty.value().toList();
    QVERIFY(list.values().size() == 1);

    QDeclarativeDomObject childItem = list.values().first().toObject();
    QVERIFY(childItem.isValid());
    QVERIFY(childItem.objectType() == "QtQuick/Item");
}

void tst_qdeclarativedom::loadComposite()
{
    QFile file(SRCDIR  "/data/top.qml");
    QVERIFY(file.open(QIODevice::ReadOnly | QIODevice::Text));

    QDeclarativeDomDocument document;
    QVERIFY(document.load(&engine, file.readAll(), QUrl::fromLocalFile(file.fileName())));
    QVERIFY(document.errors().isEmpty());

    QDeclarativeDomObject rootItem = document.rootObject();
    QVERIFY(rootItem.isValid());
    QCOMPARE(rootItem.objectType(), QByteArray("MyComponent"));
    QCOMPARE(rootItem.properties().size(), 2);

    QDeclarativeDomProperty widthProperty = rootItem.property("width");
    QVERIFY(widthProperty.value().isLiteral());

    QDeclarativeDomProperty heightProperty = rootItem.property("height");
    QVERIFY(heightProperty.value().isLiteral());
}

void tst_qdeclarativedom::testValueSource()
{
    QByteArray qml = "import QtQuick 1.0\n"
                     "Rectangle { SpringAnimation on height { spring: 1.4; damping: .15; to: Math.min(Math.max(-130, value*2.2 - 130), 133); }}";

    QDeclarativeEngine freshEngine;
    QDeclarativeDomDocument document;
    QVERIFY(document.load(&freshEngine, qml));

    QDeclarativeDomObject rootItem = document.rootObject();
    QVERIFY(rootItem.isValid());
    QDeclarativeDomProperty heightProperty = rootItem.properties().at(0);
    QVERIFY(heightProperty.propertyName() == "height");
    QVERIFY(heightProperty.value().isValueSource());

    const QDeclarativeDomValueValueSource valueSource = heightProperty.value().toValueSource();
    QDeclarativeDomObject valueSourceObject = valueSource.object();
    QVERIFY(valueSourceObject.isValid());

    QVERIFY(valueSourceObject.objectType() == "QtQuick/SpringAnimation");

    const QDeclarativeDomValue springValue = valueSourceObject.property("spring").value();
    QVERIFY(!springValue.isInvalid());
    QVERIFY(springValue.isLiteral());
    QVERIFY(springValue.toLiteral().literal() == "1.4");

    const QDeclarativeDomValue sourceValue = valueSourceObject.property("to").value();
    QVERIFY(!sourceValue.isInvalid());
    QVERIFY(sourceValue.isBinding());
    QVERIFY(sourceValue.toBinding().binding() == "Math.min(Math.max(-130, value*2.2 - 130), 133)");
}

void tst_qdeclarativedom::testValueInterceptor()
{
    QByteArray qml = "import QtQuick 1.0\n"
                     "Rectangle { Behavior on height { NumberAnimation { duration: 100 } } }";

    QDeclarativeEngine freshEngine;
    QDeclarativeDomDocument document;
    QVERIFY(document.load(&freshEngine, qml));

    QDeclarativeDomObject rootItem = document.rootObject();
    QVERIFY(rootItem.isValid());
    QDeclarativeDomProperty heightProperty = rootItem.properties().at(0);
    QVERIFY(heightProperty.propertyName() == "height");
    QVERIFY(heightProperty.value().isValueInterceptor());

    const QDeclarativeDomValueValueInterceptor valueInterceptor = heightProperty.value().toValueInterceptor();
    QDeclarativeDomObject valueInterceptorObject = valueInterceptor.object();
    QVERIFY(valueInterceptorObject.isValid());

    QVERIFY(valueInterceptorObject.objectType() == "QtQuick/Behavior");

    const QDeclarativeDomValue animationValue = valueInterceptorObject.property("animation").value();
    QVERIFY(!animationValue.isInvalid());
    QVERIFY(animationValue.isObject());
}

// Test QDeclarativeDomDocument::imports()
void tst_qdeclarativedom::loadImports()
{
    QByteArray qml = "import QtQuick 1.0\n"
                     "import importlib.sublib 1.1\n"
                     "import importlib.sublib 1.0 as NewFoo\n"
                     "import 'import'\n"
                     "import 'import' as X\n"
                     "Item {}";

    QDeclarativeEngine engine;
    engine.addImportPath(SRCDIR "/data");
    QDeclarativeDomDocument document;
    QVERIFY(document.load(&engine, qml, QUrl::fromLocalFile(SRCDIR "/data/dummy.qml")));

    QCOMPARE(document.imports().size(), 5);

    QDeclarativeDomImport import = document.imports().at(0);
    QCOMPARE(import.type(), QDeclarativeDomImport::Library);
    QCOMPARE(import.uri(), QLatin1String("QtQuick"));
    QCOMPARE(import.qualifier(), QString());
    QCOMPARE(import.version(), QLatin1String("1.0"));

    import = document.imports().at(1);
    QCOMPARE(import.type(), QDeclarativeDomImport::Library);
    QCOMPARE(import.uri(), QLatin1String("importlib.sublib"));
    QCOMPARE(import.qualifier(), QString());
    QCOMPARE(import.version(), QLatin1String("1.1"));

    import = document.imports().at(2);
    QCOMPARE(import.type(), QDeclarativeDomImport::Library);
    QCOMPARE(import.uri(), QLatin1String("importlib.sublib"));
    QCOMPARE(import.qualifier(), QLatin1String("NewFoo"));
    QCOMPARE(import.version(), QLatin1String("1.0"));

    import = document.imports().at(3);
    QCOMPARE(import.type(), QDeclarativeDomImport::File);
    QCOMPARE(import.uri(), QLatin1String("import"));
    QCOMPARE(import.qualifier(), QLatin1String(""));
    QCOMPARE(import.version(), QLatin1String(""));

    import = document.imports().at(4);
    QCOMPARE(import.type(), QDeclarativeDomImport::File);
    QCOMPARE(import.uri(), QLatin1String("import"));
    QCOMPARE(import.qualifier(), QLatin1String("X"));
    QCOMPARE(import.version(), QLatin1String(""));
}

// Test loading a file with errors
void tst_qdeclarativedom::loadErrors()
{
    QByteArray qml = "import QtQuick 1.0\n"
                     "Item {\n"
                     "  foo: 12\n"
                     "}";

    QDeclarativeDomDocument document;
    QVERIFY(false == document.load(&engine, qml));

    QCOMPARE(document.errors().count(), 1);
    QDeclarativeError error = document.errors().first();

    QCOMPARE(error.url(), QUrl());
    QCOMPARE(error.line(), 3);
    QCOMPARE(error.column(), 3);
    QCOMPARE(error.description(), QString("Cannot assign to non-existent property \"foo\""));
}

// Test loading a file with syntax errors
void tst_qdeclarativedom::loadSyntaxErrors()
{
    QByteArray qml = "import QtQuick 1.0\n"
                     "asdf";

    QDeclarativeDomDocument document;
    QVERIFY(false == document.load(&engine, qml));

    QCOMPARE(document.errors().count(), 1);
    QDeclarativeError error = document.errors().first();

    QCOMPARE(error.url(), QUrl());
    QCOMPARE(error.line(), 2);
    QCOMPARE(error.column(), 1);
    QCOMPARE(error.description(), QString("Syntax error"));
}

// Test attempting to load a file with remote references
void tst_qdeclarativedom::loadRemoteErrors()
{
    QByteArray qml = "import QtQuick 1.0\n"
                     "import \"http://localhost/exampleQmlScript.js\" as Script\n"
                     "Item {\n"
                     "}";
    QDeclarativeDomDocument document;
    QVERIFY(false == document.load(&engine, qml));

    QCOMPARE(document.errors().count(), 1);
    QDeclarativeError error = document.errors().first();

    QCOMPARE(error.url(), QUrl());
    QCOMPARE(error.line(), -1);
    QCOMPARE(error.column(), -1);
    QCOMPARE(error.description(), QString("QDeclarativeDomDocument supports local types only"));
}

// Test dynamic property declarations
void tst_qdeclarativedom::loadDynamicProperty()
{
    {
        QByteArray qml = "import QtQuick 1.0\n"
                         "Item {\n"
                         "    property int a\n"
                         "    property bool b\n"
                         "    property double c\n"
                         "    property real d\n"
                         "    property string e\n"
                         "    property url f\n"
                         "    property color g\n"
                         "    property date h\n"
                         "    property variant i\n"
                         "    property QtObject j\n"
                         "}";

        QDeclarativeDomDocument document;
        QVERIFY(document.load(&engine, qml));

        QDeclarativeDomObject rootObject = document.rootObject();
        QVERIFY(rootObject.isValid());

        QCOMPARE(rootObject.dynamicProperties().count(), 10);

#define DP_TEST(index, name, type, test_position, test_length, propTypeName) \
    { \
        QDeclarativeDomDynamicProperty d = rootObject.dynamicProperties().at(index); \
        QVERIFY(d.isValid()); \
        QVERIFY(d.propertyName() == # name ); \
        QVERIFY(d.propertyType() == type); \
        QVERIFY(d.propertyTypeName() == propTypeName); \
        QVERIFY(d.isDefaultProperty() == false); \
        QVERIFY(d.defaultValue().isValid() == false); \
        QCOMPARE(d.position(), test_position); \
        QCOMPARE(d.length(), test_length); \
    } \

        DP_TEST(0, a, QVariant::Int, 30, 14, "int");
        DP_TEST(1, b, QVariant::Bool, 49, 15, "bool");
        DP_TEST(2, c, QMetaType::QReal, 69, 17, "double");
        DP_TEST(3, d, QMetaType::QReal, 91, 15, "real");
        DP_TEST(4, e, QVariant::String, 111, 17, "string");
        DP_TEST(5, f, QVariant::Url, 133, 14, "url");
        DP_TEST(6, g, QVariant::Color, 152, 16, "color");
        DP_TEST(7, h, QVariant::DateTime, 173, 15, "date");
        DP_TEST(8, i, qMetaTypeId<QVariant>(), 193, 18, "variant");
        DP_TEST(9, j, -1, 216, 19, "QtObject");
    }

    {
        QByteArray qml = "import QtQuick 1.0\n"
                         "Item {\n"
                         "    id: item\n"
                         "    property int a: 12\n"
                         "    property int b: a + 6\n"
                         "    default property QtObject c\n"
                         "    property alias d: item.a\n"
                         "}\n";

        QDeclarativeDomDocument document;
        QVERIFY(document.load(&engine, qml));

        QDeclarativeDomObject rootObject = document.rootObject();
        QVERIFY(rootObject.isValid());

        QCOMPARE(rootObject.dynamicProperties().count(), 4);

        {
            QDeclarativeDomDynamicProperty d = rootObject.dynamicProperties().at(0);
            QVERIFY(d.isDefaultProperty() == false);
            QVERIFY(d.isAlias() == false);
            QVERIFY(d.defaultValue().isValid());
            QVERIFY(d.defaultValue().propertyName() == "a");
            QVERIFY(d.defaultValue().value().isLiteral());
        }

        {
            QDeclarativeDomDynamicProperty d = rootObject.dynamicProperties().at(1);
            QVERIFY(d.isDefaultProperty() == false);
            QVERIFY(d.isAlias() == false);
            QVERIFY(d.defaultValue().isValid());
            QVERIFY(d.defaultValue().propertyName() == "b");
            QVERIFY(d.defaultValue().value().isBinding());
        }

        {
            QDeclarativeDomDynamicProperty d = rootObject.dynamicProperties().at(2);
            QVERIFY(d.isDefaultProperty() == true);
            QVERIFY(d.isAlias() == false);
            QVERIFY(d.defaultValue().isValid() == false);
        }

        {
            QDeclarativeDomDynamicProperty d = rootObject.dynamicProperties().at(3);
            QVERIFY(d.isDefaultProperty() == false);
            QVERIFY(d.isAlias() == true);
        }
    }
}

// Test inline components
void tst_qdeclarativedom::loadComponent()
{
    // Explicit component
    {
        QByteArray qml = "import QtQuick 1.0\n"
                         "Item {\n"
                         "    Component {\n"
                         "        id: myComponent\n"
                         "        Item {}\n"
                         "    }\n"
                         "}";

        QDeclarativeDomDocument document;
        QVERIFY(document.load(&engine, qml));

        QDeclarativeDomObject rootItem = document.rootObject();
        QVERIFY(rootItem.isValid());
        QVERIFY(rootItem.properties().size() == 1);

        QDeclarativeDomProperty listProperty = rootItem.properties().at(0);
        QVERIFY(listProperty.isDefaultProperty());
        QVERIFY(listProperty.value().isList());

        QDeclarativeDomList list = listProperty.value().toList();
        QVERIFY(list.values().size() == 1);

        QDeclarativeDomObject componentObject = list.values().first().toObject();
        QVERIFY(componentObject.isValid());
        QVERIFY(componentObject.objectClassName() == "Component");
        QVERIFY(componentObject.isComponent());

        QDeclarativeDomComponent component = componentObject.toComponent();
        QVERIFY(component.isValid());
        QVERIFY(component.objectType() == "QtQuick/Component");
        QVERIFY(component.objectTypeMajorVersion() == 1);
        QVERIFY(component.objectTypeMinorVersion() == 0);
        QVERIFY(component.objectClassName() == "Component");
        QVERIFY(component.objectId() == "myComponent");
        QVERIFY(component.properties().isEmpty());
        QVERIFY(component.dynamicProperties().isEmpty());
        QVERIFY(component.isCustomType() == false);
        QVERIFY(component.customTypeData() == "");
        QVERIFY(component.isComponent());
        QCOMPARE(component.position(), 30);
        QCOMPARE(component.length(), 57);

        QVERIFY(component.componentRoot().isValid());
        QVERIFY(component.componentRoot().objectClassName() == "Item");
    }

    // Implicit component
    {
        QByteArray qml = "import QtQuick 1.0\n"
                         "ListView {\n"
                         "    delegate: Item {}\n"
                         "}";

        QDeclarativeDomDocument document;
        QVERIFY(document.load(&engine, qml));

        QDeclarativeDomObject rootItem = document.rootObject();
        QVERIFY(rootItem.isValid());
        QVERIFY(rootItem.properties().size() == 1);

        QDeclarativeDomProperty delegate = rootItem.property("delegate");

        QDeclarativeDomObject componentObject = delegate.value().toObject();
        QVERIFY(componentObject.isValid());
        QVERIFY(componentObject.objectClassName() == "Component");
        QVERIFY(componentObject.isComponent());

        QDeclarativeDomComponent component = componentObject.toComponent();
        QVERIFY(component.isValid());
        QVERIFY(component.objectType() == "QtQuick/Component");
        QVERIFY(component.objectClassName() == "Component");
        QVERIFY(component.objectId() == "");
        QVERIFY(component.properties().isEmpty());
        QVERIFY(component.dynamicProperties().isEmpty());
        QVERIFY(component.isCustomType() == false);
        QVERIFY(component.customTypeData() == "");
        QVERIFY(component.isComponent());
        QCOMPARE(component.position(), 44);
        QCOMPARE(component.length(), 7);

        QVERIFY(component.componentRoot().isValid());
        QVERIFY(component.componentRoot().objectClassName() == "Item");
    }
}

// Test QDeclarativeDomObject::dynamicProperty() method
void tst_qdeclarativedom::object_dynamicProperty()
{
    // Invalid object
    {
        QDeclarativeDomObject object;
        QVERIFY(object.dynamicProperty("").isValid() == false);
        QVERIFY(object.dynamicProperty("foo").isValid() == false);
    }


    // Valid object, no dynamic properties
    {
        QByteArray qml = "import QtQuick 1.0\n"
                         "Item {}";

        QDeclarativeDomDocument document;
        QVERIFY(document.load(&engine, qml));

        QDeclarativeDomObject rootObject = document.rootObject();
        QVERIFY(rootObject.isValid());

        QVERIFY(rootObject.dynamicProperty("").isValid() == false);
        QVERIFY(rootObject.dynamicProperty("foo").isValid() == false);
    }

    // Valid object, dynamic properties
    {
        QByteArray qml = "import QtQuick 1.0\n"
                         "Item {\n"
                         "    property int a\n"
                         "}";

        QDeclarativeDomDocument document;
        QVERIFY(document.load(&engine, qml));

        QDeclarativeDomObject rootObject = document.rootObject();
        QVERIFY(rootObject.isValid());

        QVERIFY(rootObject.dynamicProperty("").isValid() == false);
        QVERIFY(rootObject.dynamicProperty("foo").isValid() == false);

        QDeclarativeDomDynamicProperty p = rootObject.dynamicProperty("a");
        QVERIFY(p.isValid());
        QVERIFY(p.propertyName() == "a");
        QVERIFY(p.propertyType() == QVariant::Int);
        QVERIFY(p.propertyTypeName() == "int");
        QVERIFY(p.isDefaultProperty() == false);
        QCOMPARE(p.position(), 30);
        QCOMPARE(p.length(), 14);
    }

}

// Test QDeclarativeObject::property() method
void tst_qdeclarativedom::object_property()
{
    // Invalid object
    {
        QDeclarativeDomObject object;
        QVERIFY(object.property("").isValid() == false);
        QVERIFY(object.property("foo").isValid() == false);
    }

    // Valid object - no default
    {
        QByteArray qml = "import QtQuick 1.0\n"
                         "Item {\n"
                         "    x: 10\n"
                         "    y: 12\n"
                         "}\n";

        QDeclarativeDomDocument document;
        QVERIFY(document.load(&engine, qml));

        QDeclarativeDomObject rootObject = document.rootObject();
        QVERIFY(rootObject.isValid());

        QVERIFY(rootObject.property("").isValid() == false);
        QVERIFY(rootObject.property("foo").isValid() == false);

        QDeclarativeDomProperty x = rootObject.property("x");
        QVERIFY(x.isValid());
        QVERIFY(x.propertyName() == "x");
        QVERIFY(x.propertyNameParts().count() == 1);
        QVERIFY(x.propertyNameParts().at(0) == "x");
        QVERIFY(x.isDefaultProperty() == false);
        QVERIFY(x.value().isLiteral());
        QVERIFY(x.value().toLiteral().literal() == "10");
        QCOMPARE(x.position(), 30);
        QCOMPARE(x.length(), 1);

        QDeclarativeDomProperty y = rootObject.property("y");
        QVERIFY(y.isValid());
        QVERIFY(y.propertyName() == "y");
        QVERIFY(y.propertyNameParts().count() == 1);
        QVERIFY(y.propertyNameParts().at(0) == "y");
        QVERIFY(y.isDefaultProperty() == false);
        QVERIFY(y.value().isLiteral());
        QVERIFY(y.value().toLiteral().literal() == "12");
        QCOMPARE(y.position(), 40);
        QCOMPARE(y.length(), 1);
    }

    // Valid object - with default
    {
        QByteArray qml = "import QtQuick 1.0\n"
                         "Item {\n"
                         "    x: 10\n"
                         "    y: 12\n"
                         "    Item {}\n"
                         "}\n";

        QDeclarativeDomDocument document;
        QVERIFY(document.load(&engine, qml));

        QDeclarativeDomObject rootObject = document.rootObject();
        QVERIFY(rootObject.isValid());

        QVERIFY(rootObject.property("").isValid() == false);
        QVERIFY(rootObject.property("foo").isValid() == false);

        QDeclarativeDomProperty x = rootObject.property("x");
        QVERIFY(x.isValid());
        QVERIFY(x.propertyName() == "x");
        QVERIFY(x.propertyNameParts().count() == 1);
        QVERIFY(x.propertyNameParts().at(0) == "x");
        QVERIFY(x.isDefaultProperty() == false);
        QVERIFY(x.value().isLiteral());
        QVERIFY(x.value().toLiteral().literal() == "10");
        QCOMPARE(x.position(), 30);
        QCOMPARE(x.length(), 1);

        QDeclarativeDomProperty y = rootObject.property("y");
        QVERIFY(y.isValid());
        QVERIFY(y.propertyName() == "y");
        QVERIFY(y.propertyNameParts().count() == 1);
        QVERIFY(y.propertyNameParts().at(0) == "y");
        QVERIFY(y.isDefaultProperty() == false);
        QVERIFY(y.value().isLiteral());
        QVERIFY(y.value().toLiteral().literal() == "12");
        QCOMPARE(y.position(), 40);
        QCOMPARE(y.length(), 1);

        QDeclarativeDomProperty data = rootObject.property("data");
        QVERIFY(data.isValid());
        QVERIFY(data.propertyName() == "data");
        QVERIFY(data.propertyNameParts().count() == 1);
        QVERIFY(data.propertyNameParts().at(0) == "data");
        QVERIFY(data.isDefaultProperty() == true);
        QVERIFY(data.value().isList());
        QCOMPARE(data.position(), 50);
        QCOMPARE(data.length(), 0);
    }
}

// Tests the QDeclarativeDomObject::url() method
void tst_qdeclarativedom::object_url()
{
    // Invalid object
    {
        QDeclarativeDomObject object;
        QCOMPARE(object.url(), QUrl());
    }

    // Valid builtin object
    {
        QByteArray qml = "import QtQuick 1.0\n"
                         "Item {}";

        QDeclarativeDomDocument document;
        QVERIFY(document.load(&engine, qml));

        QDeclarativeDomObject rootObject = document.rootObject();
        QVERIFY(rootObject.isValid());
        QCOMPARE(rootObject.url(), QUrl());
    }

    // Valid composite object
    {
        QByteArray qml = "import QtQuick 1.0\n"
                         "MyItem {}";

        QUrl myUrl = QUrl::fromLocalFile(SRCDIR "/data/main.qml");
        QUrl subUrl = QUrl::fromLocalFile(SRCDIR "/data/MyItem.qml");

        QDeclarativeDomDocument document;
        QVERIFY(document.load(&engine, qml, myUrl));

        QDeclarativeDomObject rootObject = document.rootObject();
        QVERIFY(rootObject.isValid());
        QCOMPARE(rootObject.url(), subUrl);
    }
}

// Test copy constructors and operators
void tst_qdeclarativedom::copy()
{
    QByteArray qml = "import QtQuick 1.0\n"
                     "MyItem {\n"
                     "    id: myItem\n"
                     "    property int a: 10\n"
                     "    x: 10\n"
                     "    y: x + 10\n"
                     "    NumberAnimation on z {}\n"
                     "    Behavior on opacity {}\n"
                     "    Component {\n"
                     "        Item{}\n"
                     "    }\n"
                     "    children: [ Item{}, Item{} ]\n"
                     "}\n";

    QUrl myUrl = QUrl::fromLocalFile(SRCDIR "/data/main.qml");

    QDeclarativeDomDocument document;
    QVERIFY(document.load(&engine, qml, myUrl));

    // QDeclarativeDomDocument
    {
        QDeclarativeDomDocument document2(document);
        QDeclarativeDomDocument document3;
        document3 = document;

        QCOMPARE(document.imports().count(), document2.imports().count());
        QCOMPARE(document.errors().count(), document2.errors().count());
        QCOMPARE(document.rootObject().objectClassName(), document2.rootObject().objectClassName());

        QCOMPARE(document.imports().count(), document3.imports().count());
        QCOMPARE(document.errors().count(), document3.errors().count());
        QCOMPARE(document.rootObject().objectClassName(), document3.rootObject().objectClassName());
    }

    // QDeclarativeDomImport
    {
        QCOMPARE(document.imports().count(), 1);
        QDeclarativeDomImport import = document.imports().at(0);

        QDeclarativeDomImport import2(import);
        QDeclarativeDomImport import3;
        import3 = import2;

        QCOMPARE(import.type(), import2.type());
        QCOMPARE(import.uri(), import2.uri());
        QCOMPARE(import.version(), import2.version());
        QCOMPARE(import.qualifier(), import2.qualifier());

        QCOMPARE(import.type(), import3.type());
        QCOMPARE(import.uri(), import3.uri());
        QCOMPARE(import.version(), import3.version());
        QCOMPARE(import.qualifier(), import3.qualifier());
    }

    // QDeclarativeDomObject
    {
        QDeclarativeDomObject object = document.rootObject();
        QVERIFY(object.isValid());

        QDeclarativeDomObject object2(object);
        QDeclarativeDomObject object3;
        object3 = object;

        QCOMPARE(object.isValid(), object2.isValid());
        QCOMPARE(object.objectType(), object2.objectType());
        QCOMPARE(object.objectClassName(), object2.objectClassName());
        QCOMPARE(object.objectTypeMajorVersion(), object2.objectTypeMajorVersion());
        QCOMPARE(object.objectTypeMinorVersion(), object2.objectTypeMinorVersion());
        QCOMPARE(object.objectId(), object2.objectId());
        QCOMPARE(object.properties().count(), object2.properties().count());
        QCOMPARE(object.dynamicProperties().count(), object2.dynamicProperties().count());
        QCOMPARE(object.isCustomType(), object2.isCustomType());
        QCOMPARE(object.customTypeData(), object2.customTypeData());
        QCOMPARE(object.isComponent(), object2.isComponent());
        QCOMPARE(object.position(), object2.position());
        QCOMPARE(object.length(), object2.length());
        QCOMPARE(object.url(), object2.url());

        QCOMPARE(object.isValid(), object3.isValid());
        QCOMPARE(object.objectType(), object3.objectType());
        QCOMPARE(object.objectClassName(), object3.objectClassName());
        QCOMPARE(object.objectTypeMajorVersion(), object3.objectTypeMajorVersion());
        QCOMPARE(object.objectTypeMinorVersion(), object3.objectTypeMinorVersion());
        QCOMPARE(object.objectId(), object3.objectId());
        QCOMPARE(object.properties().count(), object3.properties().count());
        QCOMPARE(object.dynamicProperties().count(), object3.dynamicProperties().count());
        QCOMPARE(object.isCustomType(), object3.isCustomType());
        QCOMPARE(object.customTypeData(), object3.customTypeData());
        QCOMPARE(object.isComponent(), object3.isComponent());
        QCOMPARE(object.position(), object3.position());
        QCOMPARE(object.length(), object3.length());
        QCOMPARE(object.url(), object3.url());
    }

    // QDeclarativeDomDynamicProperty
    {
        QDeclarativeDomObject object = document.rootObject();
        QDeclarativeDomDynamicProperty property = object.dynamicProperty("a");

        QDeclarativeDomDynamicProperty property2(property);
        QDeclarativeDomDynamicProperty property3;
        property3 = property;

        QCOMPARE(property.isValid(), property2.isValid());
        QCOMPARE(property.propertyName(), property2.propertyName());
        QCOMPARE(property.propertyType(), property2.propertyType());
        QCOMPARE(property.propertyTypeName(), property2.propertyTypeName());
        QCOMPARE(property.isDefaultProperty(), property2.isDefaultProperty());
        QCOMPARE(property.defaultValue().propertyName(), property2.defaultValue().propertyName());
        QCOMPARE(property.position(), property2.position());
        QCOMPARE(property.length(), property2.length());

        QCOMPARE(property.isValid(), property3.isValid());
        QCOMPARE(property.propertyName(), property3.propertyName());
        QCOMPARE(property.propertyType(), property3.propertyType());
        QCOMPARE(property.propertyTypeName(), property3.propertyTypeName());
        QCOMPARE(property.isDefaultProperty(), property3.isDefaultProperty());
        QCOMPARE(property.defaultValue().propertyName(), property3.defaultValue().propertyName());
        QCOMPARE(property.position(), property3.position());
        QCOMPARE(property.length(), property3.length());
    }

    // QDeclarativeDomProperty
    {
        QDeclarativeDomObject object = document.rootObject();
        QDeclarativeDomProperty property = object.property("opacity");

        QDeclarativeDomProperty property2(property);
        QDeclarativeDomProperty property3;
        property3 = property;

        QCOMPARE(property.isValid(), property2.isValid());
        QCOMPARE(property.propertyName(), property2.propertyName());
        QCOMPARE(property.propertyNameParts(), property2.propertyNameParts());
        QCOMPARE(property.isDefaultProperty(), property2.isDefaultProperty());
        QCOMPARE(property.value().type(), property2.value().type());
        QCOMPARE(property.position(), property2.position());
        QCOMPARE(property.length(), property2.length());

        QCOMPARE(property.isValid(), property3.isValid());
        QCOMPARE(property.propertyName(), property3.propertyName());
        QCOMPARE(property.propertyNameParts(), property3.propertyNameParts());
        QCOMPARE(property.isDefaultProperty(), property3.isDefaultProperty());
        QCOMPARE(property.value().type(), property3.value().type());
        QCOMPARE(property.position(), property3.position());
        QCOMPARE(property.length(), property3.length());
    }

    // QDeclarativeDomValueLiteral
    {
        QDeclarativeDomObject object = document.rootObject();
        QDeclarativeDomProperty property = object.property("x");
        QDeclarativeDomValueLiteral literal = property.value().toLiteral();
        QCOMPARE(literal.literal(), QString("10"));

        QDeclarativeDomValueLiteral literal2(literal);
        QDeclarativeDomValueLiteral literal3;
        literal3 = literal2;

        QCOMPARE(literal2.literal(), QString("10"));
        QCOMPARE(literal3.literal(), QString("10"));
    }


    // QDeclarativeDomValueBinding
    {
        QDeclarativeDomObject object = document.rootObject();
        QDeclarativeDomProperty property = object.property("y");
        QDeclarativeDomValueBinding binding = property.value().toBinding();
        QCOMPARE(binding.binding(), QString("x + 10"));

        QDeclarativeDomValueBinding binding2(binding);
        QDeclarativeDomValueBinding binding3;
        binding3 = binding2;

        QCOMPARE(binding2.binding(), QString("x + 10"));
        QCOMPARE(binding3.binding(), QString("x + 10"));
    }

    // QDeclarativeDomValueValueSource
    {
        QDeclarativeDomObject object = document.rootObject();
        QDeclarativeDomProperty property = object.property("z");
        QDeclarativeDomValueValueSource source = property.value().toValueSource();
        QCOMPARE(source.object().objectClassName(), QByteArray("NumberAnimation"));

        QDeclarativeDomValueValueSource source2(source);
        QDeclarativeDomValueValueSource source3;
        source3 = source;

        QCOMPARE(source2.object().objectClassName(), QByteArray("NumberAnimation"));
        QCOMPARE(source3.object().objectClassName(), QByteArray("NumberAnimation"));
    }

    // QDeclarativeDomValueValueInterceptor
    {
        QDeclarativeDomObject object = document.rootObject();
        QDeclarativeDomProperty property = object.property("opacity");
        QDeclarativeDomValueValueInterceptor interceptor = property.value().toValueInterceptor();
        QCOMPARE(interceptor.object().objectClassName(), QByteArray("Behavior"));

        QDeclarativeDomValueValueInterceptor interceptor2(interceptor);
        QDeclarativeDomValueValueInterceptor interceptor3;
        interceptor3 = interceptor;

        QCOMPARE(interceptor2.object().objectClassName(), QByteArray("Behavior"));
        QCOMPARE(interceptor3.object().objectClassName(), QByteArray("Behavior"));
    }

    // QDeclarativeDomComponent
    {
        QDeclarativeDomObject object = document.rootObject();
        QDeclarativeDomProperty property = object.property("data");
        QCOMPARE(property.value().toList().values().count(), 1);
        QDeclarativeDomComponent component =
            property.value().toList().values().at(0).toObject().toComponent();
        QCOMPARE(component.componentRoot().objectClassName(), QByteArray("Item"));

        QDeclarativeDomComponent component2(component);
        QDeclarativeDomComponent component3;
        component3 = component;

        QCOMPARE(component.componentRoot().objectClassName(), component2.componentRoot().objectClassName());
        QCOMPARE(component.isValid(), component2.isValid());
        QCOMPARE(component.objectType(), component2.objectType());
        QCOMPARE(component.objectClassName(), component2.objectClassName());
        QCOMPARE(component.objectTypeMajorVersion(), component2.objectTypeMajorVersion());
        QCOMPARE(component.objectTypeMinorVersion(), component2.objectTypeMinorVersion());
        QCOMPARE(component.objectId(), component2.objectId());
        QCOMPARE(component.properties().count(), component2.properties().count());
        QCOMPARE(component.dynamicProperties().count(), component2.dynamicProperties().count());
        QCOMPARE(component.isCustomType(), component2.isCustomType());
        QCOMPARE(component.customTypeData(), component2.customTypeData());
        QCOMPARE(component.isComponent(), component2.isComponent());
        QCOMPARE(component.position(), component2.position());
        QCOMPARE(component.length(), component2.length());
        QCOMPARE(component.url(), component2.url());

        QCOMPARE(component.componentRoot().objectClassName(), component3.componentRoot().objectClassName());
        QCOMPARE(component.isValid(), component3.isValid());
        QCOMPARE(component.objectType(), component3.objectType());
        QCOMPARE(component.objectClassName(), component3.objectClassName());
        QCOMPARE(component.objectTypeMajorVersion(), component3.objectTypeMajorVersion());
        QCOMPARE(component.objectTypeMinorVersion(), component3.objectTypeMinorVersion());
        QCOMPARE(component.objectId(), component3.objectId());
        QCOMPARE(component.properties().count(), component3.properties().count());
        QCOMPARE(component.dynamicProperties().count(), component3.dynamicProperties().count());
        QCOMPARE(component.isCustomType(), component3.isCustomType());
        QCOMPARE(component.customTypeData(), component3.customTypeData());
        QCOMPARE(component.isComponent(), component3.isComponent());
        QCOMPARE(component.position(), component3.position());
        QCOMPARE(component.length(), component3.length());
        QCOMPARE(component.url(), component3.url());
    }

    // QDeclarativeDomValue
    {
        QDeclarativeDomObject object = document.rootObject();
        QDeclarativeDomProperty property = object.property("data");
        QDeclarativeDomValue value = property.value();

        QDeclarativeDomValue value2(value);
        QDeclarativeDomValue value3;
        value3 = value;

        QCOMPARE(value.type(), value2.type());
        QCOMPARE(value.isInvalid(), value2.isInvalid());
        QCOMPARE(value.isLiteral(), value2.isLiteral());
        QCOMPARE(value.isBinding(), value2.isBinding());
        QCOMPARE(value.isValueSource(), value2.isValueSource());
        QCOMPARE(value.isValueInterceptor(), value2.isValueInterceptor());
        QCOMPARE(value.isObject(), value2.isObject());
        QCOMPARE(value.isList(), value2.isList());
        QCOMPARE(value.position(), value2.position());
        QCOMPARE(value.length(), value2.length());

        QCOMPARE(value.type(), value3.type());
        QCOMPARE(value.isInvalid(), value3.isInvalid());
        QCOMPARE(value.isLiteral(), value3.isLiteral());
        QCOMPARE(value.isBinding(), value3.isBinding());
        QCOMPARE(value.isValueSource(), value3.isValueSource());
        QCOMPARE(value.isValueInterceptor(), value3.isValueInterceptor());
        QCOMPARE(value.isObject(), value3.isObject());
        QCOMPARE(value.isList(), value3.isList());
        QCOMPARE(value.position(), value3.position());
        QCOMPARE(value.length(), value3.length());
    }
    {
        QDeclarativeDomObject object = document.rootObject();
        QDeclarativeDomProperty property = object.property("x");
        QDeclarativeDomValue value = property.value();

        QDeclarativeDomValue value2(value);
        QDeclarativeDomValue value3;
        value3 = value;

        QCOMPARE(value.type(), value2.type());
        QCOMPARE(value.isInvalid(), value2.isInvalid());
        QCOMPARE(value.isLiteral(), value2.isLiteral());
        QCOMPARE(value.isBinding(), value2.isBinding());
        QCOMPARE(value.isValueSource(), value2.isValueSource());
        QCOMPARE(value.isValueInterceptor(), value2.isValueInterceptor());
        QCOMPARE(value.isObject(), value2.isObject());
        QCOMPARE(value.isList(), value2.isList());
        QCOMPARE(value.position(), value2.position());
        QCOMPARE(value.length(), value2.length());

        QCOMPARE(value.type(), value3.type());
        QCOMPARE(value.isInvalid(), value3.isInvalid());
        QCOMPARE(value.isLiteral(), value3.isLiteral());
        QCOMPARE(value.isBinding(), value3.isBinding());
        QCOMPARE(value.isValueSource(), value3.isValueSource());
        QCOMPARE(value.isValueInterceptor(), value3.isValueInterceptor());
        QCOMPARE(value.isObject(), value3.isObject());
        QCOMPARE(value.isList(), value3.isList());
        QCOMPARE(value.position(), value3.position());
        QCOMPARE(value.length(), value3.length());
    }
    {
        QDeclarativeDomValue value;

        QDeclarativeDomValue value2(value);
        QDeclarativeDomValue value3;
        value3 = value;

        QCOMPARE(value.type(), value2.type());
        QCOMPARE(value.isInvalid(), value2.isInvalid());
        QCOMPARE(value.isLiteral(), value2.isLiteral());
        QCOMPARE(value.isBinding(), value2.isBinding());
        QCOMPARE(value.isValueSource(), value2.isValueSource());
        QCOMPARE(value.isValueInterceptor(), value2.isValueInterceptor());
        QCOMPARE(value.isObject(), value2.isObject());
        QCOMPARE(value.isList(), value2.isList());
        QCOMPARE(value.position(), value2.position());
        QCOMPARE(value.length(), value2.length());

        QCOMPARE(value.type(), value3.type());
        QCOMPARE(value.isInvalid(), value3.isInvalid());
        QCOMPARE(value.isLiteral(), value3.isLiteral());
        QCOMPARE(value.isBinding(), value3.isBinding());
        QCOMPARE(value.isValueSource(), value3.isValueSource());
        QCOMPARE(value.isValueInterceptor(), value3.isValueInterceptor());
        QCOMPARE(value.isObject(), value3.isObject());
        QCOMPARE(value.isList(), value3.isList());
        QCOMPARE(value.position(), value3.position());
        QCOMPARE(value.length(), value3.length());
    }

    // QDeclarativeDomList
    {
        QDeclarativeDomObject object = document.rootObject();
        QDeclarativeDomProperty property = object.property("children");
        QDeclarativeDomList list = property.value().toList();
        QCOMPARE(list.values().count(), 2);

        QDeclarativeDomList list2(list);
        QDeclarativeDomList list3;
        list3 = list2;

        QCOMPARE(list.values().count(), list2.values().count());
        QCOMPARE(list.position(), list2.position());
        QCOMPARE(list.length(), list2.length());
        QCOMPARE(list.commaPositions(), list2.commaPositions());

        QCOMPARE(list.values().count(), list3.values().count());
        QCOMPARE(list.position(), list3.position());
        QCOMPARE(list.length(), list3.length());
        QCOMPARE(list.commaPositions(), list3.commaPositions());

    }
}

// Tests the position/length of various elements
void tst_qdeclarativedom::position()
{
    QByteArray qml = "import QtQuick 1.0\n"
                     "Item {\n"
                     "    id: myItem\n"
                     "    property int a: 10\n"
                     "    x: 10\n"
                     "    y: x + 10\n"
                     "    NumberAnimation on z {}\n"
                     "    Behavior on opacity {}\n"
                     "    Component {\n"
                     "        Item{}\n"
                     "    }\n"
                     "    children: [ Item{}, Item{} ]\n"
                     "}\n";


    QDeclarativeDomDocument document;
    QVERIFY(document.load(&engine, qml));

    QDeclarativeDomObject root = document.rootObject();

    // All QDeclarativeDomDynamicProperty
    QDeclarativeDomDynamicProperty dynProp = root.dynamicProperty("a");
    QCOMPARE(dynProp.position(), 45);
    QCOMPARE(dynProp.length(), 18);

    // All QDeclarativeDomProperty
    QDeclarativeDomProperty x = root.property("x");
    QCOMPARE(x.position(), 68);
    QCOMPARE(x.length(), 1);

    QDeclarativeDomProperty y = root.property("y");
    QCOMPARE(y.position(), 78);
    QCOMPARE(y.length(), 1);

    QDeclarativeDomProperty z = root.property("z");
    QCOMPARE(z.position(), 111);
    QCOMPARE(z.length(), 1);

    QDeclarativeDomProperty opacity = root.property("opacity");
    QCOMPARE(opacity.position(), 132);
    QCOMPARE(opacity.length(), 7);

    QDeclarativeDomProperty data = root.property("data");
    QCOMPARE(data.position(), 147);
    QCOMPARE(data.length(), 0);

    QDeclarativeDomProperty children = root.property("children");
    QCOMPARE(children.position(), 184);
    QCOMPARE(children.length(), 8);

    QDeclarativeDomList dataList = data.value().toList();
    QCOMPARE(dataList.values().count(), 1);
    QDeclarativeDomList childrenList = children.value().toList();
    QCOMPARE(childrenList.values().count(), 2);

    // All QDeclarativeDomObject
    QCOMPARE(root.position(), 19);
    QCOMPARE(root.length(), 195);

    QDeclarativeDomObject numberAnimation = z.value().toValueSource().object();
    QCOMPARE(numberAnimation.position(), 92);
    QCOMPARE(numberAnimation.length(), 23);

    QDeclarativeDomObject behavior = opacity.value().toValueInterceptor().object();
    QCOMPARE(behavior.position(), 120);
    QCOMPARE(behavior.length(), 22);

    QDeclarativeDomObject component = dataList.values().at(0).toObject();
    QCOMPARE(component.position(), 147);
    QCOMPARE(component.length(), 32);

    QDeclarativeDomObject componentRoot = component.toComponent().componentRoot();
    QCOMPARE(componentRoot.position(), 167);
    QCOMPARE(componentRoot.length(), 6);

    QDeclarativeDomObject child1 = childrenList.values().at(0).toObject();
    QCOMPARE(child1.position(), 196);
    QCOMPARE(child1.length(), 6);

    QDeclarativeDomObject child2 = childrenList.values().at(1).toObject();
    QCOMPARE(child2.position(), 204);
    QCOMPARE(child2.length(), 6);

    // All QDeclarativeDomValue
    QDeclarativeDomValue xValue = x.value();
    QCOMPARE(xValue.position(), 71);
    QCOMPARE(xValue.length(), 2);

    QDeclarativeDomValue yValue = y.value();
    QCOMPARE(yValue.position(), 81);
    QCOMPARE(yValue.length(), 6);

    QDeclarativeDomValue zValue = z.value();
    QCOMPARE(zValue.position(), 92);
    QCOMPARE(zValue.length(), 23);

    QDeclarativeDomValue opacityValue = opacity.value();
    QCOMPARE(opacityValue.position(), 120);
    QCOMPARE(opacityValue.length(), 22);

    QDeclarativeDomValue dataValue = data.value();
    QCOMPARE(dataValue.position(), 147);
    QCOMPARE(dataValue.length(), 32);

    QDeclarativeDomValue child1Value = childrenList.values().at(0);
    QCOMPARE(child1Value.position(), 196);
    QCOMPARE(child1Value.length(), 6);

    QDeclarativeDomValue child2Value = childrenList.values().at(1);
    QCOMPARE(child2Value.position(), 204);
    QCOMPARE(child2Value.length(), 6);

    // All QDeclarativeDomList
    QCOMPARE(childrenList.position(), 194);
    QCOMPARE(childrenList.length(), 18);
}

QTEST_MAIN(tst_qdeclarativedom)

#include "tst_qdeclarativedom.moc"
