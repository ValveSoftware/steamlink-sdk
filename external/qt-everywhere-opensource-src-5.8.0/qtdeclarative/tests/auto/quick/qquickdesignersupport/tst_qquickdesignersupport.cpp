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
#include <QtQml/qqmlengine.h>
#include <QtQml/qqmlcomponent.h>
#include <QtQuick/qquickview.h>
#include <private/qquickdesignersupportitems_p.h>
#include <private/qquickdesignersupportmetainfo_p.h>
#include <private/qquickdesignersupportproperties_p.h>
#include <private/qquickdesignersupportstates_p.h>
#include <private/qquickdesignersupportpropertychanges_p.h>
#include <private/qquickitem_p.h>
#include <private/qquickstate_p.h>
#include <private/qquickstate_p_p.h>
#include <private/qquickstategroup_p.h>
#include <private/qquickpropertychanges_p.h>
#include <private/qquickrectangle_p.h>
#include "../../shared/util.h"
#include "../shared/visualtestutil.h"

using namespace QQuickVisualTestUtil;

class tst_qquickdesignersupport : public QQmlDataTest
{
    Q_OBJECT
public:
    tst_qquickdesignersupport() {}

private slots:
    void customData();
    void customDataBindings();
    void objectProperties();
    void dynamicProperty();
    void createComponent();
    void basicStates();
    void statesPropertyChanges();
    void testNotifyPropertyChangeCallBack();
    void testFixResourcePathsForObjectCallBack();
};

void tst_qquickdesignersupport::customData()
{
    QScopedPointer<QQuickView> view(new QQuickView);
    view->engine()->setOutputWarningsToStandardError(false);
    view->setSource(testFileUrl("test.qml"));

    QVERIFY(view->errors().isEmpty());

    QQuickItem *rootItem = view->rootObject();

    QVERIFY(rootItem);

    QScopedPointer<QObject> newItemScopedPointer(QQuickDesignerSupportItems::createPrimitive(QLatin1String("QtQuick/Item"), 2, 6, view->rootContext()));
    QObject *newItem = newItemScopedPointer.data();

    QVERIFY(newItem);
    QVERIFY(qobject_cast<QQuickItem*>(newItem));

    QQuickDesignerSupportProperties::registerCustomData(newItem);
    QVERIFY(QQuickDesignerSupportProperties::getResetValue(newItem, "width").isValid());
    int defaultWidth = QQuickDesignerSupportProperties::getResetValue(newItem, "width").toInt();
    QCOMPARE(defaultWidth, 0);

    newItem->setProperty("width", 200);
    QCOMPARE(newItem->property("width").toInt(), 200);

    //Check if reseting property does work
    QQuickDesignerSupportProperties::doResetProperty(newItem, view->rootContext(), "width");
    QCOMPARE(newItem->property("width").toInt(), 0);

    //Setting a binding on width
    QQuickDesignerSupportProperties::setPropertyBinding(newItem,
                                                        view->engine()->contextForObject(newItem),
                                                        "width",
                                                        QLatin1String("Math.max(0, 200)"));
    QCOMPARE(newItem->property("width").toInt(), 200);
    QVERIFY(QQuickDesignerSupportProperties::hasBindingForProperty(newItem,
                                                                   view->engine()->contextForObject(newItem),
                                                                   "width",
                                                                   0));

    //Check if reseting property does work after setting binding
    QQuickDesignerSupportProperties::doResetProperty(newItem, view->rootContext(), "width");
    QCOMPARE(newItem->property("width").toInt(), 0);

    //No custom data available for the rootItem, because not registered by QQuickDesignerSupportProperties::registerCustomData
    QVERIFY(!QQuickDesignerSupportProperties::getResetValue(rootItem, "width").isValid());

    newItemScopedPointer.reset(); //Delete the item and check if item gets removed from the hash and an invalid QVariant is returned.

    QVERIFY(!QQuickDesignerSupportProperties::getResetValue(newItem, "width").isValid());
}

