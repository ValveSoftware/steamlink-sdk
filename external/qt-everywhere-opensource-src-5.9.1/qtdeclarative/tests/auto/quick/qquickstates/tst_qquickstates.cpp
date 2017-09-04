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
#include <private/qquickstateoperations_p.h>
#include <private/qquickanchors_p_p.h>
#include <QtQuick/private/qquickrectangle_p.h>
#include <private/qquickimage_p.h>
#include <QtQuick/private/qquickpropertychanges_p.h>
#include <QtQuick/private/qquickstategroup_p.h>
#include <private/qquickitem_p.h>
#include <private/qqmlproperty_p.h>
#include "../../shared/util.h"

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

class MyRect : public QQuickRectangle
{
   Q_OBJECT
   Q_PROPERTY(int propertyWithNotify READ propertyWithNotify WRITE setPropertyWithNotify NOTIFY oddlyNamedNotifySignal)
public:
    MyRect() {}

    void doSomething() { emit didSomething(); }

    int propertyWithNotify() const { return m_prop; }
    void setPropertyWithNotify(int i) { m_prop = i; emit oddlyNamedNotifySignal(); }

    static MyAttached *qmlAttachedProperties(QObject *o) {
        return new MyAttached(o);
    }
Q_SIGNALS:
    void didSomething();
    void oddlyNamedNotifySignal();

private:
    int m_prop;
};

QML_DECLARE_TYPE(MyRect)
QML_DECLARE_TYPEINFO(MyRect, QML_HAS_ATTACHED_PROPERTIES)

class tst_qquickstates : public QQmlDataTest
{
    Q_OBJECT
public:
    tst_qquickstates() {}

private:
    QByteArray fullDataPath(const QString &path) const;

private slots:
    void initTestCase();

    void basicChanges();
    void attachedPropertyChanges();
    void basicExtension();
    void basicBinding();
    void signalOverride();
    void signalOverrideCrash();
    void signalOverrideCrash2();
    void signalOverrideCrash3();
    void signalOverrideCrash4();
    void parentChange();
    void parentChangeErrors();
    void anchorChanges();
    void anchorChanges2();
    void anchorChanges3();
    void anchorChanges4();
    void anchorChanges5();
    void anchorChangesRTL();
    void anchorChangesRTL2();
    void anchorChangesRTL3();
    void anchorChangesCrash();
    void anchorRewindBug();
    void anchorRewindBug2();
    void script();
    void restoreEntryValues();
    void explicitChanges();
    void propertyErrors();
    void incorrectRestoreBug();
    void autoStateAtStartupRestoreBug();
    void deletingChange();
    void deletingState();
    void tempState();
    void illegalTempState();
    void nonExistantProperty();
    void reset();
    void illegalObjectCreation();
    void whenOrdering();
    void urlResolution();
    void unnamedWhen();
    void returnToBase();
    void extendsBug();
    void editProperties();
    void QTBUG_14830();
    void avoidFastForward();
    void revertListBug();
    void QTBUG_38492();
    void revertListMemoryLeak();
};

void tst_qquickstates::initTestCase()
{
    QQmlDataTest::initTestCase();
    qmlRegisterType<MyRect>("Qt.test", 1, 0, "MyRectangle");
}

QByteArray tst_qquickstates::fullDataPath(const QString &path) const
{
    return testFileUrl(path).toString().toUtf8();
}

void tst_qquickstates::basicChanges()
{
    QQmlEngine engine;

    {
        QQmlComponent rectComponent(&engine, testFileUrl("basicChanges.qml"));
        QQuickRectangle *rect = qobject_cast<QQuickRectangle*>(rectComponent.create());
        QQuickItemPrivate *rectPrivate = QQuickItemPrivate::get(rect);
        QVERIFY(rect != 0);

        QCOMPARE(rect->color(),QColor("red"));

        rectPrivate->setState("blue");
        QCOMPARE(rect->color(),QColor("blue"));

        rectPrivate->setState("");
        QCOMPARE(rect->color(),QColor("red"));
    }

    {
        QQmlComponent rectComponent(&engine, testFileUrl("basicChanges2.qml"));
        QQuickRectangle *rect = qobject_cast<QQuickRectangle*>(rectComponent.create());
        QQuickItemPrivate *rectPrivate = QQuickItemPrivate::get(rect);
        QVERIFY(rect != 0);

        QCOMPARE(rect->color(),QColor("red"));

        rectPrivate->setState("blue");
        QCOMPARE(rect->color(),QColor("blue"));

        rectPrivate->setState("green");
        QCOMPARE(rect->color(),QColor("green"));

        rectPrivate->setState("");
        QCOMPARE(rect->color(),QColor("red"));

        rectPrivate->setState("green");
        QCOMPARE(rect->color(),QColor("green"));
    }

    {
        QQmlComponent rectComponent(&engine, testFileUrl("basicChanges3.qml"));
        QQuickRectangle *rect = qobject_cast<QQuickRectangle*>(rectComponent.create());
        QQuickItemPrivate *rectPrivate = QQuickItemPrivate::get(rect);
        QVERIFY(rect != 0);

        QCOMPARE(rect->color(),QColor("red"));
        QCOMPARE(rect->border()->width(),1.0);

        rectPrivate->setState("blue");
        QCOMPARE(rect->color(),QColor("blue"));
        QCOMPARE(rect->border()->width(),1.0);

        rectPrivate->setState("bordered");
        QCOMPARE(rect->color(),QColor("red"));
        QCOMPARE(rect->border()->width(),2.0);

        rectPrivate->setState("");
        QCOMPARE(rect->color(),QColor("red"));
        QCOMPARE(rect->border()->width(),1.0);
        //### we should be checking that this is an implicit rather than explicit 1 (which currently fails)

        rectPrivate->setState("bordered");
        QCOMPARE(rect->color(),QColor("red"));
        QCOMPARE(rect->border()->width(),2.0);

        rectPrivate->setState("blue");
        QCOMPARE(rect->color(),QColor("blue"));
        QCOMPARE(rect->border()->width(),1.0);

    }

    {
        // Test basicChanges4.qml can magically connect to propertyWithNotify's notify
        // signal using 'onPropertyWithNotifyChanged' even though the signal name is
        // actually 'oddlyNamedNotifySignal'

        QQmlComponent component(&engine, testFileUrl("basicChanges4.qml"));
        QVERIFY(component.isReady());

        MyRect *rect = qobject_cast<MyRect*>(component.create());
        QVERIFY(rect != 0);

        QMetaProperty prop = rect->metaObject()->property(rect->metaObject()->indexOfProperty("propertyWithNotify"));
        QVERIFY(prop.hasNotifySignal());
        QString notifySignal = prop.notifySignal().methodSignature();
        QVERIFY(!notifySignal.startsWith("propertyWithNotifyChanged("));

        QCOMPARE(rect->color(), QColor(Qt::red));

        rect->setPropertyWithNotify(100);
        QCOMPARE(rect->color(), QColor(Qt::blue));
    }
}

void tst_qquickstates::attachedPropertyChanges()
{
    QQmlEngine engine;

    QQmlComponent component(&engine, testFileUrl("attachedPropertyChanges.qml"));
    QVERIFY(component.isReady());

    QQuickItem *item = qobject_cast<QQuickItem*>(component.create());
    QVERIFY(item != 0);
    QCOMPARE(item->width(), 50.0);

    // Ensure attached property has been changed
    QObject *attObj = qmlAttachedPropertiesObject<MyRect>(item, false);
    QVERIFY(attObj);

    MyAttached *att = qobject_cast<MyAttached*>(attObj);
    QVERIFY(att);

    QCOMPARE(att->foo(), 1);
}

