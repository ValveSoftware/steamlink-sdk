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
#include <QSignalSpy>
#include <QGuiApplication>
#include <QtDeclarative/qdeclarativeengine.h>
#include <QtDeclarative/qdeclarativecomponent.h>
#include <QtDeclarative/qdeclarativeview.h>
#include <private/qdeclarativerectangle_p.h>
#include <private/qdeclarativetextedit_p.h>
#include <private/qdeclarativetext_p.h>
#include <QtDeclarative/private/qdeclarativefocusscope_p.h>

class tst_qdeclarativefocusscope : public QObject
{
    Q_OBJECT
public:
    tst_qdeclarativefocusscope() {}

    template<typename T>
    T *findItem(QGraphicsObject *parent, const QString &id);

private slots:
    void basic();
    void nested();
    void noFocus();
    void textEdit();
    void forceFocus();
    void noParentFocus();
    void signalEmission();
    void qtBug13380();
    void forceActiveFocus();
    void notifications();
    void notifications_data();
    void notificationsInScope();
    void notificationsInScope_data();
};

/*
   Find an item with the specified id.
*/
template<typename T>
T *tst_qdeclarativefocusscope::findItem(QGraphicsObject *parent, const QString &objectName)
{
    const QMetaObject &mo = T::staticMetaObject;
    QList<QGraphicsItem *> children = parent->childItems();
    for (int i = 0; i < children.count(); ++i) {
        QDeclarativeItem *item = qobject_cast<QDeclarativeItem *>(children.at(i)->toGraphicsObject());
        if (item) {
            if (mo.cast(item) && (objectName.isEmpty() || item->objectName() == objectName)) {
                return static_cast<T*>(item);
            }
            item = findItem<T>(item, objectName);
            if (item)
                return static_cast<T*>(item);
        }
    }
    return 0;
}

void tst_qdeclarativefocusscope::basic()
{
    QDeclarativeView *view = new QDeclarativeView;
    view->setSource(QUrl::fromLocalFile(SRCDIR "/data/test.qml"));

    QDeclarativeFocusScope *item0 = findItem<QDeclarativeFocusScope>(view->rootObject(), QLatin1String("item0"));
    QDeclarativeRectangle *item1 = findItem<QDeclarativeRectangle>(view->rootObject(), QLatin1String("item1"));
    QDeclarativeRectangle *item2 = findItem<QDeclarativeRectangle>(view->rootObject(), QLatin1String("item2"));
    QDeclarativeRectangle *item3 = findItem<QDeclarativeRectangle>(view->rootObject(), QLatin1String("item3"));
    QVERIFY(item0 != 0);
    QVERIFY(item1 != 0);
    QVERIFY(item2 != 0);
    QVERIFY(item3 != 0);

    view->show();
    qApp->setActiveWindow(view);
    QVERIFY(QTest::qWaitForWindowActive(view));

    QVERIFY(view->hasFocus());
    QVERIFY(view->scene()->hasFocus());
    QVERIFY(item0->hasActiveFocus() == true);
    QVERIFY(item1->hasActiveFocus() == true);
    QVERIFY(item2->hasActiveFocus() == false);
    QVERIFY(item3->hasActiveFocus() == false);

    QTest::keyClick(view, Qt::Key_Right);
    QVERIFY(item0->hasActiveFocus() == true);
    QVERIFY(item1->hasActiveFocus() == false);
    QVERIFY(item2->hasActiveFocus() == true);
    QVERIFY(item3->hasActiveFocus() == false);

    QTest::keyClick(view, Qt::Key_Down);
    QVERIFY(item0->hasActiveFocus() == false);
    QVERIFY(item1->hasActiveFocus() == false);
    QVERIFY(item2->hasActiveFocus() == false);
    QVERIFY(item3->hasActiveFocus() == true);

    delete view;
}