void tst_qquickdesignersupport::customDataBindings()
{
    QScopedPointer<QQuickView> view(new QQuickView);
    view->engine()->setOutputWarningsToStandardError(false);
    view->setSource(testFileUrl("test.qml"));

    QVERIFY(view->errors().isEmpty());

    QQuickItem *rootItem = view->rootObject();

    QVERIFY(rootItem);

    QQuickItem *testComponent = findItem<QQuickItem>(view->rootObject(), QLatin1String("testComponent"));

    QVERIFY(testComponent);
    QQuickDesignerSupportProperties::registerCustomData(testComponent);
    QQuickDesignerSupportProperties::hasValidResetBinding(testComponent, "x");
    QVERIFY(QQuickDesignerSupportProperties::hasBindingForProperty(testComponent,
                                                           view->engine()->contextForObject(testComponent),
                                                           "x",
                                                           0));

    QCOMPARE(testComponent->property("x").toInt(), 200);


    //Set property to 100 and ovveride the default binding
    QQmlProperty property(testComponent, "x", view->engine()->contextForObject(testComponent));
    QVERIFY(property.write(100));
    QCOMPARE(testComponent->property("x").toInt(), 100);

    QVERIFY(!QQuickDesignerSupportProperties::hasBindingForProperty(testComponent,
                                                           view->engine()->contextForObject(testComponent),
                                                           "x",
                                                           0));

    //Reset the binding to the default
    QQuickDesignerSupportProperties::doResetProperty(testComponent,
                                                     view->engine()->contextForObject(testComponent),
                                                     "x");

    QVERIFY(QQuickDesignerSupportProperties::hasBindingForProperty(testComponent,
                                                           view->engine()->contextForObject(testComponent),
                                                           "x",
                                                           0));
    QCOMPARE(testComponent->property("x").toInt(), 200);



    //Set a different binding/expression
    QQuickDesignerSupportProperties::setPropertyBinding(testComponent,
                                                        view->engine()->contextForObject(testComponent),
                                                        "x",
                                                        QLatin1String("Math.max(0, 300)"));

    QVERIFY(QQuickDesignerSupportProperties::hasBindingForProperty(testComponent,
                                                           view->engine()->contextForObject(testComponent),
                                                           "x",
                                                           0));

    QCOMPARE(testComponent->property("x").toInt(), 300);



    //Reset the binding to the default
    QQuickDesignerSupportProperties::doResetProperty(testComponent,
                                                     view->engine()->contextForObject(testComponent),
                                                     "x");


    QVERIFY(QQuickDesignerSupportProperties::hasBindingForProperty(testComponent,
                                                           view->engine()->contextForObject(testComponent),
                                                           "x",
                                                           0));
    QCOMPARE(testComponent->property("x").toInt(), 200);
}

void tst_qquickdesignersupport::objectProperties()
{
    QScopedPointer<QQuickView> view(new QQuickView);
    view->engine()->setOutputWarningsToStandardError(false);
    view->setSource(testFileUrl("test.qml"));

    QVERIFY(view->errors().isEmpty());

    QQuickItem *rootItem = view->rootObject();

    QVERIFY(rootItem);

    QQuickItem *rectangleItem = findItem<QQuickItem>(view->rootObject(), QLatin1String("rectangleItem"));
    QVERIFY(rectangleItem);


    //Read gradient property as QObject
    int propertyIndex = rectangleItem->metaObject()->indexOfProperty("gradient");
    QVERIFY(propertyIndex > 0);
    QMetaProperty metaProperty = rectangleItem->metaObject()->property(propertyIndex);
    QVERIFY(metaProperty.isValid());

    QVERIFY(QQuickDesignerSupportProperties::isPropertyQObject(metaProperty));

    QObject*gradient = QQuickDesignerSupportProperties::readQObjectProperty(metaProperty, rectangleItem);
    QVERIFY(gradient);


    //The width property is not a QObject
    propertyIndex = rectangleItem->metaObject()->indexOfProperty("width");
    QVERIFY(propertyIndex > 0);
    metaProperty = rectangleItem->metaObject()->property(propertyIndex);
    QVERIFY(metaProperty.isValid());
    QVERIFY(!QQuickDesignerSupportProperties::isPropertyQObject(metaProperty));
}

void tst_qquickdesignersupport::dynamicProperty()
{
    QScopedPointer<QQuickView> view(new QQuickView);
    view->engine()->setOutputWarningsToStandardError(false);
    view->setSource(testFileUrl("test.qml"));

    QVERIFY(view->errors().isEmpty());

    QQuickItem *rootItem = view->rootObject();

    QVERIFY(rootItem);

    QQuickItem *simpleItem = findItem<QQuickItem>(view->rootObject(), QLatin1String("simpleItem"));

    QVERIFY(simpleItem);

    QQuickDesignerSupportProperties::registerNodeInstanceMetaObject(simpleItem, view->engine());
    QQuickDesignerSupportProperties::getPropertyCache(simpleItem, view->engine());

    QQuickDesignerSupportProperties::createNewDynamicProperty(simpleItem, view->engine(), QLatin1String("dynamicProperty"));

    QQmlProperty property(simpleItem, "dynamicProperty", view->engine()->contextForObject(simpleItem));
    QVERIFY(property.isValid());
    QVERIFY(property.write(QLatin1String("test")));


    QCOMPARE(property.read().toString(), QLatin1String("test"));

    //Force evalutation of all bindings
    QQuickDesignerSupport::refreshExpressions(view->rootContext());

    //Check if expression to dynamic property gets properly resolved
    property = QQmlProperty(simpleItem, "testProperty", view->engine()->contextForObject(simpleItem));
    QVERIFY(property.isValid());
    QCOMPARE(property.read().toString(), QLatin1String("test"));
}