void tst_qquickstates::basicExtension()
{
    QQmlEngine engine;

    {
        QQmlComponent rectComponent(&engine, testFileUrl("basicExtension.qml"));
        QQuickRectangle *rect = qobject_cast<QQuickRectangle*>(rectComponent.create());
        QQuickItemPrivate *rectPrivate = QQuickItemPrivate::get(rect);
        QVERIFY(rect != 0);

        QCOMPARE(rect->color(),QColor("red"));
        QCOMPARE(rect->border()->width(),1.0);

        rectPrivate->setState("blue");
        QCOMPARE(rect->color(),QColor("blue"));
        QCOMPARE(rect->border()->width(),1.0);

        rectPrivate->setState("bordered");
        QCOMPARE(rect->color(),QColor("blue"));
        QCOMPARE(rect->border()->width(),2.0);

        rectPrivate->setState("blue");
        QCOMPARE(rect->color(),QColor("blue"));
        QCOMPARE(rect->border()->width(),1.0);

        rectPrivate->setState("");
        QCOMPARE(rect->color(),QColor("red"));
        QCOMPARE(rect->border()->width(),1.0);

        rectPrivate->setState("bordered");
        QCOMPARE(rect->color(),QColor("blue"));
        QCOMPARE(rect->border()->width(),2.0);

        rectPrivate->setState("");
        QCOMPARE(rect->color(),QColor("red"));
        QCOMPARE(rect->border()->width(),1.0);
    }

    {
        QQmlComponent rectComponent(&engine, testFileUrl("fakeExtension.qml"));
        QQuickRectangle *rect = qobject_cast<QQuickRectangle*>(rectComponent.create());
        QQuickItemPrivate *rectPrivate = QQuickItemPrivate::get(rect);
        QVERIFY(rect != 0);

        QCOMPARE(rect->color(),QColor("red"));

        rectPrivate->setState("blue");
        QCOMPARE(rect->color(),QColor("blue"));

        rectPrivate->setState("green");
        QCOMPARE(rect->color(),QColor("green"));

        rectPrivate->setState("blue");
        QCOMPARE(rect->color(),QColor("blue"));

        rectPrivate->setState("green");
        QCOMPARE(rect->color(),QColor("green"));

        rectPrivate->setState("");
        QCOMPARE(rect->color(),QColor("red"));

        rectPrivate->setState("green");
        QCOMPARE(rect->color(),QColor("green"));
    }
}

void tst_qquickstates::basicBinding()
{
    QQmlEngine engine;

    {
        QQmlComponent rectComponent(&engine, testFileUrl("basicBinding.qml"));
        QQuickRectangle *rect = qobject_cast<QQuickRectangle*>(rectComponent.create());
        QQuickItemPrivate *rectPrivate = QQuickItemPrivate::get(rect);
        QVERIFY(rect != 0);

        QCOMPARE(rect->color(),QColor("red"));

        rectPrivate->setState("blue");
        QCOMPARE(rect->color(),QColor("blue"));

        rectPrivate->setState("");
        QCOMPARE(rect->color(),QColor("red"));

        rectPrivate->setState("blue");
        QCOMPARE(rect->color(),QColor("blue"));
        rect->setProperty("sourceColor", QColor("green"));
        QCOMPARE(rect->color(),QColor("green"));

        rectPrivate->setState("");
        QCOMPARE(rect->color(),QColor("red"));
        rect->setProperty("sourceColor", QColor("yellow"));
        QCOMPARE(rect->color(),QColor("red"));

        rectPrivate->setState("blue");
        QCOMPARE(rect->color(),QColor("yellow"));
    }

    {
        QQmlComponent rectComponent(&engine, testFileUrl("basicBinding2.qml"));
        QQuickRectangle *rect = qobject_cast<QQuickRectangle*>(rectComponent.create());
        QQuickItemPrivate *rectPrivate = QQuickItemPrivate::get(rect);
        QVERIFY(rect != 0);

        QCOMPARE(rect->color(),QColor("red"));

        rectPrivate->setState("blue");
        QCOMPARE(rect->color(),QColor("blue"));

        rectPrivate->setState("");
        QCOMPARE(rect->color(),QColor("red"));

        rectPrivate->setState("blue");
        QCOMPARE(rect->color(),QColor("blue"));
        rect->setProperty("sourceColor", QColor("green"));
        QCOMPARE(rect->color(),QColor("blue"));

        rectPrivate->setState("");
        QCOMPARE(rect->color(),QColor("green"));
        rect->setProperty("sourceColor", QColor("yellow"));
        QCOMPARE(rect->color(),QColor("yellow"));

        rectPrivate->setState("blue");
        QCOMPARE(rect->color(),QColor("blue"));

        rectPrivate->setState("");
        QCOMPARE(rect->color(),QColor("yellow"));
    }

    {
        QQmlComponent rectComponent(&engine, testFileUrl("basicBinding3.qml"));
        QQuickRectangle *rect = qobject_cast<QQuickRectangle*>(rectComponent.create());
        QQuickItemPrivate *rectPrivate = QQuickItemPrivate::get(rect);
        QVERIFY(rect != 0);

        QCOMPARE(rect->color(),QColor("red"));
        rect->setProperty("sourceColor", QColor("green"));
        QCOMPARE(rect->color(),QColor("green"));

        rectPrivate->setState("blue");
        QCOMPARE(rect->color(),QColor("blue"));
        rect->setProperty("sourceColor", QColor("red"));
        QCOMPARE(rect->color(),QColor("blue"));
        rect->setProperty("sourceColor2", QColor("yellow"));
        QCOMPARE(rect->color(),QColor("yellow"));

        rectPrivate->setState("");
        QCOMPARE(rect->color(),QColor("red"));
        rect->setProperty("sourceColor2", QColor("green"));
        QCOMPARE(rect->color(),QColor("red"));
        rect->setProperty("sourceColor", QColor("yellow"));
        QCOMPARE(rect->color(),QColor("yellow"));
    }

    {
        QQmlComponent rectComponent(&engine, testFileUrl("basicBinding4.qml"));
        QQuickRectangle *rect = qobject_cast<QQuickRectangle*>(rectComponent.create());
        QQuickItemPrivate *rectPrivate = QQuickItemPrivate::get(rect);
        QVERIFY(rect != 0);

        QCOMPARE(rect->color(),QColor("red"));

        rectPrivate->setState("blue");
        QCOMPARE(rect->color(),QColor("blue"));
        rect->setProperty("sourceColor", QColor("yellow"));
        QCOMPARE(rect->color(),QColor("yellow"));

        rectPrivate->setState("green");
        QCOMPARE(rect->color(),QColor("green"));
        rect->setProperty("sourceColor", QColor("purple"));
        QCOMPARE(rect->color(),QColor("green"));

        rectPrivate->setState("blue");
        QCOMPARE(rect->color(),QColor("purple"));

        rectPrivate->setState("green");
        QCOMPARE(rect->color(),QColor("green"));

        rectPrivate->setState("");
        QCOMPARE(rect->color(),QColor("red"));
    }
}

void tst_qquickstates::signalOverride()
{
    QQmlEngine engine;

    {
        QQmlComponent rectComponent(&engine, testFileUrl("signalOverride.qml"));
        MyRect *rect = qobject_cast<MyRect*>(rectComponent.create());
        QVERIFY(rect != 0);

        QCOMPARE(rect->color(),QColor("red"));
        rect->doSomething();
        QCOMPARE(rect->color(),QColor("blue"));

        QQuickItemPrivate::get(rect)->setState("green");
        rect->doSomething();
        QCOMPARE(rect->color(),QColor("green"));

        delete rect;
    }

    {
        QQmlComponent rectComponent(&engine, testFileUrl("signalOverride2.qml"));
        MyRect *rect = qobject_cast<MyRect*>(rectComponent.create());
        QVERIFY(rect != 0);

        QCOMPARE(rect->color(),QColor("white"));
        rect->doSomething();
        QCOMPARE(rect->color(),QColor("blue"));

        QQuickRectangle *innerRect = qobject_cast<QQuickRectangle*>(rect->findChild<QQuickRectangle*>("extendedRect"));
        QQuickItemPrivate::get(innerRect)->setState("green");
        rect->doSomething();
        QCOMPARE(rect->color(),QColor("blue"));
        QCOMPARE(innerRect->color(),QColor("green"));
        QCOMPARE(innerRect->property("extendedColor").value<QColor>(),QColor("green"));

        delete rect;
    }
}

void tst_qquickstates::signalOverrideCrash()
{
    QQmlEngine engine;

    QQmlComponent rectComponent(&engine, testFileUrl("signalOverrideCrash.qml"));
    MyRect *rect = qobject_cast<MyRect*>(rectComponent.create());
    QVERIFY(rect != 0);

    QQuickItemPrivate::get(rect)->setState("overridden");
    rect->doSomething();
}

void tst_qquickstates::signalOverrideCrash2()
{
    QQmlEngine engine;

    QQmlComponent rectComponent(&engine, testFileUrl("signalOverrideCrash2.qml"));
    QQuickRectangle *rect = qobject_cast<QQuickRectangle*>(rectComponent.create());
    QVERIFY(rect != 0);

    QQuickItemPrivate::get(rect)->setState("state1");
    QQuickItemPrivate::get(rect)->setState("state2");
    QQuickItemPrivate::get(rect)->setState("state1");

    delete rect;
}

void tst_qquickstates::signalOverrideCrash3()
{
    QQmlEngine engine;

    QQmlComponent rectComponent(&engine, testFileUrl("signalOverrideCrash3.qml"));
    QQuickRectangle *rect = qobject_cast<QQuickRectangle*>(rectComponent.create());
    QVERIFY(rect != 0);

    QQuickItemPrivate::get(rect)->setState("state1");
    QQuickItemPrivate::get(rect)->setState("");
    QQuickItemPrivate::get(rect)->setState("state2");
    QQuickItemPrivate::get(rect)->setState("");

    delete rect;
}