void tst_qdeclarativefocusscope::nested()
{
    QDeclarativeView *view = new QDeclarativeView;
    view->setSource(QUrl::fromLocalFile(SRCDIR "/data/test2.qml"));

    QDeclarativeFocusScope *item1 = findItem<QDeclarativeFocusScope>(view->rootObject(), QLatin1String("item1"));
    QDeclarativeFocusScope *item2 = findItem<QDeclarativeFocusScope>(view->rootObject(), QLatin1String("item2"));
    QDeclarativeFocusScope *item3 = findItem<QDeclarativeFocusScope>(view->rootObject(), QLatin1String("item3"));
    QDeclarativeFocusScope *item4 = findItem<QDeclarativeFocusScope>(view->rootObject(), QLatin1String("item4"));
    QDeclarativeFocusScope *item5 = findItem<QDeclarativeFocusScope>(view->rootObject(), QLatin1String("item5"));
    QVERIFY(item1 != 0);
    QVERIFY(item2 != 0);
    QVERIFY(item3 != 0);
    QVERIFY(item4 != 0);
    QVERIFY(item5 != 0);

    view->show();
    qApp->setActiveWindow(view);
    QVERIFY(QTest::qWaitForWindowActive(view));
    QVERIFY(view->hasFocus());
    QVERIFY(view->scene()->hasFocus());

    QVERIFY(item1->hasActiveFocus() == true);
    QVERIFY(item2->hasActiveFocus() == true);
    QVERIFY(item3->hasActiveFocus() == true);
    QVERIFY(item4->hasActiveFocus() == true);
    QVERIFY(item5->hasActiveFocus() == true);
    delete view;
}

void tst_qdeclarativefocusscope::noFocus()
{
    QDeclarativeView *view = new QDeclarativeView;
    view->setSource(QUrl::fromLocalFile(SRCDIR "/data/test4.qml"));

    QDeclarativeRectangle *item0 = findItem<QDeclarativeRectangle>(view->rootObject(), QLatin1String("item0"));
    QDeclarativeRectangle *item1 = findItem<QDeclarativeRectangle>(view->rootObject(), QLatin1String("item1"));
    QDeclarativeRectangle *item2 = findItem<QDeclarativeRectangle>(view->rootObject(), QLatin1String("item2"));
    QDeclarativeRectangle *item3 = findItem<QDeclarativeRectangle>(view->rootObject(), QLatin1String("item3"));
    QVERIFY(item0 != 0);
    QVERIFY(item1 != 0);
    QVERIFY(item2 != 0);
    QVERIFY(item3 != 0);

    view->show();
    qApp->setActiveWindow(view);
    QVERIFY(QTest::qWaitForWindowActive(view));

    QVERIFY(view->hasFocus());
    QVERIFY(view->scene()->hasFocus());
    QVERIFY(item0->hasActiveFocus() == false);
    QVERIFY(item1->hasActiveFocus() == false);
    QVERIFY(item2->hasActiveFocus() == false);
    QVERIFY(item3->hasActiveFocus() == false);

    QTest::keyClick(view, Qt::Key_Right);
    QVERIFY(item0->hasActiveFocus() == false);
    QVERIFY(item1->hasActiveFocus() == false);
    QVERIFY(item2->hasActiveFocus() == false);
    QVERIFY(item3->hasActiveFocus() == false);

    QTest::keyClick(view, Qt::Key_Down);
    QVERIFY(item0->hasActiveFocus() == false);
    QVERIFY(item1->hasActiveFocus() == false);
    QVERIFY(item2->hasActiveFocus() == false);
    QVERIFY(item3->hasActiveFocus() == false);

    delete view;
}

void tst_qdeclarativefocusscope::textEdit()
{
    QDeclarativeView *view = new QDeclarativeView;
    view->setSource(QUrl::fromLocalFile(SRCDIR "/data/test5.qml"));

    QDeclarativeFocusScope *item0 = findItem<QDeclarativeFocusScope>(view->rootObject(), QLatin1String("item0"));
    QDeclarativeTextEdit *item1 = findItem<QDeclarativeTextEdit>(view->rootObject(), QLatin1String("item1"));
    QDeclarativeRectangle *item2 = findItem<QDeclarativeRectangle>(view->rootObject(), QLatin1String("item2"));
    QDeclarativeTextEdit *item3 = findItem<QDeclarativeTextEdit>(view->rootObject(), QLatin1String("item3"));
    QVERIFY(item0 != 0);
    QVERIFY(item1 != 0);
    QVERIFY(item2 != 0);
    QVERIFY(item3 != 0);

    view->show();
    qApp->setActiveWindow(view);
    QVERIFY(QTest::qWaitForWindowActive(view));

    QVERIFY(view->hasFocus());
    QVERIFY(view->scene()->hasFocus());
    QVERIFY(item0->hasActiveFocus() == true);
    QVERIFY(item1->hasActiveFocus() == true);
    QVERIFY(item2->hasActiveFocus() == false);
    QVERIFY(item3->hasActiveFocus() == false);

    QTest::keyClick(view, Qt::Key_Right);
    QVERIFY(item0->hasActiveFocus() == true);
    QVERIFY(item1->hasActiveFocus() == true);
    QVERIFY(item2->hasActiveFocus() == false);
    QVERIFY(item3->hasActiveFocus() == false);

    QTest::keyClick(view, Qt::Key_Right);
    QTest::keyClick(view, Qt::Key_Right);
    QTest::keyClick(view, Qt::Key_Right);
    QTest::keyClick(view, Qt::Key_Right);
    QTest::keyClick(view, Qt::Key_Right);
    QVERIFY(item0->hasActiveFocus() == true);
    QVERIFY(item1->hasActiveFocus() == false);
    QVERIFY(item2->hasActiveFocus() == true);
    QVERIFY(item3->hasActiveFocus() == false);

    QTest::keyClick(view, Qt::Key_Down);
    QVERIFY(item0->hasActiveFocus() == false);
    QVERIFY(item1->hasActiveFocus() == false);
    QVERIFY(item2->hasActiveFocus() == false);
    QVERIFY(item3->hasActiveFocus() == true);

    delete view;
}

