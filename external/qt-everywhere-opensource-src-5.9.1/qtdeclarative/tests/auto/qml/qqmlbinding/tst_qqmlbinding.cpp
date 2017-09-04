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
#include <private/qqmlbind_p.h>
#include <QtQuick/private/qquickrectangle_p.h>
#include "../../shared/util.h"

class tst_qqmlbinding : public QQmlDataTest
{
    Q_OBJECT
public:
    tst_qqmlbinding();

private slots:
    void binding();
    void whenAfterValue();
    void restoreBinding();
    void restoreBindingWithLoop();
    void restoreBindingWithoutCrash();
    void deletedObject();
    void warningOnUnknownProperty();
    void warningOnReadOnlyProperty();
    void disabledOnUnknownProperty();
    void disabledOnReadonlyProperty();
    void delayed();

private:
    QQmlEngine engine;
};

tst_qqmlbinding::tst_qqmlbinding()
{
}

void tst_qqmlbinding::binding()
{
    QQmlEngine engine;
    QQmlComponent c(&engine, testFileUrl("test-binding.qml"));
    QQuickRectangle *rect = qobject_cast<QQuickRectangle*>(c.create());
    QVERIFY(rect != 0);

    QQmlBind *binding3 = qobject_cast<QQmlBind*>(rect->findChild<QQmlBind*>("binding3"));
    QVERIFY(binding3 != 0);

    QCOMPARE(rect->color(), QColor("yellow"));
    QCOMPARE(rect->property("text").toString(), QString("Hello"));
    QCOMPARE(binding3->when(), false);

    rect->setProperty("changeColor", true);
    QCOMPARE(rect->color(), QColor("red"));

    QCOMPARE(binding3->when(), true);

    QQmlBind *binding = qobject_cast<QQmlBind*>(rect->findChild<QQmlBind*>("binding1"));
    QVERIFY(binding != 0);
    QCOMPARE(binding->object(), qobject_cast<QObject*>(rect));
    QCOMPARE(binding->property(), QLatin1String("text"));
    QCOMPARE(binding->value().toString(), QLatin1String("Hello"));

    delete rect;
}

void tst_qqmlbinding::whenAfterValue()
{
    QQmlEngine engine;
    QQmlComponent c(&engine, testFileUrl("test-binding2.qml"));
    QQuickRectangle *rect = qobject_cast<QQuickRectangle*>(c.create());

    QVERIFY(rect != 0);
    QCOMPARE(rect->color(), QColor("yellow"));
    QCOMPARE(rect->property("text").toString(), QString("Hello"));

    rect->setProperty("changeColor", true);
    QCOMPARE(rect->color(), QColor("red"));

    delete rect;
}

void tst_qqmlbinding::restoreBinding()
{
    QQmlEngine engine;
    QQmlComponent c(&engine, testFileUrl("restoreBinding.qml"));
    QQuickRectangle *rect = qobject_cast<QQuickRectangle*>(c.create());
    QVERIFY(rect != 0);

    QQuickRectangle *myItem = qobject_cast<QQuickRectangle*>(rect->findChild<QQuickRectangle*>("myItem"));
    QVERIFY(myItem != 0);

    myItem->setY(25);
    QCOMPARE(myItem->x(), qreal(100-25));

    myItem->setY(13);
    QCOMPARE(myItem->x(), qreal(100-13));

    //Binding takes effect
    myItem->setY(51);
    QCOMPARE(myItem->x(), qreal(51));

    myItem->setY(88);
    QCOMPARE(myItem->x(), qreal(88));

    //original binding restored
    myItem->setY(49);
    QCOMPARE(myItem->x(), qreal(100-49));

    delete rect;
}

void tst_qqmlbinding::restoreBindingWithLoop()
{
    QQmlEngine engine;
    QQmlComponent c(&engine, testFileUrl("restoreBindingWithLoop.qml"));
    QQuickRectangle *rect = qobject_cast<QQuickRectangle*>(c.create());
    QVERIFY(rect != 0);

    QQuickRectangle *myItem = qobject_cast<QQuickRectangle*>(rect->findChild<QQuickRectangle*>("myItem"));
    QVERIFY(myItem != 0);

    myItem->setY(25);
    QCOMPARE(myItem->x(), qreal(25 + 100));

    myItem->setY(13);
    QCOMPARE(myItem->x(), qreal(13 + 100));

    //Binding takes effect
    rect->setProperty("activateBinding", true);
    myItem->setY(51);
    QCOMPARE(myItem->x(), qreal(51));

    myItem->setY(88);
    QCOMPARE(myItem->x(), qreal(88));

    //original binding restored
    QString warning = c.url().toString() + QLatin1String(":9:5: QML Rectangle: Binding loop detected for property \"x\"");
    QTest::ignoreMessage(QtWarningMsg, qPrintable(warning));
    rect->setProperty("activateBinding", false);
    QCOMPARE(myItem->x(), qreal(88 + 100)); //if loop handling changes this could be 90 + 100

    myItem->setY(49);
    QCOMPARE(myItem->x(), qreal(49 + 100));

    delete rect;
}