void tst_qquickstates::signalOverrideCrash4()
{
    QQmlEngine engine;
    QQmlComponent c(&engine, testFileUrl("signalOverrideCrash4.qml"));
    QQuickRectangle *rect = qobject_cast<QQuickRectangle*>(c.create());
    QVERIFY(rect != 0);

    QQuickItemPrivate *rectPrivate = QQuickItemPrivate::get(rect);

    rectPrivate->setState("state1");
    rectPrivate->setState("state2");
    rectPrivate->setState("state1");
    rectPrivate->setState("state2");
    rectPrivate->setState("");

    delete rect;
}

void tst_qquickstates::parentChange()
{
    QQmlEngine engine;

    {
        QQmlComponent rectComponent(&engine, testFileUrl("parentChange1.qml"));
        QQuickRectangle *rect = qobject_cast<QQuickRectangle*>(rectComponent.create());
        QVERIFY(rect != 0);

        QQuickRectangle *innerRect = qobject_cast<QQuickRectangle*>(rect->findChild<QQuickRectangle*>("MyRect"));
        QVERIFY(innerRect != 0);

        QQmlListReference list(rect, "states");
        QQuickState *state = qobject_cast<QQuickState*>(list.at(0));
        QVERIFY(state != 0);

        qmlExecuteDeferred(state);
        QQuickParentChange *pChange = qobject_cast<QQuickParentChange*>(state->operationAt(0));
        QVERIFY(pChange != 0);
        QQuickItem *nParent = qobject_cast<QQuickItem*>(rect->findChild<QQuickItem*>("NewParent"));
        QVERIFY(nParent != 0);

        QCOMPARE(pChange->parent(), nParent);

        QQuickItemPrivate::get(rect)->setState("reparented");
        QCOMPARE(innerRect->rotation(), qreal(0));
        QCOMPARE(innerRect->scale(), qreal(1));
        QCOMPARE(innerRect->x(), qreal(-133));
        QCOMPARE(innerRect->y(), qreal(-300));
    }

    {
        QQmlComponent rectComponent(&engine, testFileUrl("parentChange2.qml"));
        QQuickRectangle *rect = qobject_cast<QQuickRectangle*>(rectComponent.create());
        QVERIFY(rect != 0);
        QQuickItemPrivate *rectPrivate = QQuickItemPrivate::get(rect);
        QQuickRectangle *innerRect = qobject_cast<QQuickRectangle*>(rect->findChild<QQuickRectangle*>("MyRect"));
        QVERIFY(innerRect != 0);

        rectPrivate->setState("reparented");
        QCOMPARE(innerRect->rotation(), qreal(15));
        QCOMPARE(innerRect->scale(), qreal(.5));
        QCOMPARE(QString("%1").arg(innerRect->x()), QString("%1").arg(-19.9075));
        QCOMPARE(QString("%1").arg(innerRect->y()), QString("%1").arg(-8.73433));
    }

    {
        QQmlComponent rectComponent(&engine, testFileUrl("parentChange3.qml"));
        QQuickRectangle *rect = qobject_cast<QQuickRectangle*>(rectComponent.create());
        QVERIFY(rect != 0);
        QQuickItemPrivate *rectPrivate = QQuickItemPrivate::get(rect);
        QQuickRectangle *innerRect = qobject_cast<QQuickRectangle*>(rect->findChild<QQuickRectangle*>("MyRect"));
        QVERIFY(innerRect != 0);

        rectPrivate->setState("reparented");
        QCOMPARE(innerRect->rotation(), qreal(-37));
        QCOMPARE(innerRect->scale(), qreal(.25));
        QCOMPARE(QString("%1").arg(innerRect->x()), QString("%1").arg(-217.305));
        QCOMPARE(QString("%1").arg(innerRect->y()), QString("%1").arg(-164.413));

        rectPrivate->setState("");
        QCOMPARE(innerRect->rotation(), qreal(0));
        QCOMPARE(innerRect->scale(), qreal(1));
        QCOMPARE(innerRect->x(), qreal(5));
        //do a non-qFuzzyCompare fuzzy compare
        QVERIFY(innerRect->y() < qreal(0.00001) && innerRect->y() > qreal(-0.00001));
    }

    {
        QQmlComponent rectComponent(&engine, testFileUrl("parentChange6.qml"));
        QQuickRectangle *rect = qobject_cast<QQuickRectangle*>(rectComponent.create());
        QVERIFY(rect != 0);

        QQuickRectangle *innerRect = qobject_cast<QQuickRectangle*>(rect->findChild<QQuickRectangle*>("MyRect"));
        QVERIFY(innerRect != 0);

        QQuickItemPrivate::get(rect)->setState("reparented");
        QCOMPARE(innerRect->rotation(), qreal(180));
        QCOMPARE(innerRect->scale(), qreal(1));
        QCOMPARE(innerRect->x(), qreal(-105));
        QCOMPARE(innerRect->y(), qreal(-105));
    }
}

void tst_qquickstates::parentChangeErrors()
{
    QQmlEngine engine;

    {
        QQmlComponent rectComponent(&engine, testFileUrl("parentChange4.qml"));
        QQuickRectangle *rect = qobject_cast<QQuickRectangle*>(rectComponent.create());
        QVERIFY(rect != 0);

        QQuickRectangle *innerRect = qobject_cast<QQuickRectangle*>(rect->findChild<QQuickRectangle*>("MyRect"));
        QVERIFY(innerRect != 0);

        QTest::ignoreMessage(QtWarningMsg, fullDataPath("parentChange4.qml") + ":25:9: QML ParentChange: Unable to preserve appearance under non-uniform scale");
        QQuickItemPrivate::get(rect)->setState("reparented");
        QCOMPARE(innerRect->rotation(), qreal(0));
        QCOMPARE(innerRect->scale(), qreal(1));
        QCOMPARE(innerRect->x(), qreal(5));
        QCOMPARE(innerRect->y(), qreal(5));
    }

    {
        QQmlComponent rectComponent(&engine, testFileUrl("parentChange5.qml"));
        QQuickRectangle *rect = qobject_cast<QQuickRectangle*>(rectComponent.create());
        QVERIFY(rect != 0);

        QQuickRectangle *innerRect = qobject_cast<QQuickRectangle*>(rect->findChild<QQuickRectangle*>("MyRect"));
        QVERIFY(innerRect != 0);

        QTest::ignoreMessage(QtWarningMsg, fullDataPath("parentChange5.qml") + ":25:9: QML ParentChange: Unable to preserve appearance under complex transform");
        QQuickItemPrivate::get(rect)->setState("reparented");
        QCOMPARE(innerRect->rotation(), qreal(0));
        QCOMPARE(innerRect->scale(), qreal(1));
        QCOMPARE(innerRect->x(), qreal(5));
        QCOMPARE(innerRect->y(), qreal(5));
    }
}

void tst_qquickstates::anchorChanges()
{
    QQmlEngine engine;

    QQmlComponent rectComponent(&engine, testFileUrl("anchorChanges1.qml"));
    QQuickRectangle *rect = qobject_cast<QQuickRectangle*>(rectComponent.create());
    QVERIFY(rect != 0);
    QQuickItemPrivate *rectPrivate = QQuickItemPrivate::get(rect);

    QQuickRectangle *innerRect = qobject_cast<QQuickRectangle*>(rect->findChild<QQuickRectangle*>("MyRect"));
    QVERIFY(innerRect != 0);

    QQmlListReference list(rect, "states");
    QQuickState *state = qobject_cast<QQuickState*>(list.at(0));
    QVERIFY(state != 0);

    qmlExecuteDeferred(state);
    QQuickAnchorChanges *aChanges = qobject_cast<QQuickAnchorChanges*>(state->operationAt(0));
    QVERIFY(aChanges != 0);

    QCOMPARE(aChanges->anchors()->left().isUndefinedLiteral(), true);
    QVERIFY(!aChanges->anchors()->left().isEmpty());
    QVERIFY(!aChanges->anchors()->right().isEmpty());

    rectPrivate->setState("right");
    QCOMPARE(innerRect->x(), qreal(150));
    QCOMPARE(aChanges->object(), qobject_cast<QQuickItem*>(innerRect));
    QCOMPARE(QQuickItemPrivate::get(aChanges->object())->anchors()->left().anchorLine, QQuickAnchors::InvalidAnchor);  //### was reset (how do we distinguish from not set at all)
    QCOMPARE(QQuickItemPrivate::get(aChanges->object())->anchors()->right().item, rectPrivate->right().item);
    QCOMPARE(QQuickItemPrivate::get(aChanges->object())->anchors()->right().anchorLine, rectPrivate->right().anchorLine);

    rectPrivate->setState("");
    QCOMPARE(innerRect->x(), qreal(5));

    delete rect;
}