void tst_qdeclarativefocusscope::forceFocus()
{
    QDeclarativeView *view = new QDeclarativeView;
    view->setSource(QUrl::fromLocalFile(SRCDIR "/data/forcefocus.qml"));

    QDeclarativeFocusScope *item0 = findItem<QDeclarativeFocusScope>(view->rootObject(), QLatin1String("item0"));
    QDeclarativeRectangle *item1 = findItem<QDeclarativeRectangle>(view->rootObject(), QLatin1String("item1"));
    QDeclarativeRectangle *item2 = findItem<QDeclarativeRectangle>(view->rootObject(), QLatin1String("item2"));
    QDeclarativeFocusScope *item3 = findItem<QDeclarativeFocusScope>(view->rootObject(), QLatin1String("item3"));
    QDeclarativeRectangle *item4 = findItem<QDeclarativeRectangle>(view->rootObject(), QLatin1String("item4"));
    QDeclarativeRectangle *item5 = findItem<QDeclarativeRectangle>(view->rootObject(), QLatin1String("item5"));
    QVERIFY(item0 != 0);
    QVERIFY(item1 != 0);
    QVERIFY(item2 != 0);
    QVERIFY(item3 != 0);
    QVERIFY(item4 != 0);
    QVERIFY(item5 != 0);

    view->show();
    qApp->setActiveWindow(view);
    QVERIFY(QTest::qWaitForWindowActive(view));

    QVERIFY(view->hasFocus());
    QVERIFY(view->scene()->hasFocus());
    QVERIFY(item0->hasActiveFocus() == true);
    QVERIFY(item1->hasActiveFocus() == true);
    QVERIFY(item2->hasActiveFocus() == false);
    QVERIFY(item3->hasActiveFocus() == false);
    QVERIFY(item4->hasActiveFocus() == false);
    QVERIFY(item5->hasActiveFocus() == false);

    QTest::keyClick(view, Qt::Key_4);
    QVERIFY(item0->hasActiveFocus() == true);
    QVERIFY(item1->hasActiveFocus() == true);
    QVERIFY(item2->hasActiveFocus() == false);
    QVERIFY(item3->hasActiveFocus() == false);
    QVERIFY(item4->hasActiveFocus() == false);
    QVERIFY(item5->hasActiveFocus() == false);

    QTest::keyClick(view, Qt::Key_5);
    QVERIFY(item0->hasActiveFocus() == false);
    QVERIFY(item1->hasActiveFocus() == false);
    QVERIFY(item2->hasActiveFocus() == false);
    QVERIFY(item3->hasActiveFocus() == true);
    QVERIFY(item4->hasActiveFocus() == false);
    QVERIFY(item5->hasActiveFocus() == true);

    delete view;
}

void tst_qdeclarativefocusscope::noParentFocus()
{
    QDeclarativeView *view = new QDeclarativeView;
    view->setSource(QUrl::fromLocalFile(SRCDIR "/data/chain.qml"));
    QVERIFY(view->rootObject());

    QVERIFY(view->rootObject()->property("focus1") == false);
    QVERIFY(view->rootObject()->property("focus2") == false);
    QVERIFY(view->rootObject()->property("focus3") == true);
    QVERIFY(view->rootObject()->property("focus4") == true);
    QVERIFY(view->rootObject()->property("focus5") == true);

    delete view;
}