void tst_qquickdesignersupport::createComponent()
{
    QScopedPointer<QQuickView> view(new QQuickView);
    view->engine()->setOutputWarningsToStandardError(false);
    view->setSource(testFileUrl("test.qml"));

    QVERIFY(view->errors().isEmpty());

    QQuickItem *rootItem = view->rootObject();

    QVERIFY(rootItem);

    QObject *testComponentObject = QQuickDesignerSupportItems::createComponent(testFileUrl("TestComponent.qml"), view->rootContext());
    QVERIFY(testComponentObject);

    QVERIFY(QQuickDesignerSupportMetaInfo::isSubclassOf(testComponentObject, "QtQuick/Item"));
}

void tst_qquickdesignersupport::basicStates()
{
    QScopedPointer<QQuickView> view(new QQuickView);
    view->engine()->setOutputWarningsToStandardError(false);
    view->setSource(testFileUrl("test.qml"));

    QVERIFY(view->errors().isEmpty());

    QQuickItem *rootItem = view->rootObject();

    QVERIFY(rootItem);

    QQuickStateGroup *stateGroup = QQuickItemPrivate::get(rootItem)->_states();

    QVERIFY(stateGroup);

    QCOMPARE(stateGroup->states().count(), 2 );

    QQuickState *state01 = stateGroup->states().first();
    QQuickState *state02 = stateGroup->states().last();

    QVERIFY(state01);
    QVERIFY(state02);

    QCOMPARE(state01->property("name").toString(), QLatin1String("state01"));
    QCOMPARE(state02->property("name").toString(), QLatin1String("state02"));

    QVERIFY(!QQuickDesignerSupportStates::isStateActive(state01, view->rootContext()));
    QVERIFY(!QQuickDesignerSupportStates::isStateActive(state01, view->rootContext()));

    QQuickDesignerSupportStates::activateState(state01, view->rootContext());
    QVERIFY(QQuickDesignerSupportStates::isStateActive(state01, view->rootContext()));

    QQuickDesignerSupportStates::activateState(state02, view->rootContext());
    QVERIFY(QQuickDesignerSupportStates::isStateActive(state02, view->rootContext()));
    QVERIFY(!QQuickDesignerSupportStates::isStateActive(state01, view->rootContext()));

    QQuickDesignerSupportStates::deactivateState(state02);
    QVERIFY(!QQuickDesignerSupportStates::isStateActive(state01, view->rootContext()));
    QVERIFY(!QQuickDesignerSupportStates::isStateActive(state01, view->rootContext()));
}

void tst_qquickdesignersupport::statesPropertyChanges()
{
    QScopedPointer<QQuickView> view(new QQuickView);
    view->engine()->setOutputWarningsToStandardError(false);
    view->setSource(testFileUrl("test.qml"));

    QVERIFY(view->errors().isEmpty());

    QQuickItem *rootItem = view->rootObject();

    QVERIFY(rootItem);

    QQuickItem *simpleItem = findItem<QQuickItem>(view->rootObject(), QLatin1String("simpleItem"));

    QVERIFY(simpleItem);

    QQuickStateGroup *stateGroup = QQuickItemPrivate::get(rootItem)->_states();

    QVERIFY(stateGroup);

    QCOMPARE(stateGroup->states().count(), 2 );

    QQuickState *state01 = stateGroup->states().first();
    QQuickState *state02 = stateGroup->states().last();

    QVERIFY(state01);
    QVERIFY(state02);

    QCOMPARE(state01->property("name").toString(), QLatin1String("state01"));
    QCOMPARE(state02->property("name").toString(), QLatin1String("state02"));

    //PropertyChanges are parsed lazily
    QQuickDesignerSupportStates::activateState(state01, view->rootContext());
    QQuickDesignerSupportStates::deactivateState(state01);

    QQuickStatePrivate *statePrivate01 = static_cast<QQuickStatePrivate *>(QQuickStatePrivate::get(state01));

    QCOMPARE(state01->operationCount(), 1);

    QCOMPARE(statePrivate01->operations.count(), 1);

    QQuickStateOperation *propertyChange = statePrivate01->operations.at(0).data();

    QCOMPARE(QQuickDesignerSupportPropertyChanges::stateObject(propertyChange), state01);

    QQuickDesignerSupportPropertyChanges::changeValue(propertyChange, "width", 300);

    QCOMPARE(simpleItem->property("width").toInt(), 0);
    QQuickDesignerSupportStates::activateState(state01, view->rootContext());
    QCOMPARE(simpleItem->property("width").toInt(), 300);
    QQuickDesignerSupportStates::deactivateState(state01);
    QCOMPARE(simpleItem->property("width").toInt(), 0);

    //Set "base state value" in state1 using the revert list
    QQuickDesignerSupportStates::activateState(state01, view->rootContext());
    QQuickDesignerSupportStates::changeValueInRevertList(state01, simpleItem, "width", 200);
    QCOMPARE(simpleItem->property("width").toInt(), 300);
    QQuickDesignerSupportStates::deactivateState(state01);
    QCOMPARE(simpleItem->property("width").toInt(), 200);


    //Create new PropertyChanges
    QQuickPropertyChanges *newPropertyChange = new QQuickPropertyChanges();
    newPropertyChange->setParent(state01);
    QQmlListProperty<QQuickStateOperation> changes = state01->changes();
    QQuickStatePrivate::operations_append(&changes, newPropertyChange);

    newPropertyChange->setObject(rootItem);

    QQuickDesignerSupportPropertyChanges::attachToState(newPropertyChange);

    QCOMPARE(rootItem, QQuickDesignerSupportPropertyChanges::targetObject(newPropertyChange));

    QCOMPARE(state01->operationCount(), 2);
    QCOMPARE(statePrivate01->operations.count(), 2);

    QCOMPARE(QQuickDesignerSupportPropertyChanges::stateObject(newPropertyChange), state01);

    //Set color for rootItem in state1
    QQuickDesignerSupportPropertyChanges::changeValue(newPropertyChange, "color", QColor(Qt::red));

    QQuickDesignerSupportStates::activateState(state01, view->rootContext());
    QCOMPARE(rootItem->property("color").value<QColor>(), QColor(Qt::red));
    QQuickDesignerSupportStates::deactivateState(state01);
    QCOMPARE(rootItem->property("color").value<QColor>(), QColor(Qt::white));

    QQuickDesignerSupportPropertyChanges::removeProperty(newPropertyChange, "color");
    QQuickDesignerSupportStates::activateState(state01, view->rootContext());
    QCOMPARE(rootItem->property("color").value<QColor>(), QColor(Qt::white));

}