void tst_qquickstates::anchorChanges2()
{
    QQmlEngine engine;

    QQmlComponent rectComponent(&engine, testFileUrl("anchorChanges2.qml"));
    QQuickRectangle *rect = qobject_cast<QQuickRectangle*>(rectComponent.create());
    QVERIFY(rect != 0);
    QQuickItemPrivate *rectPrivate = QQuickItemPrivate::get(rect);

    QQuickRectangle *innerRect = qobject_cast<QQuickRectangle*>(rect->findChild<QQuickRectangle*>("MyRect"));
    QVERIFY(innerRect != 0);

    rectPrivate->setState("right");
    QCOMPARE(innerRect->x(), qreal(150));

    rectPrivate->setState("");
    QCOMPARE(innerRect->x(), qreal(5));

    delete rect;
}

void tst_qquickstates::anchorChanges3()
{
    QQmlEngine engine;

    QQmlComponent rectComponent(&engine, testFileUrl("anchorChanges3.qml"));
    QQuickRectangle *rect = qobject_cast<QQuickRectangle*>(rectComponent.create());
    QVERIFY(rect != 0);
    QQuickItemPrivate *rectPrivate = QQuickItemPrivate::get(rect);

    QQuickRectangle *innerRect = qobject_cast<QQuickRectangle*>(rect->findChild<QQuickRectangle*>("MyRect"));
    QVERIFY(innerRect != 0);

    QQuickItem *leftGuideline = qobject_cast<QQuickItem*>(rect->findChild<QQuickItem*>("LeftGuideline"));
    QVERIFY(leftGuideline != 0);

    QQuickItem *bottomGuideline = qobject_cast<QQuickItem*>(rect->findChild<QQuickItem*>("BottomGuideline"));
    QVERIFY(bottomGuideline != 0);

    QQmlListReference list(rect, "states");
    QQuickState *state = qobject_cast<QQuickState*>(list.at(0));
    QVERIFY(state != 0);

    qmlExecuteDeferred(state);
    QQuickAnchorChanges *aChanges = qobject_cast<QQuickAnchorChanges*>(state->operationAt(0));
    QVERIFY(aChanges != 0);

    QVERIFY(!aChanges->anchors()->top().isEmpty());
    QVERIFY(!aChanges->anchors()->bottom().isEmpty());

    rectPrivate->setState("reanchored");
    QCOMPARE(aChanges->object(), qobject_cast<QQuickItem*>(innerRect));
    QCOMPARE(QQuickItemPrivate::get(aChanges->object())->anchors()->left().item, QQuickItemPrivate::get(leftGuideline)->left().item);
    QCOMPARE(QQuickItemPrivate::get(aChanges->object())->anchors()->left().anchorLine, QQuickItemPrivate::get(leftGuideline)->left().anchorLine);
    QCOMPARE(QQuickItemPrivate::get(aChanges->object())->anchors()->right().item, rectPrivate->right().item);
    QCOMPARE(QQuickItemPrivate::get(aChanges->object())->anchors()->right().anchorLine, rectPrivate->right().anchorLine);
    QCOMPARE(QQuickItemPrivate::get(aChanges->object())->anchors()->top().item, rectPrivate->top().item);
    QCOMPARE(QQuickItemPrivate::get(aChanges->object())->anchors()->top().anchorLine, rectPrivate->top().anchorLine);
    QCOMPARE(QQuickItemPrivate::get(aChanges->object())->anchors()->bottom().item, QQuickItemPrivate::get(bottomGuideline)->bottom().item);
    QCOMPARE(QQuickItemPrivate::get(aChanges->object())->anchors()->bottom().anchorLine, QQuickItemPrivate::get(bottomGuideline)->bottom().anchorLine);

    QCOMPARE(innerRect->x(), qreal(10));
    QCOMPARE(innerRect->y(), qreal(0));
    QCOMPARE(innerRect->width(), qreal(190));
    QCOMPARE(innerRect->height(), qreal(150));

    rectPrivate->setState("");
    QCOMPARE(innerRect->x(), qreal(0));
    QCOMPARE(innerRect->y(), qreal(10));
    QCOMPARE(innerRect->width(), qreal(150));
    QCOMPARE(innerRect->height(), qreal(190));

    delete rect;
}

void tst_qquickstates::anchorChanges4()
{
    QQmlEngine engine;

    QQmlComponent rectComponent(&engine, testFileUrl("anchorChanges4.qml"));
    QQuickRectangle *rect = qobject_cast<QQuickRectangle*>(rectComponent.create());
    QVERIFY(rect != 0);

    QQuickRectangle *innerRect = qobject_cast<QQuickRectangle*>(rect->findChild<QQuickRectangle*>("MyRect"));
    QVERIFY(innerRect != 0);

    QQuickItem *leftGuideline = qobject_cast<QQuickItem*>(rect->findChild<QQuickItem*>("LeftGuideline"));
    QVERIFY(leftGuideline != 0);

    QQuickItem *bottomGuideline = qobject_cast<QQuickItem*>(rect->findChild<QQuickItem*>("BottomGuideline"));
    QVERIFY(bottomGuideline != 0);

    QQmlListReference list(rect, "states");
    QQuickState *state = qobject_cast<QQuickState*>(list.at(0));
    QVERIFY(state != 0);

    qmlExecuteDeferred(state);
    QQuickAnchorChanges *aChanges = qobject_cast<QQuickAnchorChanges*>(state->operationAt(0));
    QVERIFY(aChanges != 0);

    QVERIFY(!aChanges->anchors()->horizontalCenter().isEmpty());
    QVERIFY(!aChanges->anchors()->verticalCenter().isEmpty());

    QQuickItemPrivate::get(rect)->setState("reanchored");
    QCOMPARE(aChanges->object(), qobject_cast<QQuickItem*>(innerRect));
    QCOMPARE(QQuickItemPrivate::get(aChanges->object())->anchors()->horizontalCenter().item, QQuickItemPrivate::get(bottomGuideline)->horizontalCenter().item);
    QCOMPARE(QQuickItemPrivate::get(aChanges->object())->anchors()->horizontalCenter().anchorLine, QQuickItemPrivate::get(bottomGuideline)->horizontalCenter().anchorLine);
    QCOMPARE(QQuickItemPrivate::get(aChanges->object())->anchors()->verticalCenter().item, QQuickItemPrivate::get(leftGuideline)->verticalCenter().item);
    QCOMPARE(QQuickItemPrivate::get(aChanges->object())->anchors()->verticalCenter().anchorLine, QQuickItemPrivate::get(leftGuideline)->verticalCenter().anchorLine);

    delete rect;
}

void tst_qquickstates::anchorChanges5()
{
    QQmlEngine engine;

    QQmlComponent rectComponent(&engine, testFileUrl("anchorChanges5.qml"));
    QQuickRectangle *rect = qobject_cast<QQuickRectangle*>(rectComponent.create());
    QVERIFY(rect != 0);

    QQuickRectangle *innerRect = qobject_cast<QQuickRectangle*>(rect->findChild<QQuickRectangle*>("MyRect"));
    QVERIFY(innerRect != 0);

    QQuickItem *leftGuideline = qobject_cast<QQuickItem*>(rect->findChild<QQuickItem*>("LeftGuideline"));
    QVERIFY(leftGuideline != 0);

    QQuickItem *bottomGuideline = qobject_cast<QQuickItem*>(rect->findChild<QQuickItem*>("BottomGuideline"));
    QVERIFY(bottomGuideline != 0);

    QQmlListReference list(rect, "states");
    QQuickState *state = qobject_cast<QQuickState*>(list.at(0));
    QVERIFY(state != 0);

    qmlExecuteDeferred(state);
    QQuickAnchorChanges *aChanges = qobject_cast<QQuickAnchorChanges*>(state->operationAt(0));
    QVERIFY(aChanges != 0);

    QVERIFY(!aChanges->anchors()->baseline().isEmpty());

    QQuickItemPrivate::get(rect)->setState("reanchored");
    QCOMPARE(aChanges->object(), qobject_cast<QQuickItem*>(innerRect));
    QCOMPARE(QQuickItemPrivate::get(aChanges->object())->anchors()->horizontalCenter().item, QQuickItemPrivate::get(bottomGuideline)->horizontalCenter().item);
    QCOMPARE(QQuickItemPrivate::get(aChanges->object())->anchors()->horizontalCenter().anchorLine, QQuickItemPrivate::get(bottomGuideline)->horizontalCenter().anchorLine);
    QCOMPARE(QQuickItemPrivate::get(aChanges->object())->anchors()->baseline().item, QQuickItemPrivate::get(leftGuideline)->baseline().item);
    QCOMPARE(QQuickItemPrivate::get(aChanges->object())->anchors()->baseline().anchorLine, QQuickItemPrivate::get(leftGuideline)->baseline().anchorLine);

    delete rect;
}