void tst_qdeclarativefocusscope::signalEmission()
{
    QDeclarativeView *view = new QDeclarativeView;
    view->setSource(QUrl::fromLocalFile(SRCDIR "/data/signalEmission.qml"));

    QDeclarativeRectangle *item1 = findItem<QDeclarativeRectangle>(view->rootObject(), QLatin1String("item1"));
    QDeclarativeRectangle *item2 = findItem<QDeclarativeRectangle>(view->rootObject(), QLatin1String("item2"));
    QDeclarativeRectangle *item3 = findItem<QDeclarativeRectangle>(view->rootObject(), QLatin1String("item3"));
    QDeclarativeRectangle *item4 = findItem<QDeclarativeRectangle>(view->rootObject(), QLatin1String("item4"));
    QVERIFY(item1 != 0);
    QVERIFY(item2 != 0);
    QVERIFY(item3 != 0);
    QVERIFY(item4 != 0);

    view->show();
    qApp->setActiveWindow(view);
    QVERIFY(QTest::qWaitForWindowActive(view));

    QVariant blue(QColor("blue"));
    QVariant red(QColor("red"));

    QVERIFY(view->hasFocus());
    QVERIFY(view->scene()->hasFocus());
    item1->setFocus(true);
    QCOMPARE(item1->property("color"), red);
    QCOMPARE(item2->property("color"), blue);
    QCOMPARE(item3->property("color"), blue);
    QCOMPARE(item4->property("color"), blue);

    item2->setFocus(true);
    QCOMPARE(item1->property("color"), blue);
    QCOMPARE(item2->property("color"), red);
    QCOMPARE(item3->property("color"), blue);
    QCOMPARE(item4->property("color"), blue);

    item3->setFocus(true);
    QCOMPARE(item1->property("color"), blue);
    QCOMPARE(item2->property("color"), red);
    QCOMPARE(item3->property("color"), red);
    QCOMPARE(item4->property("color"), blue);

    item4->setFocus(true);
    QCOMPARE(item1->property("color"), blue);
    QCOMPARE(item2->property("color"), red);
    QCOMPARE(item3->property("color"), blue);
    QCOMPARE(item4->property("color"), red);

    item4->setFocus(false);
    QCOMPARE(item1->property("color"), blue);
    QCOMPARE(item2->property("color"), red);
    QCOMPARE(item3->property("color"), blue);
    QCOMPARE(item4->property("color"), blue);

    delete view;
}

void tst_qdeclarativefocusscope::qtBug13380()
{
    QDeclarativeView *view = new QDeclarativeView;
    view->setSource(QUrl::fromLocalFile(SRCDIR "/data/qtBug13380.qml"));

    view->show();
    QVERIFY(view->rootObject());
    qApp->setActiveWindow(view);
    QVERIFY(QTest::qWaitForWindowActive(view));
    QVERIFY(view->hasFocus());
    QVERIFY(view->scene()->hasFocus());
    QVERIFY(view->rootObject()->property("noFocus").toBool());

    view->rootObject()->setProperty("showRect", true);
    QVERIFY(view->rootObject()->property("noFocus").toBool());

    delete view;
}