void tst_qqmlbinding::restoreBindingWithoutCrash()
{
    QQmlEngine engine;
    QQmlComponent c(&engine, testFileUrl("restoreBindingWithoutCrash.qml"));
    QQuickRectangle *rect = qobject_cast<QQuickRectangle*>(c.create());
    QVERIFY(rect != 0);

    QQuickRectangle *myItem = qobject_cast<QQuickRectangle*>(rect->findChild<QQuickRectangle*>("myItem"));
    QVERIFY(myItem != 0);

    myItem->setY(25);
    QCOMPARE(myItem->x(), qreal(100-25));

    myItem->setY(13);
    QCOMPARE(myItem->x(), qreal(100-13));

    //Binding takes effect
    myItem->setY(51);
    QCOMPARE(myItem->x(), qreal(51));

    myItem->setY(88);
    QCOMPARE(myItem->x(), qreal(88));

    //state sets a new binding
    rect->setState("state1");
    //this binding temporarily takes effect. We may want to change this behavior in the future
    QCOMPARE(myItem->x(), qreal(112));

    //Binding still controls this value
    myItem->setY(104);
    QCOMPARE(myItem->x(), qreal(104));

    //original binding restored
    myItem->setY(49);
    QCOMPARE(myItem->x(), qreal(100-49));

    delete rect;
}

//QTBUG-20692
void tst_qqmlbinding::deletedObject()
{
    QQmlEngine engine;
    QQmlComponent c(&engine, testFileUrl("deletedObject.qml"));
    QQuickRectangle *rect = qobject_cast<QQuickRectangle*>(c.create());
    QVERIFY(rect != 0);

    QGuiApplication::sendPostedEvents(0, QEvent::DeferredDelete);

    //don't crash
    rect->setProperty("activateBinding", true);

    delete rect;
}

void tst_qqmlbinding::warningOnUnknownProperty()
{
    QQmlTestMessageHandler messageHandler;

    QQmlEngine engine;
    QQmlComponent c(&engine, testFileUrl("unknownProperty.qml"));
    QQuickItem *item = qobject_cast<QQuickItem*>(c.create());
    QVERIFY(item);
    delete item;

    QCOMPARE(messageHandler.messages().count(), 1);

    const QString expectedMessage = c.url().toString() + QLatin1String(":6:5: QML Binding: Property 'unknown' does not exist on Item.");
    QCOMPARE(messageHandler.messages().first(), expectedMessage);
}

void tst_qqmlbinding::warningOnReadOnlyProperty()
{
    QQmlTestMessageHandler messageHandler;

    QQmlEngine engine;
    QQmlComponent c(&engine, testFileUrl("readonlyProperty.qml"));
    QQuickItem *item = qobject_cast<QQuickItem*>(c.create());
    QVERIFY(item);
    delete item;

    QCOMPARE(messageHandler.messages().count(), 1);

    const QString expectedMessage = c.url().toString() + QLatin1String(":8:5: QML Binding: Property 'name' on Item is read-only.");
    QCOMPARE(messageHandler.messages().first(), expectedMessage);
}

void tst_qqmlbinding::disabledOnUnknownProperty()
{
    QQmlTestMessageHandler messageHandler;

    QQmlEngine engine;
    QQmlComponent c(&engine, testFileUrl("disabledUnknown.qml"));
    QQuickItem *item = qobject_cast<QQuickItem*>(c.create());
    QVERIFY(item);
    delete item;

    QCOMPARE(messageHandler.messages().count(), 0);
}

void tst_qqmlbinding::disabledOnReadonlyProperty()
{
    QQmlTestMessageHandler messageHandler;

    QQmlEngine engine;
    QQmlComponent c(&engine, testFileUrl("disabledReadonly.qml"));
    QQuickItem *item = qobject_cast<QQuickItem*>(c.create());
    QVERIFY(item);
    delete item;

    QCOMPARE(messageHandler.messages().count(), 0);
}

void tst_qqmlbinding::delayed()
{
    QQmlEngine engine;
    QQmlComponent c(&engine, testFileUrl("delayed.qml"));
    QQuickItem *item = qobject_cast<QQuickItem*>(c.create());

    QVERIFY(item != 0);
    // update on creation
    QCOMPARE(item->property("changeCount").toInt(), 1);

    QMetaObject::invokeMethod(item, "updateText");
    // doesn't update immediately
    QCOMPARE(item->property("changeCount").toInt(), 1);

    QCoreApplication::processEvents();
    // only updates once (non-delayed would update twice)
    QCOMPARE(item->property("changeCount").toInt(), 2);

    delete item;
}

QTEST_MAIN(tst_qqmlbinding)

#include "tst_qqmlbinding.moc"