void mirrorAnchors(QQuickItem *item) {
    QQuickItemPrivate *itemPrivate = QQuickItemPrivate::get(item);
    itemPrivate->setLayoutMirror(true);
}

qreal offsetRTL(QQuickItem *anchorItem, QQuickItem *item) {
    return anchorItem->width()+2*anchorItem->x()-item->width();
}

void tst_qquickstates::anchorChangesRTL()
{
    QQmlEngine engine;

    QQmlComponent rectComponent(&engine, testFileUrl("anchorChanges1.qml"));
    QQuickRectangle *rect = qobject_cast<QQuickRectangle*>(rectComponent.create());
    QVERIFY(rect != 0);
    QQuickItemPrivate *rectPrivate = QQuickItemPrivate::get(rect);

    QQuickRectangle *innerRect = qobject_cast<QQuickRectangle*>(rect->findChild<QQuickRectangle*>("MyRect"));
    QVERIFY(innerRect != 0);
    mirrorAnchors(innerRect);

    QQmlListReference list(rect, "states");
    QQuickState *state = qobject_cast<QQuickState*>(list.at(0));
    QVERIFY(state != 0);

    qmlExecuteDeferred(state);
    QQuickAnchorChanges *aChanges = qobject_cast<QQuickAnchorChanges*>(state->operationAt(0));
    QVERIFY(aChanges != 0);

    rectPrivate->setState("right");
    QCOMPARE(innerRect->x(), offsetRTL(rect, innerRect) - qreal(150));
    QCOMPARE(aChanges->object(), qobject_cast<QQuickItem*>(innerRect));
    QCOMPARE(QQuickItemPrivate::get(aChanges->object())->anchors()->left().anchorLine, QQuickAnchors::InvalidAnchor);  //### was reset (how do we distinguish from not set at all)
    QCOMPARE(QQuickItemPrivate::get(aChanges->object())->anchors()->right().item, rectPrivate->right().item);
    QCOMPARE(QQuickItemPrivate::get(aChanges->object())->anchors()->right().anchorLine, rectPrivate->right().anchorLine);

    rectPrivate->setState("");
    QCOMPARE(innerRect->x(), offsetRTL(rect, innerRect) -qreal(5));

    delete rect;
}

void tst_qquickstates::anchorChangesRTL2()
{
    QQmlEngine engine;

    QQmlComponent rectComponent(&engine, testFileUrl("anchorChanges2.qml"));
    QQuickRectangle *rect = qobject_cast<QQuickRectangle*>(rectComponent.create());
    QVERIFY(rect != 0);
    QQuickItemPrivate *rectPrivate = QQuickItemPrivate::get(rect);

    QQuickRectangle *innerRect = qobject_cast<QQuickRectangle*>(rect->findChild<QQuickRectangle*>("MyRect"));
    QVERIFY(innerRect != 0);
    mirrorAnchors(innerRect);

    rectPrivate->setState("right");
    QCOMPARE(innerRect->x(), offsetRTL(rect, innerRect) - qreal(150));

    rectPrivate->setState("");
    QCOMPARE(innerRect->x(), offsetRTL(rect, innerRect) - qreal(5));

    delete rect;
}

void tst_qquickstates::anchorChangesRTL3()
{
    QQmlEngine engine;

    QQmlComponent rectComponent(&engine, testFileUrl("anchorChanges3.qml"));
    QQuickRectangle *rect = qobject_cast<QQuickRectangle*>(rectComponent.create());
    QVERIFY(rect != 0);
    QQuickItemPrivate *rectPrivate = QQuickItemPrivate::get(rect);

    QQuickRectangle *innerRect = qobject_cast<QQuickRectangle*>(rect->findChild<QQuickRectangle*>("MyRect"));
    QVERIFY(innerRect != 0);
    mirrorAnchors(innerRect);

    QQuickItem *leftGuideline = qobject_cast<QQuickItem*>(rect->findChild<QQuickItem*>("LeftGuideline"));
    QVERIFY(leftGuideline != 0);

    QQuickItem *bottomGuideline = qobject_cast<QQuickItem*>(rect->findChild<QQuickItem*>("BottomGuideline"));
    QVERIFY(bottomGuideline != 0);

    QQmlListReference list(rect, "states");
    QQuickState *state = qobject_cast<QQuickState*>(list.at(0));
    QVERIFY(state != 0);

    qmlExecuteDeferred(state);
    QQuickAnchorChanges *aChanges = qobject_cast<QQuickAnchorChanges*>(state->operationAt(0));
    QVERIFY(aChanges != 0);

    rectPrivate->setState("reanchored");
    QCOMPARE(aChanges->object(), qobject_cast<QQuickItem*>(innerRect));
    QCOMPARE(QQuickItemPrivate::get(aChanges->object())->anchors()->left().item, QQuickItemPrivate::get(leftGuideline)->left().item);
    QCOMPARE(QQuickItemPrivate::get(aChanges->object())->anchors()->left().anchorLine, QQuickItemPrivate::get(leftGuideline)->left().anchorLine);
    QCOMPARE(QQuickItemPrivate::get(aChanges->object())->anchors()->right().item, rectPrivate->right().item);
    QCOMPARE(QQuickItemPrivate::get(aChanges->object())->anchors()->right().anchorLine, rectPrivate->right().anchorLine);
    QCOMPARE(QQuickItemPrivate::get(aChanges->object())->anchors()->top().item, rectPrivate->top().item);
    QCOMPARE(QQuickItemPrivate::get(aChanges->object())->anchors()->top().anchorLine, rectPrivate->top().anchorLine);
    QCOMPARE(QQuickItemPrivate::get(aChanges->object())->anchors()->bottom().item, QQuickItemPrivate::get(bottomGuideline)->bottom().item);
    QCOMPARE(QQuickItemPrivate::get(aChanges->object())->anchors()->bottom().anchorLine, QQuickItemPrivate::get(bottomGuideline)->bottom().anchorLine);

    QCOMPARE(innerRect->x(), offsetRTL(leftGuideline, innerRect) - qreal(10));
    QCOMPARE(innerRect->y(), qreal(0));
    // between left side of parent and leftGuideline.x: 10, which has width 0
    QCOMPARE(innerRect->width(), qreal(10));
    QCOMPARE(innerRect->height(), qreal(150));

    rectPrivate->setState("");
    QCOMPARE(innerRect->x(), offsetRTL(rect, innerRect) - qreal(0));
    QCOMPARE(innerRect->y(), qreal(10));
    // between right side of parent and left side of rightGuideline.x: 150, which has width 0
    QCOMPARE(innerRect->width(), qreal(50));
    QCOMPARE(innerRect->height(), qreal(190));

    delete rect;
}

//QTBUG-9609
void tst_qquickstates::anchorChangesCrash()
{
    QQmlEngine engine;

    QQmlComponent rectComponent(&engine, testFileUrl("anchorChangesCrash.qml"));
    QQuickRectangle *rect = qobject_cast<QQuickRectangle*>(rectComponent.create());
    QVERIFY(rect != 0);

    QQuickItemPrivate::get(rect)->setState("reanchored");

    delete rect;
}

// QTBUG-12273
void tst_qquickstates::anchorRewindBug()
{
    QQuickView *view = new QQuickView;
    view->setSource(testFileUrl("anchorRewindBug.qml"));

    view->show();

    QVERIFY(QTest::qWaitForWindowExposed(view));

    QQuickRectangle *rect = qobject_cast<QQuickRectangle*>(view->rootObject());
    QVERIFY(rect != 0);

    QQuickItem * column = rect->findChild<QQuickItem*>("column");

    QVERIFY(column != 0);
    QVERIFY(!QQuickItemPrivate::get(column)->heightValid);
    QVERIFY(!QQuickItemPrivate::get(column)->widthValid);
    QCOMPARE(column->height(), 200.0);
    QQuickItemPrivate::get(rect)->setState("reanchored");

    // column height and width should stay implicit
    // and column's implicit resizing should still work
    QVERIFY(!QQuickItemPrivate::get(column)->heightValid);
    QVERIFY(!QQuickItemPrivate::get(column)->widthValid);
    QTRY_COMPARE(column->height(), 100.0);

    QQuickItemPrivate::get(rect)->setState("");

    // column height and width should stay implicit
    // and column's implicit resizing should still work
    QVERIFY(!QQuickItemPrivate::get(column)->heightValid);
    QVERIFY(!QQuickItemPrivate::get(column)->widthValid);
    QTRY_COMPARE(column->height(), 200.0);

    delete view;
}