void tst_qdeclarativefocusscope::forceActiveFocus()
{
    QDeclarativeView *view = new QDeclarativeView;
    view->setSource(QUrl::fromLocalFile(SRCDIR "/data/forceActiveFocus.qml"));
    view->show();
    view->activateWindow();
    QVERIFY(QTest::qWaitForWindowActive(view));

    QDeclarativeItem *rootObject = qobject_cast<QDeclarativeItem *>(view->rootObject());
    QVERIFY(rootObject);

    QDeclarativeItem *scope = findItem<QDeclarativeItem>(rootObject, QLatin1String("scope"));
    QDeclarativeItem *itemA1 = findItem<QDeclarativeItem>(rootObject, QLatin1String("item-a1"));
    QDeclarativeItem *scopeA = findItem<QDeclarativeItem>(rootObject, QLatin1String("scope-a"));
    QDeclarativeItem *itemA2 = findItem<QDeclarativeItem>(rootObject, QLatin1String("item-a2"));
    QDeclarativeItem *itemB1 = findItem<QDeclarativeItem>(rootObject, QLatin1String("item-b1"));
    QDeclarativeItem *scopeB = findItem<QDeclarativeItem>(rootObject, QLatin1String("scope-b"));
    QDeclarativeItem *itemB2 = findItem<QDeclarativeItem>(rootObject, QLatin1String("item-b2"));

    QVERIFY(scope);
    QVERIFY(itemA1);
    QVERIFY(scopeA);
    QVERIFY(itemA2);
    QVERIFY(itemB1);
    QVERIFY(scopeB);
    QVERIFY(itemB2);

    QCOMPARE(rootObject->hasActiveFocus(), false);
    QCOMPARE(scope->hasActiveFocus(), false);
    QCOMPARE(scopeA->hasActiveFocus(), false);
    QCOMPARE(scopeB->hasActiveFocus(), false);

    QSignalSpy rootSpy(rootObject, SIGNAL(activeFocusChanged(bool)));
    QSignalSpy scopeSpy(scope, SIGNAL(activeFocusChanged(bool)));
    QSignalSpy scopeASpy(scopeA, SIGNAL(activeFocusChanged(bool)));
    QSignalSpy scopeBSpy(scopeB, SIGNAL(activeFocusChanged(bool)));

    // First, walk the focus from item-a1 down to item-a2 and back again
    itemA1->forceActiveFocus();
    QVERIFY(itemA1->hasActiveFocus());
    QCOMPARE(rootSpy.count(), 0);
    QCOMPARE(scopeSpy.count(), 1);

    scopeA->forceActiveFocus();
    QVERIFY(!itemA1->hasActiveFocus());
    QVERIFY(scopeA->hasActiveFocus());
    QCOMPARE(scopeASpy.count(), 1);
    QCOMPARE(rootSpy.count(), 0);
    QCOMPARE(scopeSpy.count(), 1);

    itemA2->forceActiveFocus();
    QVERIFY(!itemA1->hasActiveFocus());
    QVERIFY(itemA2->hasActiveFocus());
    QVERIFY(scopeA->hasActiveFocus());
    if (scopeASpy.count() == 3) {
        qWarning() << "ignoring spurious changed signals";
        QCOMPARE(scopeASpy.takeFirst().first().toBool(), true);
        QCOMPARE(scopeASpy.takeFirst().first().toBool(), false);
        QCOMPARE(scopeASpy.first().first().toBool(), true);
    }
    QCOMPARE(scopeASpy.count(), 1);
    QCOMPARE(rootSpy.count(), 0);
    if (scopeSpy.count() == 3) {
        qWarning() << "ignoring spurious changed signals";
        QCOMPARE(scopeSpy.takeFirst().first().toBool(), true);
        QCOMPARE(scopeSpy.takeFirst().first().toBool(), false);
        QCOMPARE(scopeSpy.first().first().toBool(), true);
    }
    QCOMPARE(scopeSpy.count(), 1);

    scopeA->forceActiveFocus();
    QVERIFY(!itemA1->hasActiveFocus());
    QVERIFY(itemA2->hasActiveFocus());
    QVERIFY(scopeA->hasActiveFocus());
    QCOMPARE(scopeASpy.count(), 1);
    QCOMPARE(rootSpy.count(), 0);
    QCOMPARE(scopeSpy.count(), 1);

    itemA1->forceActiveFocus();
    QVERIFY(itemA1->hasActiveFocus());
    QVERIFY(!scopeA->hasActiveFocus());
    QVERIFY(!itemA2->hasActiveFocus());
    QCOMPARE(scopeASpy.count(), 2);
    QCOMPARE(rootSpy.count(), 0);
    QCOMPARE(scopeSpy.count(), 1);

    // Then jump back and forth between branch 'a' and 'b'
    itemB1->forceActiveFocus();
    QVERIFY(itemB1->hasActiveFocus());
    QCOMPARE(rootSpy.count(), 0);
    QCOMPARE(scopeSpy.count(), 1);

    scopeA->forceActiveFocus();
    QVERIFY(!itemA1->hasActiveFocus());
    QVERIFY(!itemB1->hasActiveFocus());
    QVERIFY(scopeA->hasActiveFocus());
    QCOMPARE(scopeASpy.count(), 3);
    QCOMPARE(rootSpy.count(), 0);
    QCOMPARE(scopeSpy.count(), 1);

    scopeB->forceActiveFocus();
    QVERIFY(!scopeA->hasActiveFocus());
    QVERIFY(!itemB1->hasActiveFocus());
    QVERIFY(scopeB->hasActiveFocus());
    QCOMPARE(scopeASpy.count(), 4);
    QCOMPARE(scopeBSpy.count(), 1);
    QCOMPARE(rootSpy.count(), 0);
    QCOMPARE(scopeSpy.count(), 1);

    itemA2->forceActiveFocus();
    QVERIFY(!scopeB->hasActiveFocus());
    QVERIFY(itemA2->hasActiveFocus());
    QCOMPARE(scopeASpy.count(), 5);
    QCOMPARE(scopeBSpy.count(), 2);
    QCOMPARE(rootSpy.count(), 0);
    QCOMPARE(scopeSpy.count(), 1);

    itemB2->forceActiveFocus();
    QVERIFY(!itemA2->hasActiveFocus());
    QVERIFY(itemB2->hasActiveFocus());
    QCOMPARE(scopeASpy.count(), 6);
    QCOMPARE(scopeBSpy.count(), 3);
    QCOMPARE(rootSpy.count(), 0);
    QCOMPARE(scopeSpy.count(), 1);

    delete view;
}