static QObject * s_object = 0;
static QQuickDesignerSupport::PropertyName  s_propertyName;

static void notifyPropertyChangeCallBackFunction(QObject *object, const QQuickDesignerSupport::PropertyName &propertyName)
{
    s_object = object;
    s_propertyName = propertyName;
}

static void (*notifyPropertyChangeCallBackPointer)(QObject *, const QQuickDesignerSupport::PropertyName &) = &notifyPropertyChangeCallBackFunction;


void tst_qquickdesignersupport::testNotifyPropertyChangeCallBack()
{
    QScopedPointer<QQuickView> view(new QQuickView);
    view->engine()->setOutputWarningsToStandardError(false);
    view->setSource(testFileUrl("test.qml"));

    QVERIFY(view->errors().isEmpty());

    QQuickItem *rootItem = view->rootObject();

    QVERIFY(rootItem);

    QQuickRectangle *rectangle = static_cast<QQuickRectangle *>(rootItem);
    QVERIFY(rectangle);

    QQuickDesignerSupportProperties::registerNodeInstanceMetaObject(rectangle, view->engine());

    QQuickGradient *gradient = new QQuickGradient(rectangle);

    QQuickDesignerSupportMetaInfo::registerNotifyPropertyChangeCallBack(notifyPropertyChangeCallBackPointer);

    rectangle->setProperty("gradient", QVariant::fromValue<QQuickGradient *>(gradient));

    QVERIFY(s_object);
    QCOMPARE(s_object, rootItem);
    QCOMPARE(s_propertyName, QQuickDesignerSupport::PropertyName("gradient"));
}

static void fixResourcePathsForObjectCallBackFunction(QObject *object)
{
    s_object = object;
}

static void (*fixResourcePathsForObjectCallBackPointer)(QObject *) = &fixResourcePathsForObjectCallBackFunction;

void tst_qquickdesignersupport::testFixResourcePathsForObjectCallBack()
{
    QScopedPointer<QQuickView> view(new QQuickView);
    view->engine()->setOutputWarningsToStandardError(false);
    view->setSource(testFileUrl("test.qml"));

    QVERIFY(view->errors().isEmpty());

    QQuickItem *rootItem = view->rootObject();

    QVERIFY(rootItem);

    s_object = 0;

    QQuickDesignerSupportItems::registerFixResourcePathsForObjectCallBack(fixResourcePathsForObjectCallBackPointer);

    QQuickItem *simpleItem = findItem<QQuickItem>(view->rootObject(), QLatin1String("simpleItem"));

    QVERIFY(simpleItem);

    QQuickDesignerSupportItems::tweakObjects(simpleItem);

    //Check that the fixResourcePathsForObjectCallBack was called on simpleItem
    QCOMPARE(simpleItem , s_object);
}


QTEST_MAIN(tst_qquickdesignersupport)

#include "tst_qquickdesignersupport.moc"