// QTBUG-11834
void tst_qquickstates::anchorRewindBug2()
{
    QQmlEngine engine;

    QQmlComponent rectComponent(&engine, testFileUrl("anchorRewindBug2.qml"));
    QQuickRectangle *rect = qobject_cast<QQuickRectangle*>(rectComponent.create());
    QVERIFY(rect != 0);

    QQuickRectangle *mover = rect->findChild<QQuickRectangle*>("mover");

    QVERIFY(mover != 0);
    QCOMPARE(mover->y(), qreal(0.0));
    QCOMPARE(mover->width(), qreal(50.0));

    QQuickItemPrivate::get(rect)->setState("anchored");
    QCOMPARE(mover->y(), qreal(250.0));
    QCOMPARE(mover->width(), qreal(200.0));

    QQuickItemPrivate::get(rect)->setState("");
    QCOMPARE(mover->y(), qreal(0.0));
    QCOMPARE(mover->width(), qreal(50.0));

    delete rect;
}

void tst_qquickstates::script()
{
    QQmlEngine engine;

    {
        QQmlComponent rectComponent(&engine, testFileUrl("script.qml"));
        QQuickRectangle *rect = qobject_cast<QQuickRectangle*>(rectComponent.create());
        QVERIFY(rect != 0);
        QQuickItemPrivate *rectPrivate = QQuickItemPrivate::get(rect);
        QCOMPARE(rect->color(),QColor("red"));

        rectPrivate->setState("blue");
        QCOMPARE(rect->color(),QColor("blue"));

        rectPrivate->setState("");
        QCOMPARE(rect->color(),QColor("blue")); // a script isn't reverted
    }
}

void tst_qquickstates::restoreEntryValues()
{
    QQmlEngine engine;

    QQmlComponent rectComponent(&engine, testFileUrl("restoreEntryValues.qml"));
    QQuickRectangle *rect = qobject_cast<QQuickRectangle*>(rectComponent.create());
    QVERIFY(rect != 0);
    QQuickItemPrivate *rectPrivate = QQuickItemPrivate::get(rect);
    QCOMPARE(rect->color(),QColor("red"));

    rectPrivate->setState("blue");
    QCOMPARE(rect->color(),QColor("blue"));

    rectPrivate->setState("");
    QCOMPARE(rect->color(),QColor("blue"));
}

void tst_qquickstates::explicitChanges()
{
    QQmlEngine engine;

    QQmlComponent rectComponent(&engine, testFileUrl("explicit.qml"));
    QQuickRectangle *rect = qobject_cast<QQuickRectangle*>(rectComponent.create());
    QVERIFY(rect != 0);
    QQuickItemPrivate *rectPrivate = QQuickItemPrivate::get(rect);
    QQmlListReference list(rect, "states");
    QQuickState *state = qobject_cast<QQuickState*>(list.at(0));
    QVERIFY(state != 0);

    qmlExecuteDeferred(state);
    QQuickPropertyChanges *changes = qobject_cast<QQuickPropertyChanges*>(rect->findChild<QQuickPropertyChanges*>("changes"));
    QVERIFY(changes != 0);
    QVERIFY(changes->isExplicit());

    QCOMPARE(rect->color(),QColor("red"));

    rectPrivate->setState("blue");
    QCOMPARE(rect->color(),QColor("blue"));

    rect->setProperty("sourceColor", QColor("green"));
    QCOMPARE(rect->color(),QColor("blue"));

    rectPrivate->setState("");
    QCOMPARE(rect->color(),QColor("red"));
    rect->setProperty("sourceColor", QColor("yellow"));
    QCOMPARE(rect->color(),QColor("red"));

    rectPrivate->setState("blue");
    QCOMPARE(rect->color(),QColor("yellow"));
}

void tst_qquickstates::propertyErrors()
{
    QQmlEngine engine;
    QQmlComponent rectComponent(&engine, testFileUrl("propertyErrors.qml"));
    QQuickRectangle *rect = qobject_cast<QQuickRectangle*>(rectComponent.create());
    QVERIFY(rect != 0);

    QCOMPARE(rect->color(),QColor("red"));

    QTest::ignoreMessage(QtWarningMsg, fullDataPath("propertyErrors.qml") + ":8:9: QML PropertyChanges: Cannot assign to non-existent property \"colr\"");
    QTest::ignoreMessage(QtWarningMsg, fullDataPath("propertyErrors.qml") + ":8:9: QML PropertyChanges: Cannot assign to read-only property \"activeFocus\"");
    QQuickItemPrivate::get(rect)->setState("blue");
}

void tst_qquickstates::incorrectRestoreBug()
{
    QQmlEngine engine;

    QQmlComponent rectComponent(&engine, testFileUrl("basicChanges.qml"));
    QQuickRectangle *rect = qobject_cast<QQuickRectangle*>(rectComponent.create());
    QVERIFY(rect != 0);
    QQuickItemPrivate *rectPrivate = QQuickItemPrivate::get(rect);
    QCOMPARE(rect->color(),QColor("red"));

    rectPrivate->setState("blue");
    QCOMPARE(rect->color(),QColor("blue"));

    rectPrivate->setState("");
    QCOMPARE(rect->color(),QColor("red"));

    // make sure if we change the base state value, we then restore to it correctly
    rect->setColor(QColor("green"));

    rectPrivate->setState("blue");
    QCOMPARE(rect->color(),QColor("blue"));

    rectPrivate->setState("");
    QCOMPARE(rect->color(),QColor("green"));
}

void tst_qquickstates::autoStateAtStartupRestoreBug()
{
    QQmlEngine engine;

    QQmlComponent component(&engine, testFileUrl("autoStateAtStartupRestoreBug.qml"));
    QObject *obj = component.create();

    QVERIFY(obj != 0);
    QCOMPARE(obj->property("test").toInt(), 3);

    obj->setProperty("input", 2);

    QCOMPARE(obj->property("test").toInt(), 9);

    delete obj;
}

void tst_qquickstates::deletingChange()
{
    QQmlEngine engine;

    QQmlComponent rectComponent(&engine, testFileUrl("deleting.qml"));
    QQuickRectangle *rect = qobject_cast<QQuickRectangle*>(rectComponent.create());
    QVERIFY(rect != 0);
    QQuickItemPrivate *rectPrivate = QQuickItemPrivate::get(rect);
    rectPrivate->setState("blue");
    QCOMPARE(rect->color(),QColor("blue"));
    QCOMPARE(rect->radius(),qreal(5));

    rectPrivate->setState("");
    QCOMPARE(rect->color(),QColor("red"));
    QCOMPARE(rect->radius(),qreal(0));

    QQuickPropertyChanges *pc = rect->findChild<QQuickPropertyChanges*>("pc1");
    QVERIFY(pc != 0);
    delete pc;

    QQuickState *state = rect->findChild<QQuickState*>();
    QVERIFY(state != 0);
    qmlExecuteDeferred(state);
    QCOMPARE(state->operationCount(), 1);

    rectPrivate->setState("blue");
    QCOMPARE(rect->color(),QColor("red"));
    QCOMPARE(rect->radius(),qreal(5));

    delete rect;
}

void tst_qquickstates::deletingState()
{
    QQmlEngine engine;

    QQmlComponent rectComponent(&engine, testFileUrl("deletingState.qml"));
    QQuickRectangle *rect = qobject_cast<QQuickRectangle*>(rectComponent.create());
    QVERIFY(rect != 0);

    QQuickStateGroup *sg = rect->findChild<QQuickStateGroup*>();
    QVERIFY(sg != 0);
    QVERIFY(sg->findState("blue") != 0);

    sg->setState("blue");
    QCOMPARE(rect->color(),QColor("blue"));

    sg->setState("");
    QCOMPARE(rect->color(),QColor("red"));

    QQuickState *state = rect->findChild<QQuickState*>();
    QVERIFY(state != 0);
    delete state;

    QVERIFY(!sg->findState("blue"));

    //### should we warn that state doesn't exist
    sg->setState("blue");
    QCOMPARE(rect->color(),QColor("red"));

    delete rect;
}

void tst_qquickstates::tempState()
{
    QQmlEngine engine;

    QQmlComponent rectComponent(&engine, testFileUrl("legalTempState.qml"));
    QQuickRectangle *rect = qobject_cast<QQuickRectangle*>(rectComponent.create());
    QVERIFY(rect != 0);
    QQuickItemPrivate *rectPrivate = QQuickItemPrivate::get(rect);
    QTest::ignoreMessage(QtDebugMsg, "entering placed");
    QTest::ignoreMessage(QtDebugMsg, "entering idle");
    rectPrivate->setState("placed");
    QCOMPARE(rectPrivate->state(), QLatin1String("idle"));
}