void tst_qdeclarativefocusscope::notifications_data()
{
    QTest::addColumn<QString>("objectName");

    QTest::newRow("rootItem") << "";
    QTest::newRow("item1") << "item1";
    QTest::newRow("item3") << "item3";
    QTest::newRow("focusScope") << "scope1";
}

void tst_qdeclarativefocusscope::notifications()
{
    QFETCH(QString, objectName);
    QDeclarativeView canvas;
    canvas.setSource(QUrl::fromLocalFile(SRCDIR "/data/notifications.qml"));
    canvas.show();
    canvas.activateWindow();
    QVERIFY(QTest::qWaitForWindowActive(&canvas));

    QGraphicsScene *scene = canvas.scene();

    QDeclarativeItem *item = qobject_cast<QDeclarativeItem*>(canvas.rootObject());
    QVERIFY(item);

    item = objectName.isEmpty() ? item : item->findChild<QDeclarativeItem *>(objectName);
    QVERIFY(item);

    QSignalSpy focusSpy(item, SIGNAL(focusChanged(bool)));
    QSignalSpy activeFocusSpy(item, SIGNAL(activeFocusChanged(bool)));

    QCOMPARE(item->hasFocus(), false);
    QCOMPARE(item->hasActiveFocus(), false);

    item->setFocus(true);

    QCOMPARE(item->hasFocus(), true);
    QCOMPARE(item->hasActiveFocus(), true);
    QCOMPARE(focusSpy.count(), 1);
    QCOMPARE(activeFocusSpy.count(), 1);
    QCOMPARE(focusSpy.takeFirst().first().toBool(), true);
    QCOMPARE(activeFocusSpy.takeFirst().first().toBool(), true);
    QCOMPARE(item->property("handlerFocus").value<bool>(), true);
    QCOMPARE(item->property("handlerActiveFocus").value<bool>(), true);

    item->setFocus(false);

    QCOMPARE(item->hasFocus(), false);
    QCOMPARE(item->hasActiveFocus(), false);
    QCOMPARE(focusSpy.count(), 1);
    QCOMPARE(activeFocusSpy.count(), 1);
    QCOMPARE(focusSpy.takeFirst().first().toBool(), false);
    QCOMPARE(activeFocusSpy.takeFirst().first().toBool(), false);
    QCOMPARE(item->property("handlerFocus").value<bool>(), false);
    QCOMPARE(item->property("handlerActiveFocus").value<bool>(), false);

    item->QGraphicsItem::setFocus();

    QCOMPARE(item->hasFocus(), true);
    QCOMPARE(item->hasActiveFocus(), true);
    QCOMPARE(focusSpy.count(), 1);
    QCOMPARE(activeFocusSpy.count(), 1);
    QCOMPARE(focusSpy.takeFirst().first().toBool(), true);
    QCOMPARE(activeFocusSpy.takeFirst().first().toBool(), true);
    QCOMPARE(item->property("handlerFocus").value<bool>(), true);
    QCOMPARE(item->property("handlerActiveFocus").value<bool>(), true);

    item->clearFocus();

    QCOMPARE(item->hasFocus(), false);
    QCOMPARE(item->hasActiveFocus(), false);
    QCOMPARE(focusSpy.count(), 1);
    QCOMPARE(activeFocusSpy.count(), 1);
    QCOMPARE(focusSpy.takeFirst().first().toBool(), false);
    QCOMPARE(activeFocusSpy.takeFirst().first().toBool(), false);
    QCOMPARE(item->property("handlerFocus").value<bool>(), false);
    QCOMPARE(item->property("handlerActiveFocus").value<bool>(), false);

    scene->setFocusItem(item);

    QCOMPARE(item->hasFocus(), true);
    QCOMPARE(item->hasActiveFocus(), true);
    QCOMPARE(focusSpy.count(), 1);
    QCOMPARE(activeFocusSpy.count(), 1);
    QCOMPARE(focusSpy.takeFirst().first().toBool(), true);
    QCOMPARE(activeFocusSpy.takeFirst().first().toBool(), true);
    QCOMPARE(item->property("handlerFocus").value<bool>(), true);
    QCOMPARE(item->property("handlerActiveFocus").value<bool>(), true);

    scene->setFocusItem(0);

    QCOMPARE(item->hasFocus(), false);
    QCOMPARE(item->hasActiveFocus(), false);
    QCOMPARE(focusSpy.count(), 1);
    QCOMPARE(activeFocusSpy.count(), 1);
    QCOMPARE(focusSpy.takeFirst().first().toBool(), false);
    QCOMPARE(activeFocusSpy.takeFirst().first().toBool(), false);
    QCOMPARE(item->property("handlerFocus").value<bool>(), false);
    QCOMPARE(item->property("handlerActiveFocus").value<bool>(), false);
}

void tst_qdeclarativefocusscope::notificationsInScope_data()
{
    QTest::addColumn<QString>("itemName");
    QTest::addColumn<QString>("scopeName");

    QTest::newRow("item4") << "item4" << "scope1";
    QTest::newRow("item5") << "item5" << "scope2";
}

void tst_qdeclarativefocusscope::notificationsInScope()
{
    QFETCH(QString, itemName);
    QFETCH(QString, scopeName);
    QDeclarativeView canvas;
    canvas.setSource(QUrl::fromLocalFile(SRCDIR "/data/notifications.qml"));
    canvas.show();
    canvas.activateWindow();
    QVERIFY(QTest::qWaitForWindowActive(&canvas));

    QVERIFY(canvas.rootObject());

    QDeclarativeItem *scope = canvas.rootObject()->findChild<QDeclarativeItem *>(scopeName);
    QVERIFY(scope);

    QDeclarativeItem *item = scope->findChild<QDeclarativeItem *>(itemName);
    QVERIFY(item);

    QSignalSpy itemFocusSpy(item, SIGNAL(focusChanged(bool)));
    QSignalSpy itemActiveFocusSpy(item, SIGNAL(activeFocusChanged(bool)));

    QSignalSpy scopeFocusSpy(scope, SIGNAL(focusChanged(bool)));
    QSignalSpy scopeActiveFocusSpy(scope, SIGNAL(activeFocusChanged(bool)));

    QCOMPARE(item->hasFocus(), false);
    QCOMPARE(item->hasActiveFocus(), false);
    QCOMPARE(scope->hasFocus(), false);
    QCOMPARE(scope->hasActiveFocus(), false);
    QCOMPARE(item->property("handlerFocus").value<bool>(), false);
    QCOMPARE(item->property("handlerActiveFocus").value<bool>(), false);
    QCOMPARE(scope->property("handlerFocus").value<bool>(), false);
    QCOMPARE(scope->property("handlerActiveFocus").value<bool>(), false);

    item->setFocus(true);
    QCOMPARE(item->hasFocus(), true);
    QCOMPARE(item->hasActiveFocus(), false);
    QCOMPARE(scope->hasFocus(), false);
    QCOMPARE(scope->hasActiveFocus(), false);
    QCOMPARE(itemFocusSpy.count(), 1);
    QCOMPARE(itemFocusSpy.takeFirst().first().toBool(), true);
    QCOMPARE(itemActiveFocusSpy.count(), 0);
    QCOMPARE(scopeFocusSpy.count(), 0);
    QCOMPARE(scopeActiveFocusSpy.count(), 0);
    QCOMPARE(item->property("handlerFocus").value<bool>(), true);
    QCOMPARE(item->property("handlerActiveFocus").value<bool>(), false);
    QCOMPARE(scope->property("handlerFocus").value<bool>(), false);
    QCOMPARE(scope->property("handlerActiveFocus").value<bool>(), false);

    item->setFocus(false);
    QCOMPARE(item->hasFocus(), false);
    QCOMPARE(item->hasActiveFocus(), false);
    QCOMPARE(scope->hasFocus(), false);
    QCOMPARE(scope->hasActiveFocus(), false);
    QCOMPARE(itemFocusSpy.count(), 1);
    QCOMPARE(itemFocusSpy.takeFirst().first().toBool(), false);
    QCOMPARE(itemActiveFocusSpy.count(), 0);
    QCOMPARE(scopeFocusSpy.count(), 0);
    QCOMPARE(scopeActiveFocusSpy.count(), 0);
    QCOMPARE(item->property("handlerFocus").value<bool>(), false);
    QCOMPARE(item->property("handlerActiveFocus").value<bool>(), false);
    QCOMPARE(scope->property("handlerFocus").value<bool>(), false);
    QCOMPARE(scope->property("handlerActiveFocus").value<bool>(), false);

    item->forceActiveFocus();
    QCOMPARE(item->hasFocus(), true);
    QCOMPARE(item->hasActiveFocus(), true);
    QCOMPARE(scope->hasFocus(), true);
    QCOMPARE(scope->hasActiveFocus(), true);
    QCOMPARE(itemFocusSpy.count(), 1);
    QCOMPARE(itemFocusSpy.takeFirst().first().toBool(), true);
    QCOMPARE(itemActiveFocusSpy.count(), 1);
    QCOMPARE(itemActiveFocusSpy.takeFirst().first().toBool(), true);
    QCOMPARE(scopeFocusSpy.count(), 1);
    QCOMPARE(scopeFocusSpy.takeFirst().first().toBool(), true);
    QCOMPARE(scopeActiveFocusSpy.count(), 1);
    QCOMPARE(scopeActiveFocusSpy.takeFirst().first().toBool(), true);
    QCOMPARE(item->property("handlerFocus").value<bool>(), true);
    QCOMPARE(item->property("handlerActiveFocus").value<bool>(), true);
    QCOMPARE(scope->property("handlerFocus").value<bool>(), true);
    QCOMPARE(scope->property("handlerActiveFocus").value<bool>(), true);

    scope->setFocus(false);
    QCOMPARE(item->hasFocus(), true);
    QCOMPARE(item->hasActiveFocus(), false);
    QCOMPARE(scope->hasFocus(), false);
    QCOMPARE(scope->hasActiveFocus(), false);
    QCOMPARE(itemFocusSpy.count(), 0);
    QCOMPARE(itemActiveFocusSpy.count(), 1);
    QCOMPARE(itemActiveFocusSpy.takeFirst().first().toBool(), false);
    QCOMPARE(scopeFocusSpy.count(), 1);
    QCOMPARE(scopeFocusSpy.takeFirst().first().toBool(), false);
    QCOMPARE(scopeActiveFocusSpy.count(), 1);
    QCOMPARE(scopeActiveFocusSpy.takeFirst().first().toBool(), false);
    QCOMPARE(item->property("handlerFocus").value<bool>(), true);
    QCOMPARE(item->property("handlerActiveFocus").value<bool>(), false);
    QCOMPARE(scope->property("handlerFocus").value<bool>(), false);
    QCOMPARE(scope->property("handlerActiveFocus").value<bool>(), false);

    scope->setFocus(true);
    QCOMPARE(item->hasFocus(), true);
    QCOMPARE(item->hasActiveFocus(), true);
    QCOMPARE(scope->hasFocus(), true);
    QCOMPARE(scope->hasActiveFocus(), true);
    QCOMPARE(itemFocusSpy.count(), 0);
    QCOMPARE(itemActiveFocusSpy.count(), 1);
    QCOMPARE(itemActiveFocusSpy.takeFirst().first().toBool(), true);
    QCOMPARE(scopeFocusSpy.count(), 1);
    QCOMPARE(scopeFocusSpy.takeFirst().first().toBool(), true);
    QCOMPARE(scopeActiveFocusSpy.count(), 1);
    QCOMPARE(scopeActiveFocusSpy.takeFirst().first().toBool(), true);
    QCOMPARE(item->property("handlerFocus").value<bool>(), true);
    QCOMPARE(item->property("handlerActiveFocus").value<bool>(), true);
    QCOMPARE(scope->property("handlerFocus").value<bool>(), true);
    QCOMPARE(scope->property("handlerActiveFocus").value<bool>(), true);

    item->setFocus(false);
    QCOMPARE(item->hasFocus(), false);
    QCOMPARE(item->hasActiveFocus(), false);
    QCOMPARE(scope->hasFocus(), true);
    QCOMPARE(scope->hasActiveFocus(), true);
    QCOMPARE(itemFocusSpy.count(), 1);
    QCOMPARE(itemFocusSpy.takeFirst().first().toBool(), false);
    QCOMPARE(itemActiveFocusSpy.count(), 1);
    QCOMPARE(itemActiveFocusSpy.takeFirst().first().toBool(), false);
    if (scopeFocusSpy.count() == 2) {
        qWarning() << "ignoring spurious changed signals";
        QCOMPARE(scopeFocusSpy.takeFirst().first().toBool(), false);
        QCOMPARE(scopeFocusSpy.takeFirst().first().toBool(), true);
    }
    QCOMPARE(scopeFocusSpy.count(), 0);
    QCOMPARE(scopeActiveFocusSpy.count(), 0);
    QCOMPARE(item->property("handlerFocus").value<bool>(), false);
    QCOMPARE(item->property("handlerActiveFocus").value<bool>(), false);
    QCOMPARE(scope->property("handlerFocus").value<bool>(), true);
    QCOMPARE(scope->property("handlerActiveFocus").value<bool>(), true);
}

QTEST_MAIN(tst_qdeclarativefocusscope)

#include "tst_qdeclarativefocusscope.moc"