void tst_qquickstates::illegalTempState()
{
    QQmlEngine engine;

    QQmlComponent rectComponent(&engine, testFileUrl("illegalTempState.qml"));
    QQuickRectangle *rect = qobject_cast<QQuickRectangle*>(rectComponent.create());
    QVERIFY(rect != 0);
    QQuickItemPrivate *rectPrivate = QQuickItemPrivate::get(rect);
    QTest::ignoreMessage(QtWarningMsg, "<Unknown File>: QML StateGroup: Can't apply a state change as part of a state definition.");
    rectPrivate->setState("placed");
    QCOMPARE(rectPrivate->state(), QLatin1String("placed"));
}

void tst_qquickstates::nonExistantProperty()
{
    QQmlEngine engine;

    QQmlComponent rectComponent(&engine, testFileUrl("nonExistantProp.qml"));
    QQuickRectangle *rect = qobject_cast<QQuickRectangle*>(rectComponent.create());
    QVERIFY(rect != 0);
    QQuickItemPrivate *rectPrivate = QQuickItemPrivate::get(rect);
    QTest::ignoreMessage(QtWarningMsg, fullDataPath("nonExistantProp.qml") + ":9:9: QML PropertyChanges: Cannot assign to non-existent property \"colr\"");
    rectPrivate->setState("blue");
    QCOMPARE(rectPrivate->state(), QLatin1String("blue"));
}

void tst_qquickstates::reset()
{
    QQmlEngine engine;

    QQmlComponent c(&engine, testFileUrl("reset.qml"));
    QQuickRectangle *rect = qobject_cast<QQuickRectangle*>(c.create());
    QVERIFY(rect != 0);

    QQuickImage *image = rect->findChild<QQuickImage*>();
    QVERIFY(image != 0);
    QCOMPARE(image->width(), qreal(40.));
    QCOMPARE(image->height(), qreal(20.));

    QQuickItemPrivate::get(rect)->setState("state1");

    QCOMPARE(image->width(), 20.0);
    QCOMPARE(image->height(), qreal(20.));

    delete rect;
}

void tst_qquickstates::illegalObjectCreation()
{
    QQmlEngine engine;

    QQmlComponent component(&engine, testFileUrl("illegalObj.qml"));
    QList<QQmlError> errors = component.errors();
    QCOMPARE(errors.count(), 1);
    const QQmlError &error = errors.at(0);
    QCOMPARE(error.line(), 9);
    QCOMPARE(error.column(), 23);
    QCOMPARE(error.description().toUtf8().constData(), "PropertyChanges does not support creating state-specific objects.");
}

void tst_qquickstates::whenOrdering()
{
    QQmlEngine engine;

    QQmlComponent c(&engine, testFileUrl("whenOrdering.qml"));
    QQuickRectangle *rect = qobject_cast<QQuickRectangle*>(c.create());
    QVERIFY(rect != 0);
    QQuickItemPrivate *rectPrivate = QQuickItemPrivate::get(rect);

    QCOMPARE(rectPrivate->state(), QLatin1String(""));
    rect->setProperty("condition2", true);
    QCOMPARE(rectPrivate->state(), QLatin1String("state2"));
    rect->setProperty("condition1", true);
    QCOMPARE(rectPrivate->state(), QLatin1String("state1"));
    rect->setProperty("condition2", false);
    QCOMPARE(rectPrivate->state(), QLatin1String("state1"));
    rect->setProperty("condition2", true);
    QCOMPARE(rectPrivate->state(), QLatin1String("state1"));
    rect->setProperty("condition1", false);
    rect->setProperty("condition2", false);
    QCOMPARE(rectPrivate->state(), QLatin1String(""));
}

void tst_qquickstates::urlResolution()
{
    QQmlEngine engine;

    QQmlComponent c(&engine, testFileUrl("urlResolution.qml"));
    QQuickRectangle *rect = qobject_cast<QQuickRectangle*>(c.create());
    QVERIFY(rect != 0);

    QQuickItem *myType = rect->findChild<QQuickItem*>("MyType");
    QQuickImage *image1 = rect->findChild<QQuickImage*>("image1");
    QQuickImage *image2 = rect->findChild<QQuickImage*>("image2");
    QQuickImage *image3 = rect->findChild<QQuickImage*>("image3");
    QVERIFY(myType != 0 && image1 != 0 && image2 != 0 && image3 != 0);

    QQuickItemPrivate::get(myType)->setState("SetImageState");
    QUrl resolved = testFileUrl("Implementation/images/qt-logo.png");
    QCOMPARE(image1->source(), resolved);
    QCOMPARE(image2->source(), resolved);
    QCOMPARE(image3->source(), resolved);

    delete rect;
}

void tst_qquickstates::unnamedWhen()
{
    QQmlEngine engine;

    QQmlComponent c(&engine, testFileUrl("unnamedWhen.qml"));
    QQuickRectangle *rect = qobject_cast<QQuickRectangle*>(c.create());
    QVERIFY(rect != 0);
    QQuickItemPrivate *rectPrivate = QQuickItemPrivate::get(rect);

    QCOMPARE(rectPrivate->state(), QLatin1String(""));
    QCOMPARE(rect->property("stateString").toString(), QLatin1String(""));
    rect->setProperty("triggerState", true);
    QCOMPARE(rectPrivate->state(), QLatin1String("anonymousState1"));
    QCOMPARE(rect->property("stateString").toString(), QLatin1String("inState"));
    rect->setProperty("triggerState", false);
    QCOMPARE(rectPrivate->state(), QLatin1String(""));
    QCOMPARE(rect->property("stateString").toString(), QLatin1String(""));
}

void tst_qquickstates::returnToBase()
{
    QQmlEngine engine;

    QQmlComponent c(&engine, testFileUrl("returnToBase.qml"));
    QQuickRectangle *rect = qobject_cast<QQuickRectangle*>(c.create());
    QVERIFY(rect != 0);
    QQuickItemPrivate *rectPrivate = QQuickItemPrivate::get(rect);

    QCOMPARE(rectPrivate->state(), QLatin1String(""));
    QCOMPARE(rect->property("stateString").toString(), QLatin1String(""));
    rect->setProperty("triggerState", true);
    QCOMPARE(rectPrivate->state(), QLatin1String("anonymousState1"));
    QCOMPARE(rect->property("stateString").toString(), QLatin1String("inState"));
    rect->setProperty("triggerState", false);
    QCOMPARE(rectPrivate->state(), QLatin1String(""));
    QCOMPARE(rect->property("stateString").toString(), QLatin1String("originalState"));
}

//QTBUG-12559
void tst_qquickstates::extendsBug()
{
    QQmlEngine engine;

    QQmlComponent c(&engine, testFileUrl("extendsBug.qml"));
    QQuickRectangle *rect = qobject_cast<QQuickRectangle*>(c.create());
    QVERIFY(rect != 0);
    QQuickItemPrivate *rectPrivate = QQuickItemPrivate::get(rect);
    QQuickRectangle *greenRect = rect->findChild<QQuickRectangle*>("greenRect");

    rectPrivate->setState("b");
    QCOMPARE(greenRect->x(), qreal(100));
    QCOMPARE(greenRect->y(), qreal(100));
}

void tst_qquickstates::editProperties()
{
    QQmlEngine engine;

    QQmlComponent c(&engine, testFileUrl("editProperties.qml"));
    QQuickRectangle *rect = qobject_cast<QQuickRectangle*>(c.create());
    QVERIFY(rect != 0);

    QQuickItemPrivate *rectPrivate = QQuickItemPrivate::get(rect);

    QQuickStateGroup *stateGroup = rectPrivate->_states();
    QVERIFY(stateGroup != 0);
    qmlExecuteDeferred(stateGroup);

    QQuickState *blueState = stateGroup->findState("blue");
    QVERIFY(blueState != 0);
    qmlExecuteDeferred(blueState);

    QQuickPropertyChanges *propertyChangesBlue = qobject_cast<QQuickPropertyChanges*>(blueState->operationAt(0));
    QVERIFY(propertyChangesBlue != 0);

    QQuickState *greenState = stateGroup->findState("green");
    QVERIFY(greenState != 0);
    qmlExecuteDeferred(greenState);

    QQuickPropertyChanges *propertyChangesGreen = qobject_cast<QQuickPropertyChanges*>(greenState->operationAt(0));
    QVERIFY(propertyChangesGreen != 0);

    QQuickRectangle *childRect = rect->findChild<QQuickRectangle*>("rect2");
    QVERIFY(childRect != 0);
    QCOMPARE(childRect->width(), qreal(402));
    QVERIFY(QQmlPropertyPrivate::binding(QQmlProperty(childRect, "width")));
    QCOMPARE(childRect->height(), qreal(200));

    rectPrivate->setState("blue");
    QCOMPARE(childRect->width(), qreal(50));
    QCOMPARE(childRect->height(), qreal(40));
    QVERIFY(!QQmlPropertyPrivate::binding(QQmlProperty(childRect, "width")));
    QVERIFY(blueState->bindingInRevertList(childRect, "width"));


    rectPrivate->setState("green");
    QCOMPARE(childRect->width(), qreal(200));
    QCOMPARE(childRect->height(), qreal(100));
    QVERIFY(greenState->bindingInRevertList(childRect, "width"));


    rectPrivate->setState("");


    QCOMPARE(propertyChangesBlue->actions().length(), 2);
    QVERIFY(propertyChangesBlue->containsValue("width"));
    QVERIFY(!propertyChangesBlue->containsProperty("x"));
    QCOMPARE(propertyChangesBlue->value("width").toInt(), 50);
    QVERIFY(!propertyChangesBlue->value("x").isValid());

    propertyChangesBlue->changeValue("width", 60);
    QCOMPARE(propertyChangesBlue->value("width").toInt(), 60);
    QCOMPARE(propertyChangesBlue->actions().length(), 2);


    propertyChangesBlue->changeExpression("width", "myRectangle.width / 2");
    QVERIFY(!propertyChangesBlue->containsValue("width"));
    QVERIFY(propertyChangesBlue->containsExpression("width"));
    QCOMPARE(propertyChangesBlue->value("width").toInt(), 0);
    QCOMPARE(propertyChangesBlue->actions().length(), 2);

    propertyChangesBlue->changeValue("width", 50);
    QVERIFY(propertyChangesBlue->containsValue("width"));
    QVERIFY(!propertyChangesBlue->containsExpression("width"));
    QCOMPARE(propertyChangesBlue->value("width").toInt(), 50);
    QCOMPARE(propertyChangesBlue->actions().length(), 2);

    QVERIFY(QQmlPropertyPrivate::binding(QQmlProperty(childRect, "width")));
    rectPrivate->setState("blue");
    QCOMPARE(childRect->width(), qreal(50));
    QCOMPARE(childRect->height(), qreal(40));

    propertyChangesBlue->changeValue("width", 60);
    QCOMPARE(propertyChangesBlue->value("width").toInt(), 60);
    QCOMPARE(propertyChangesBlue->actions().length(), 2);
    QCOMPARE(childRect->width(), qreal(60));
    QVERIFY(!QQmlPropertyPrivate::binding(QQmlProperty(childRect, "width")));

    propertyChangesBlue->changeExpression("width", "myRectangle.width / 2");
    QVERIFY(!propertyChangesBlue->containsValue("width"));
    QVERIFY(propertyChangesBlue->containsExpression("width"));
    QCOMPARE(propertyChangesBlue->value("width").toInt(), 0);
    QCOMPARE(propertyChangesBlue->actions().length(), 2);
    QVERIFY(QQmlPropertyPrivate::binding(QQmlProperty(childRect, "width")));
    QCOMPARE(childRect->width(), qreal(200));

    propertyChangesBlue->changeValue("width", 50);
    QCOMPARE(childRect->width(), qreal(50));

    rectPrivate->setState("");
    QCOMPARE(childRect->width(), qreal(402));
    QVERIFY(QQmlPropertyPrivate::binding(QQmlProperty(childRect, "width")));

    QCOMPARE(propertyChangesGreen->actions().length(), 2);
    rectPrivate->setState("green");
    QCOMPARE(childRect->width(), qreal(200));
    QCOMPARE(childRect->height(), qreal(100));
    QVERIFY(QQmlPropertyPrivate::binding(QQmlProperty(childRect, "width")));
    QVERIFY(greenState->bindingInRevertList(childRect, "width"));
    QCOMPARE(propertyChangesGreen->actions().length(), 2);


    propertyChangesGreen->removeProperty("height");
    QVERIFY(!QQmlPropertyPrivate::binding(QQmlProperty(childRect, "height")));
    QCOMPARE(childRect->height(), qreal(200));

    QVERIFY(greenState->bindingInRevertList(childRect, "width"));
    QVERIFY(greenState->containsPropertyInRevertList(childRect, "width"));
    propertyChangesGreen->removeProperty("width");
    QVERIFY(QQmlPropertyPrivate::binding(QQmlProperty(childRect, "width")));
    QCOMPARE(childRect->width(), qreal(402));
    QVERIFY(!greenState->bindingInRevertList(childRect, "width"));
    QVERIFY(!greenState->containsPropertyInRevertList(childRect, "width"));

    propertyChangesBlue->removeProperty("width");
    QCOMPARE(childRect->width(), qreal(402));

    rectPrivate->setState("blue");
    QCOMPARE(childRect->width(), qreal(402));
    QCOMPARE(childRect->height(), qreal(40));
}

void tst_qquickstates::QTBUG_14830()
{
    QQmlEngine engine;

    QQmlComponent c(&engine, testFileUrl("QTBUG-14830.qml"));
    QQuickRectangle *rect = qobject_cast<QQuickRectangle*>(c.create());
    QVERIFY(rect != 0);
    QQuickItem *item = rect->findChild<QQuickItem*>("area");

    QCOMPARE(item->width(), qreal(170));
}

void tst_qquickstates::avoidFastForward()
{
    QQmlEngine engine;

    //shouldn't fast forward if there isn't a transition
    QQmlComponent c(&engine, testFileUrl("avoidFastForward.qml"));
    QQuickRectangle *rect = qobject_cast<QQuickRectangle*>(c.create());
    QVERIFY(rect != 0);

    QQuickItemPrivate *rectPrivate = QQuickItemPrivate::get(rect);
    rectPrivate->setState("a");
    QCOMPARE(rect->property("updateCount").toInt(), 1);
}

//QTBUG-22583
void tst_qquickstates::revertListBug()
{
    QQmlEngine engine;

    QQmlComponent c(&engine, testFileUrl("revertListBug.qml"));
    QQuickRectangle *rect = qobject_cast<QQuickRectangle*>(c.create());
    QVERIFY(rect != 0);

    QQuickRectangle *rect1 = rect->findChild<QQuickRectangle*>("rect1");
    QQuickRectangle *rect2 = rect->findChild<QQuickRectangle*>("rect2");
    QQuickItem *origParent1 = rect->findChild<QQuickItem*>("originalParent1");
    QQuickItem *origParent2 = rect->findChild<QQuickItem*>("originalParent2");
    QQuickItem *newParent = rect->findChild<QQuickItem*>("newParent");

    QCOMPARE(rect1->parentItem(), origParent1);
    QCOMPARE(rect2->parentItem(), origParent2);

    QQuickItemPrivate *rectPrivate = QQuickItemPrivate::get(rect);
    rectPrivate->setState("reparented");

    QCOMPARE(rect1->parentItem(), newParent);
    QCOMPARE(rect2->parentItem(), origParent2);

    rectPrivate->setState("");

    QCOMPARE(rect1->parentItem(), origParent1);
    QCOMPARE(rect2->parentItem(), origParent2);

    QMetaObject::invokeMethod(rect, "switchTargetItem");

    rectPrivate->setState("reparented");

    QCOMPARE(rect1->parentItem(), origParent1);
    QCOMPARE(rect2->parentItem(), newParent);

    rectPrivate->setState("");

    QCOMPARE(rect1->parentItem(), origParent1);
    QCOMPARE(rect2->parentItem(), origParent2); //QTBUG-22583 causes rect2's parent item to be origParent1
}

void tst_qquickstates::QTBUG_38492()
{
    QQmlEngine engine;

    QQmlComponent rectComponent(&engine, testFileUrl("QTBUG-38492.qml"));
    QQuickItem *item = qobject_cast<QQuickItem*>(rectComponent.create());
    QVERIFY(item != 0);

    QQuickItemPrivate::get(item)->setState("apply");

    QCOMPARE(item->property("text").toString(), QString("Test"));

    delete item;
}

void tst_qquickstates::revertListMemoryLeak()
{
    QQmlAbstractBinding::Ptr bindingPtr;
    {
        QQmlEngine engine;

        QQmlComponent c(&engine, testFileUrl("revertListMemoryLeak.qml"));
        QQuickItem *item = qobject_cast<QQuickItem *>(c.create());
        QVERIFY(item);
        QQuickState *state = item->findChild<QQuickState*>("testState");
        QVERIFY(state);

        item->setState("testState");

        QQmlAbstractBinding *binding = state->bindingInRevertList(item, "height");
        QVERIFY(binding);
        bindingPtr = binding;
        QVERIFY(bindingPtr->ref > 1);

        delete item;
    }
    QVERIFY(bindingPtr->ref == 1);
}

QTEST_MAIN(tst_qquickstates)

#include "tst_qquickstates.moc"
