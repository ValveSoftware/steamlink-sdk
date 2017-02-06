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

#include <QtQuick/qquickitem.h>
#include <QtQuick/qquickwindow.h>
#include <QtQuick/qquickview.h>
#include "private/qquickfocusscope_p.h"
#include "private/qquickitem_p.h"
#include <qpa/qwindowsysteminterface.h>
#include <QDebug>
#include <QTimer>
#include <QQmlEngine>
#include "../../shared/util.h"
#include "../shared/viewtestutil.h"
#include <QSignalSpy>

class TestItem : public QQuickItem
{
Q_OBJECT
public:
    TestItem(QQuickItem *parent = 0)
        : QQuickItem(parent), focused(false), pressCount(0), releaseCount(0)
        , wheelCount(0), acceptIncomingTouchEvents(true)
        , touchEventReached(false), timestamp(0) {}

    bool focused;
    int pressCount;
    int releaseCount;
    int wheelCount;
    bool acceptIncomingTouchEvents;
    bool touchEventReached;
    ulong timestamp;
protected:
    virtual void focusInEvent(QFocusEvent *) { Q_ASSERT(!focused); focused = true; }
    virtual void focusOutEvent(QFocusEvent *) { Q_ASSERT(focused); focused = false; }
    virtual void mousePressEvent(QMouseEvent *event) { event->accept(); ++pressCount; }
    virtual void mouseReleaseEvent(QMouseEvent *event) { event->accept(); ++releaseCount; }
    virtual void touchEvent(QTouchEvent *event) {
        touchEventReached = true;
        event->setAccepted(acceptIncomingTouchEvents);
    }
    virtual void wheelEvent(QWheelEvent *event) { event->accept(); ++wheelCount; timestamp = event->timestamp(); }
};

class TestWindow: public QQuickWindow
{
public:
    TestWindow()
        : QQuickWindow()
    {}

    virtual bool event(QEvent *event)
    {
        return QQuickWindow::event(event);
    }
};

class TestPolishItem : public QQuickItem
{
Q_OBJECT
public:
    TestPolishItem(QQuickItem *parent = 0)
    : QQuickItem(parent), wasPolished(false) {

    }

    bool wasPolished;

protected:
    virtual void updatePolish() {
        wasPolished = true;
    }

public slots:
    void doPolish() {
        polish();
    }
};

class TestFocusScope : public QQuickFocusScope
{
Q_OBJECT
public:
    TestFocusScope(QQuickItem *parent = 0) : QQuickFocusScope(parent), focused(false) {}

    bool focused;
protected:
    virtual void focusInEvent(QFocusEvent *) { Q_ASSERT(!focused); focused = true; }
    virtual void focusOutEvent(QFocusEvent *) { Q_ASSERT(focused); focused = false; }
};

class tst_qquickitem : public QQmlDataTest
{
    Q_OBJECT
public:

private slots:
    void initTestCase();

    void noWindow();
    void simpleFocus();
    void scopedFocus();
    void addedToWindow();
    void changeParent();
    void multipleFocusClears();
    void focusSubItemInNonFocusScope();
    void parentItemWithFocus();
    void reparentFocusedItem();

    void constructor();
    void setParentItem();

    void visible();
    void enabled();
    void enabledFocus();

    void mouseGrab();
    void touchEventAcceptIgnore_data();
    void touchEventAcceptIgnore();
    void polishOutsideAnimation();
    void polishOnCompleted();

    void wheelEvent_data();
    void wheelEvent();
    void hoverEvent_data();
    void hoverEvent();
    void hoverEventInParent();

    void paintOrder_data();
    void paintOrder();

    void acceptedMouseButtons();

    void visualParentOwnership();
    void visualParentOwnershipWindow();

    void testSGInvalidate();

    void objectChildTransform();

    void contains_data();
    void contains();

    void childAt();

    void ignoreButtonPressNotInAcceptedMouseButtons();

private:

    enum PaintOrderOp {
        NoOp, Append, Remove, StackBefore, StackAfter, SetZ
    };

    void ensureFocus(QWindow *w) {
        if (w->width() <=0 || w->height() <= 0)
            w->setGeometry(100, 100, 400, 300);
        w->show();
        w->requestActivate();
        QTest::qWaitForWindowActive(w);
    }
};

void tst_qquickitem::initTestCase()
{
    QQmlDataTest::initTestCase();
    qmlRegisterType<TestPolishItem>("Qt.test", 1, 0, "TestPolishItem");
}

// Focus still updates when outside a window
void tst_qquickitem::noWindow()
{
    QQuickItem *root = new TestItem;
    QQuickItem *child = new TestItem(root);
    QQuickItem *scope = new TestItem(root);
    QQuickFocusScope *scopedChild = new TestFocusScope(scope);
    QQuickFocusScope *scopedChild2 = new TestFocusScope(scope);

    QCOMPARE(root->hasFocus(), false);
    QCOMPARE(child->hasFocus(), false);
    QCOMPARE(scope->hasFocus(), false);
    QCOMPARE(scopedChild->hasFocus(), false);
    QCOMPARE(scopedChild2->hasFocus(), false);

    root->setFocus(true);
    scope->setFocus(true);
    scopedChild2->setFocus(true);
    QCOMPARE(root->hasFocus(), false);
    QCOMPARE(child->hasFocus(), false);
    QCOMPARE(scope->hasFocus(), false);
    QCOMPARE(scopedChild->hasFocus(), false);
    QCOMPARE(scopedChild2->hasFocus(), true);

    root->setFocus(false);
    child->setFocus(true);
    scopedChild->setFocus(true);
    scope->setFocus(false);
    QCOMPARE(root->hasFocus(), false);
    QCOMPARE(child->hasFocus(), false);
    QCOMPARE(scope->hasFocus(), false);
    QCOMPARE(scopedChild->hasFocus(), true);
    QCOMPARE(scopedChild2->hasFocus(), false);

    delete root;
}

struct FocusData {
    FocusData() : focus(false), activeFocus(false) {}

    void set(bool f, bool af) { focus = f; activeFocus = af; }
    bool focus;
    bool activeFocus;
};
struct FocusState : public QHash<QQuickItem *, FocusData>
{
    FocusState() : activeFocusItem(0) {}
    FocusState &operator<<(QQuickItem *item) {
        insert(item, FocusData());
        return *this;
    }

    void active(QQuickItem *i) {
        activeFocusItem = i;
    }
    QQuickItem *activeFocusItem;
};

#define FVERIFY() \
    do { \
        if (focusState.activeFocusItem) { \
            QCOMPARE(window.activeFocusItem(), focusState.activeFocusItem); \
            if (qobject_cast<TestItem *>(window.activeFocusItem())) \
                QCOMPARE(qobject_cast<TestItem *>(window.activeFocusItem())->focused, true); \
            else if (qobject_cast<TestFocusScope *>(window.activeFocusItem())) \
                QCOMPARE(qobject_cast<TestFocusScope *>(window.activeFocusItem())->focused, true); \
        } else { \
            QCOMPARE(window.activeFocusItem(), window.contentItem()); \
        } \
        for (QHash<QQuickItem *, FocusData>::Iterator iter = focusState.begin(); \
            iter != focusState.end(); \
            iter++) { \
            QCOMPARE(iter.key()->hasFocus(), iter.value().focus); \
            QCOMPARE(iter.key()->hasActiveFocus(), iter.value().activeFocus); \
        } \
    } while (false)

// Tests a simple set of top-level scoped items
void tst_qquickitem::simpleFocus()
{
    QQuickWindow window;
    ensureFocus(&window);

    QTRY_COMPARE(QGuiApplication::focusWindow(), &window);

    QQuickItem *l1c1 = new TestItem(window.contentItem());
    QQuickItem *l1c2 = new TestItem(window.contentItem());
    QQuickItem *l1c3 = new TestItem(window.contentItem());

    QQuickItem *l2c1 = new TestItem(l1c1);
    QQuickItem *l2c2 = new TestItem(l1c1);
    QQuickItem *l2c3 = new TestItem(l1c3);

    FocusState focusState;
    focusState << l1c1 << l1c2 << l1c3
               << l2c1 << l2c2 << l2c3;
    FVERIFY();

    l1c1->setFocus(true);
    focusState[l1c1].set(true, true);
    focusState.active(l1c1);
    FVERIFY();

    l2c3->setFocus(true);
    focusState[l1c1].set(false, false);
    focusState[l2c3].set(true, true);
    focusState.active(l2c3);
    FVERIFY();

    l1c3->setFocus(true);
    focusState[l2c3].set(false, false);
    focusState[l1c3].set(true, true);
    focusState.active(l1c3);
    FVERIFY();

    l1c2->setFocus(false);
    FVERIFY();

    l1c3->setFocus(false);
    focusState[l1c3].set(false, false);
    focusState.active(0);
    FVERIFY();

    l2c1->setFocus(true);
    focusState[l2c1].set(true, true);
    focusState.active(l2c1);
    FVERIFY();
}

// Items with a focus scope
void tst_qquickitem::scopedFocus()
{
    QQuickWindow window;
    ensureFocus(&window);
    QTRY_COMPARE(QGuiApplication::focusWindow(), &window);

    QQuickItem *l1c1 = new TestItem(window.contentItem());
    QQuickItem *l1c2 = new TestItem(window.contentItem());
    QQuickItem *l1c3 = new TestItem(window.contentItem());

    QQuickItem *l2c1 = new TestItem(l1c1);
    QQuickItem *l2c2 = new TestItem(l1c1);
    QQuickItem *l2c3 = new TestFocusScope(l1c3);

    QQuickItem *l3c1 = new TestItem(l2c3);
    QQuickItem *l3c2 = new TestFocusScope(l2c3);

    QQuickItem *l4c1 = new TestItem(l3c2);
    QQuickItem *l4c2 = new TestItem(l3c2);

    FocusState focusState;
    focusState << l1c1 << l1c2 << l1c3
               << l2c1 << l2c2 << l2c3
               << l3c1 << l3c2
               << l4c1 << l4c2;
    FVERIFY();

    l4c2->setFocus(true);
    focusState[l4c2].set(true, false);
    FVERIFY();

    l4c1->setFocus(true);
    focusState[l4c2].set(false, false);
    focusState[l4c1].set(true, false);
    FVERIFY();

    l1c1->setFocus(true);
    focusState[l1c1].set(true, true);
    focusState.active(l1c1);
    FVERIFY();

    l3c2->setFocus(true);
    focusState[l3c2].set(true, false);
    FVERIFY();

    l2c3->setFocus(true);
    focusState[l1c1].set(false, false);
    focusState[l2c3].set(true, true);
    focusState[l3c2].set(true, true);
    focusState[l4c1].set(true, true);
    focusState.active(l4c1);
    FVERIFY();

    l3c2->setFocus(false);
    focusState[l3c2].set(false, false);
    focusState[l4c1].set(true, false);
    focusState.active(l2c3);
    FVERIFY();

    l3c2->setFocus(true);
    focusState[l3c2].set(true, true);
    focusState[l4c1].set(true, true);
    focusState.active(l4c1);
    FVERIFY();

    l4c1->setFocus(false);
    focusState[l4c1].set(false, false);
    focusState.active(l3c2);
    FVERIFY();

    l1c3->setFocus(true);
    focusState[l1c3].set(true, true);
    focusState[l2c3].set(false, false);
    focusState[l3c2].set(true, false);
    focusState.active(l1c3);
    FVERIFY();
}

// Tests focus corrects itself when a tree is added to a window for the first time
void tst_qquickitem::addedToWindow()
{
    {
    QQuickWindow window;
    ensureFocus(&window);
    QTRY_COMPARE(QGuiApplication::focusWindow(), &window);

    QQuickItem *item = new TestItem;

    FocusState focusState;
    focusState << item;

    item->setFocus(true);
    focusState[item].set(true, false);
    FVERIFY();

    item->setParentItem(window.contentItem());
    focusState[item].set(true, true);
    focusState.active(item);
    FVERIFY();
    }

    {
    QQuickWindow window;
    ensureFocus(&window);
    QTRY_COMPARE(QGuiApplication::focusWindow(), &window);

    QQuickItem *item = new TestItem(window.contentItem());

    QQuickItem *tree = new TestItem;
    QQuickItem *c1 = new TestItem(tree);
    QQuickItem *c2 = new TestItem(tree);

    FocusState focusState;
    focusState << item << tree << c1 << c2;

    item->setFocus(true);
    c1->setFocus(true);
    c2->setFocus(true);
    focusState[item].set(true, true);
    focusState[c1].set(false, false);
    focusState[c2].set(true, false);
    focusState.active(item);
    FVERIFY();

    tree->setParentItem(item);
    focusState[c1].set(false, false);
    focusState[c2].set(false, false);
    FVERIFY();
    }

    {
    QQuickWindow window;
    ensureFocus(&window);
    QTRY_COMPARE(QGuiApplication::focusWindow(), &window);

    QQuickItem *tree = new TestItem;
    QQuickItem *c1 = new TestItem(tree);
    QQuickItem *c2 = new TestItem(tree);

    FocusState focusState;
    focusState << tree << c1 << c2;
    c1->setFocus(true);
    c2->setFocus(true);
    focusState[c1].set(false, false);
    focusState[c2].set(true, false);
    FVERIFY();

    tree->setParentItem(window.contentItem());
    focusState[c1].set(false, false);
    focusState[c2].set(true, true);
    focusState.active(c2);
    FVERIFY();
    }

    {
    QQuickWindow window;
    ensureFocus(&window);
    QTRY_COMPARE(QGuiApplication::focusWindow(), &window);
    QQuickItem *tree = new TestFocusScope;
    QQuickItem *c1 = new TestItem(tree);
    QQuickItem *c2 = new TestItem(tree);

    FocusState focusState;
    focusState << tree << c1 << c2;
    c1->setFocus(true);
    c2->setFocus(true);
    focusState[c1].set(false, false);
    focusState[c2].set(true, false);
    FVERIFY();

    tree->setParentItem(window.contentItem());
    focusState[c1].set(false, false);
    focusState[c2].set(true, false);
    FVERIFY();

    tree->setFocus(true);
    focusState[tree].set(true, true);
    focusState[c2].set(true, true);
    focusState.active(c2);
    FVERIFY();
    }

    {
    QQuickWindow window;
    ensureFocus(&window);
    QTRY_COMPARE(QGuiApplication::focusWindow(), &window);
    QQuickItem *tree = new TestFocusScope;
    QQuickItem *c1 = new TestItem(tree);
    QQuickItem *c2 = new TestItem(tree);

    FocusState focusState;
    focusState << tree << c1 << c2;
    tree->setFocus(true);
    c1->setFocus(true);
    c2->setFocus(true);
    focusState[tree].set(true, false);
    focusState[c1].set(false, false);
    focusState[c2].set(true, false);
    FVERIFY();

    tree->setParentItem(window.contentItem());
    focusState[tree].set(true, true);
    focusState[c1].set(false, false);
    focusState[c2].set(true, true);
    focusState.active(c2);
    FVERIFY();
    }

    {
    QQuickWindow window;
    ensureFocus(&window);
    QTRY_COMPARE(QGuiApplication::focusWindow(), &window);
    QQuickItem *child = new TestItem(window.contentItem());
    QQuickItem *tree = new TestFocusScope;
    QQuickItem *c1 = new TestItem(tree);
    QQuickItem *c2 = new TestItem(tree);

    FocusState focusState;
    focusState << child << tree << c1 << c2;
    child->setFocus(true);
    tree->setFocus(true);
    c1->setFocus(true);
    c2->setFocus(true);
    focusState[child].set(true, true);
    focusState[tree].set(true, false);
    focusState[c1].set(false, false);
    focusState[c2].set(true, false);
    focusState.active(child);
    FVERIFY();

    tree->setParentItem(window.contentItem());
    focusState[tree].set(false, false);
    focusState[c1].set(false, false);
    focusState[c2].set(true, false);
    FVERIFY();

    tree->setFocus(true);
    focusState[child].set(false, false);
    focusState[tree].set(true, true);
    focusState[c2].set(true, true);
    focusState.active(c2);
    FVERIFY();
    }
}

void tst_qquickitem::changeParent()
{
    // Parent to no parent
    {
    QQuickWindow window;
    ensureFocus(&window);
    QTRY_COMPARE(QGuiApplication::focusWindow(), &window);
    QQuickItem *child = new TestItem(window.contentItem());

    FocusState focusState;
    focusState << child;
    FVERIFY();

    child->setFocus(true);
    focusState[child].set(true, true);
    focusState.active(child);
    FVERIFY();

    child->setParentItem(0);
    focusState[child].set(true, false);
    focusState.active(0);
    FVERIFY();
    }

    // Different parent, same focus scope
    {
    QQuickWindow window;
    ensureFocus(&window);
    QTRY_COMPARE(QGuiApplication::focusWindow(), &window);
    QQuickItem *child = new TestItem(window.contentItem());
    QQuickItem *child2 = new TestItem(window.contentItem());

    FocusState focusState;
    focusState << child << child2;
    FVERIFY();

    child->setFocus(true);
    focusState[child].set(true, true);
    focusState.active(child);
    FVERIFY();

    child->setParentItem(child2);
    FVERIFY();
    }

    // Different parent, different focus scope
    {
    QQuickWindow window;
    ensureFocus(&window);
    QTRY_COMPARE(QGuiApplication::focusWindow(), &window);
    QQuickItem *child = new TestItem(window.contentItem());
    QQuickItem *child2 = new TestFocusScope(window.contentItem());
    QQuickItem *item = new TestItem(child);

    FocusState focusState;
    focusState << child << child2 << item;
    FVERIFY();

    item->setFocus(true);
    focusState[item].set(true, true);
    focusState.active(item);
    FVERIFY();

    item->setParentItem(child2);
    focusState[item].set(true, false);
    focusState.active(0);
    FVERIFY();
    }
    {
    QQuickWindow window;
    ensureFocus(&window);
    QTRY_COMPARE(QGuiApplication::focusWindow(), &window);
    QQuickItem *child = new TestItem(window.contentItem());
    QQuickItem *child2 = new TestFocusScope(window.contentItem());
    QQuickItem *item = new TestItem(child2);

    FocusState focusState;
    focusState << child << child2 << item;
    FVERIFY();

    item->setFocus(true);
    focusState[item].set(true, false);
    focusState.active(0);
    FVERIFY();

    item->setParentItem(child);
    focusState[item].set(true, true);
    focusState.active(item);
    FVERIFY();
    }
    {
    QQuickWindow window;
    ensureFocus(&window);
    QTRY_COMPARE(QGuiApplication::focusWindow(), &window);
    QQuickItem *child = new TestItem(window.contentItem());
    QQuickItem *child2 = new TestFocusScope(window.contentItem());
    QQuickItem *item = new TestItem(child2);

    FocusState focusState;
    focusState << child << child2 << item;
    FVERIFY();

    child->setFocus(true);
    item->setFocus(true);
    focusState[child].set(true, true);
    focusState[item].set(true, false);
    focusState.active(child);
    FVERIFY();

    item->setParentItem(child);
    focusState[item].set(false, false);
    FVERIFY();
    }

    // child has active focus, then its fs parent changes parent to 0, then
    // child is deleted, then its parent changes again to a valid parent
    {
    QQuickWindow window;
    ensureFocus(&window);
    QTRY_COMPARE(QGuiApplication::focusWindow(), &window);
    QQuickItem *item = new TestFocusScope(window.contentItem());
    QQuickItem *child = new TestItem(item);
    QQuickItem *child2 = new TestItem;

    FocusState focusState;
    focusState << item << child;
    FVERIFY();

    item->setFocus(true);
    child->setFocus(true);
    focusState[child].set(true, true);
    focusState[item].set(true, true);
    focusState.active(child);
    FVERIFY();

    item->setParentItem(0);
    focusState[child].set(true, false);
    focusState[item].set(true, false);
    focusState.active(0);
    FVERIFY();

    focusState.remove(child);
    delete child;
    item->setParentItem(window.contentItem());
    focusState[item].set(true, true);
    focusState.active(item);
    FVERIFY();
    delete child2;
    }
}

void tst_qquickitem::multipleFocusClears()
{
    //Multiple clears of focus inside a focus scope shouldn't crash. QTBUG-24714
    QQuickView view;
    view.setSource(testFileUrl("multipleFocusClears.qml"));
    view.show();
    ensureFocus(&view);
    QTRY_COMPARE(QGuiApplication::focusWindow(), &view);
}

void tst_qquickitem::focusSubItemInNonFocusScope()
{
    QQuickView view;
    view.setSource(testFileUrl("focusSubItemInNonFocusScope.qml"));
    view.show();
    QTest::qWaitForWindowActive(&view);

    QQuickItem *dummyItem = view.rootObject()->findChild<QQuickItem *>("dummyItem");
    QVERIFY(dummyItem);

    QQuickItem *textInput = view.rootObject()->findChild<QQuickItem *>("textInput");
    QVERIFY(textInput);

    QVERIFY(dummyItem->hasFocus());
    QVERIFY(!textInput->hasFocus());
    QVERIFY(dummyItem->hasActiveFocus());

    QVERIFY(QMetaObject::invokeMethod(textInput, "forceActiveFocus"));

    QVERIFY(!dummyItem->hasFocus());
    QVERIFY(textInput->hasFocus());
    QVERIFY(textInput->hasActiveFocus());
}

void tst_qquickitem::parentItemWithFocus()
{
    QQuickWindow window;
    ensureFocus(&window);
    QTRY_COMPARE(QGuiApplication::focusWindow(), &window);
    {
    QQuickItem parent;
    QQuickItem child;

    FocusState focusState;
    focusState << &parent << &child;
    FVERIFY();

    parent.setFocus(true);
    child.setFocus(true);
    focusState[&parent].set(true, false);
    focusState[&child].set(true, false);
    FVERIFY();

    child.setParentItem(&parent);
    focusState[&parent].set(true, false);
    focusState[&child].set(false, false);
    FVERIFY();

    parent.setParentItem(window.contentItem());
    focusState[&parent].set(true, true);
    focusState[&child].set(false, false);
    focusState.active(&parent);
    FVERIFY();

    child.forceActiveFocus();
    focusState[&parent].set(false, false);
    focusState[&child].set(true, true);
    focusState.active(&child);
    FVERIFY();
    } {
    QQuickItem parent;
    QQuickItem child;
    QQuickItem grandchild(&child);

    FocusState focusState;
    focusState << &parent << &child << &grandchild;
    FVERIFY();

    parent.setFocus(true);
    grandchild.setFocus(true);
    focusState[&parent].set(true, false);
    focusState[&child].set(false, false);
    focusState[&grandchild].set(true, false);
    FVERIFY();

    child.setParentItem(&parent);
    focusState[&parent].set(true, false);
    focusState[&child].set(false, false);
    focusState[&grandchild].set(false, false);
    FVERIFY();

    parent.setParentItem(window.contentItem());
    focusState[&parent].set(true, true);
    focusState[&child].set(false, false);
    focusState[&grandchild].set(false, false);
    focusState.active(&parent);
    FVERIFY();

    grandchild.forceActiveFocus();
    focusState[&parent].set(false, false);
    focusState[&child].set(false, false);
    focusState[&grandchild].set(true, true);
    focusState.active(&grandchild);
    FVERIFY();
    }

    {
    QQuickItem parent;
    QQuickItem child1;
    QQuickItem child2;

    FocusState focusState;
    focusState << &parent << &child1 << &child2;
    parent.setFocus(true);
    child1.setParentItem(&parent);
    child2.setParentItem(&parent);
    focusState[&parent].set(true, false);
    focusState[&child1].set(false, false);
    focusState[&child2].set(false, false);
    FVERIFY();

    child1.setFocus(true);
    focusState[&parent].set(false, false);
    focusState[&child1].set(true, false);
    FVERIFY();

    parent.setFocus(true);
    focusState[&parent].set(true, false);
    focusState[&child1].set(false, false);
    FVERIFY();
    }
}

void tst_qquickitem::reparentFocusedItem()
{
    QQuickWindow window;
    ensureFocus(&window);
    QTRY_COMPARE(QGuiApplication::focusWindow(), &window);

    QQuickItem parent(window.contentItem());
    QQuickItem child(&parent);
    QQuickItem sibling(&parent);
    QQuickItem grandchild(&child);

    FocusState focusState;
    focusState << &parent << &child << &sibling << &grandchild;
    FVERIFY();

    grandchild.setFocus(true);
    focusState[&parent].set(false, false);
    focusState[&child].set(false, false);
    focusState[&sibling].set(false, false);
    focusState[&grandchild].set(true, true);
    focusState.active(&grandchild);
    FVERIFY();

    // Parenting the item to another item within the same focus scope shouldn't change it's focus.
    child.setParentItem(&sibling);
    FVERIFY();
}

void tst_qquickitem::constructor()
{
    QScopedPointer<QQuickItem> root(new QQuickItem);
    QVERIFY(!root->parent());
    QVERIFY(!root->parentItem());

    QQuickItem *child1 = new QQuickItem(root.data());
    QCOMPARE(child1->parent(), root.data());
    QCOMPARE(child1->parentItem(), root.data());
    QCOMPARE(root->childItems().count(), 1);
    QCOMPARE(root->childItems().at(0), child1);

    QQuickItem *child2 = new QQuickItem(root.data());
    QCOMPARE(child2->parent(), root.data());
    QCOMPARE(child2->parentItem(), root.data());
    QCOMPARE(root->childItems().count(), 2);
    QCOMPARE(root->childItems().at(0), child1);
    QCOMPARE(root->childItems().at(1), child2);
}

void tst_qquickitem::setParentItem()
{
    QQuickItem *root = new QQuickItem;
    QVERIFY(!root->parent());
    QVERIFY(!root->parentItem());

    QQuickItem *child1 = new QQuickItem;
    QVERIFY(!child1->parent());
    QVERIFY(!child1->parentItem());

    child1->setParentItem(root);
    QVERIFY(!child1->parent());
    QCOMPARE(child1->parentItem(), root);
    QCOMPARE(root->childItems().count(), 1);
    QCOMPARE(root->childItems().at(0), child1);

    QQuickItem *child2 = new QQuickItem;
    QVERIFY(!child2->parent());
    QVERIFY(!child2->parentItem());
    child2->setParentItem(root);
    QVERIFY(!child2->parent());
    QCOMPARE(child2->parentItem(), root);
    QCOMPARE(root->childItems().count(), 2);
    QCOMPARE(root->childItems().at(0), child1);
    QCOMPARE(root->childItems().at(1), child2);

    child1->setParentItem(0);
    QVERIFY(!child1->parent());
    QVERIFY(!child1->parentItem());
    QCOMPARE(root->childItems().count(), 1);
    QCOMPARE(root->childItems().at(0), child2);

    delete root;

    QVERIFY(!child1->parent());
    QVERIFY(!child1->parentItem());
    QVERIFY(!child2->parent());
    QVERIFY(!child2->parentItem());

    delete child1;
    delete child2;
}

void tst_qquickitem::visible()
{
    QQuickItem *root = new QQuickItem;

    QQuickItem *child1 = new QQuickItem;
    child1->setParentItem(root);

    QQuickItem *child2 = new QQuickItem;
    child2->setParentItem(root);

    QVERIFY(child1->isVisible());
    QVERIFY(child2->isVisible());

    root->setVisible(false);
    QVERIFY(!child1->isVisible());
    QVERIFY(!child2->isVisible());

    root->setVisible(true);
    QVERIFY(child1->isVisible());
    QVERIFY(child2->isVisible());

    child1->setVisible(false);
    QVERIFY(!child1->isVisible());
    QVERIFY(child2->isVisible());

    child2->setParentItem(child1);
    QVERIFY(!child1->isVisible());
    QVERIFY(!child2->isVisible());

    child2->setParentItem(root);
    QVERIFY(!child1->isVisible());
    QVERIFY(child2->isVisible());

    delete root;
    delete child1;
    delete child2;
}

void tst_qquickitem::enabled()
{
    QQuickItem *root = new QQuickItem;

    QQuickItem *child1 = new QQuickItem;
    child1->setParentItem(root);

    QQuickItem *child2 = new QQuickItem;
    child2->setParentItem(root);

    QVERIFY(child1->isEnabled());
    QVERIFY(child2->isEnabled());

    root->setEnabled(false);
    QVERIFY(!child1->isEnabled());
    QVERIFY(!child2->isEnabled());

    root->setEnabled(true);
    QVERIFY(child1->isEnabled());
    QVERIFY(child2->isEnabled());

    child1->setEnabled(false);
    QVERIFY(!child1->isEnabled());
    QVERIFY(child2->isEnabled());

    child2->setParentItem(child1);
    QVERIFY(!child1->isEnabled());
    QVERIFY(!child2->isEnabled());

    child2->setParentItem(root);
    QVERIFY(!child1->isEnabled());
    QVERIFY(child2->isEnabled());

    delete root;
    delete child1;
    delete child2;
}

void tst_qquickitem::enabledFocus()
{
    QQuickWindow window;
    ensureFocus(&window);

    QQuickFocusScope root;

    root.setFocus(true);
    root.setEnabled(false);

    QCOMPARE(root.isEnabled(), false);
    QCOMPARE(root.hasFocus(), true);
    QCOMPARE(root.hasActiveFocus(), false);

    root.setParentItem(window.contentItem());

    QCOMPARE(root.isEnabled(), false);
    QCOMPARE(root.hasFocus(), true);
    QCOMPARE(root.hasActiveFocus(), false);
    QCOMPARE(window.activeFocusItem(), window.contentItem());

    root.setEnabled(true);
    QCOMPARE(root.isEnabled(), true);
    QCOMPARE(root.hasFocus(), true);
    QCOMPARE(root.hasActiveFocus(), true);
    QCOMPARE(window.activeFocusItem(), static_cast<QQuickItem *>(&root));

    QQuickItem child1;
    child1.setParentItem(&root);

    QCOMPARE(child1.isEnabled(), true);
    QCOMPARE(child1.hasFocus(), false);
    QCOMPARE(child1.hasActiveFocus(), false);
    QCOMPARE(window.activeFocusItem(), static_cast<QQuickItem *>(&root));

    QQuickItem child2;
    child2.setFocus(true);
    child2.setParentItem(&root);

    QCOMPARE(root.isEnabled(), true);
    QCOMPARE(root.hasFocus(), true);
    QCOMPARE(root.hasActiveFocus(), true);
    QCOMPARE(child2.isEnabled(), true);
    QCOMPARE(child2.hasFocus(), true);
    QCOMPARE(child2.hasActiveFocus(), true);
    QCOMPARE(window.activeFocusItem(), &child2);

    child2.setEnabled(false);

    QCOMPARE(root.isEnabled(), true);
    QCOMPARE(root.hasFocus(), true);
    QCOMPARE(root.hasActiveFocus(), true);
    QCOMPARE(child1.isEnabled(), true);
    QCOMPARE(child1.hasFocus(), false);
    QCOMPARE(child1.hasActiveFocus(), false);
    QCOMPARE(child2.isEnabled(), false);
    QCOMPARE(child2.hasFocus(), true);
    QCOMPARE(child2.hasActiveFocus(), false);
    QCOMPARE(window.activeFocusItem(), static_cast<QQuickItem *>(&root));

    child1.setEnabled(false);
    QCOMPARE(child1.isEnabled(), false);
    QCOMPARE(child1.hasFocus(), false);
    QCOMPARE(child1.hasActiveFocus(), false);

    child1.setFocus(true);
    QCOMPARE(child1.isEnabled(), false);
    QCOMPARE(child1.hasFocus(), true);
    QCOMPARE(child1.hasActiveFocus(), false);
    QCOMPARE(child2.isEnabled(), false);
    QCOMPARE(child2.hasFocus(), false);
    QCOMPARE(child2.hasActiveFocus(), false);
    QCOMPARE(window.activeFocusItem(), static_cast<QQuickItem *>(&root));

    child1.setEnabled(true);
    QCOMPARE(child1.isEnabled(), true);
    QCOMPARE(child1.hasFocus(), true);
    QCOMPARE(child1.hasActiveFocus(), true);
    QCOMPARE(window.activeFocusItem(), static_cast<QQuickItem *>(&child1));

    root.setFocus(false);
    QCOMPARE(root.isEnabled(), true);
    QCOMPARE(root.hasFocus(), false);
    QCOMPARE(root.hasActiveFocus(), false);
    QCOMPARE(child1.isEnabled(), true);
    QCOMPARE(child1.hasFocus(), true);
    QCOMPARE(child1.hasActiveFocus(), false);
    QCOMPARE(window.activeFocusItem(), window.contentItem());

    child2.forceActiveFocus();
    QCOMPARE(root.isEnabled(), true);
    QCOMPARE(root.hasFocus(), true);
    QCOMPARE(root.hasActiveFocus(), true);
    QCOMPARE(child1.isEnabled(), true);
    QCOMPARE(child1.hasFocus(), false);
    QCOMPARE(child1.hasActiveFocus(), false);
    QCOMPARE(child2.isEnabled(), false);
    QCOMPARE(child2.hasFocus(), true);
    QCOMPARE(child2.hasActiveFocus(), false);
    QCOMPARE(window.activeFocusItem(), static_cast<QQuickItem *>(&root));

    root.setEnabled(false);
    QCOMPARE(root.isEnabled(), false);
    QCOMPARE(root.hasFocus(), true);
    QCOMPARE(root.hasActiveFocus(), false);
    QCOMPARE(child1.isEnabled(), false);
    QCOMPARE(child1.hasFocus(), false);
    QCOMPARE(child1.hasActiveFocus(), false);
    QCOMPARE(child2.isEnabled(), false);
    QCOMPARE(child2.hasFocus(), true);
    QCOMPARE(child2.hasActiveFocus(), false);
    QCOMPARE(window.activeFocusItem(), window.contentItem());

    child1.forceActiveFocus();
    QCOMPARE(root.isEnabled(), false);
    QCOMPARE(root.hasFocus(), true);
    QCOMPARE(root.hasActiveFocus(), false);
    QCOMPARE(child1.isEnabled(), false);
    QCOMPARE(child1.hasFocus(), true);
    QCOMPARE(child1.hasActiveFocus(), false);
    QCOMPARE(child2.isEnabled(), false);
    QCOMPARE(child2.hasFocus(), false);
    QCOMPARE(child2.hasActiveFocus(), false);
    QCOMPARE(window.activeFocusItem(), window.contentItem());

    root.setEnabled(true);
    QCOMPARE(root.isEnabled(), true);
    QCOMPARE(root.hasFocus(), true);
    QCOMPARE(root.hasActiveFocus(), true);
    QCOMPARE(child1.isEnabled(), true);
    QCOMPARE(child1.hasFocus(), true);
    QCOMPARE(child1.hasActiveFocus(), true);
    QCOMPARE(child2.isEnabled(), false);
    QCOMPARE(child2.hasFocus(), false);
    QCOMPARE(child2.hasActiveFocus(), false);
    QCOMPARE(window.activeFocusItem(), static_cast<QQuickItem *>(&child1));

    child2.setFocus(true);
    QCOMPARE(root.isEnabled(), true);
    QCOMPARE(root.hasFocus(), true);
    QCOMPARE(root.hasActiveFocus(), true);
    QCOMPARE(child1.isEnabled(), true);
    QCOMPARE(child1.hasFocus(), false);
    QCOMPARE(child1.hasActiveFocus(), false);
    QCOMPARE(child2.isEnabled(), false);
    QCOMPARE(child2.hasFocus(), true);
    QCOMPARE(child2.hasActiveFocus(), false);
    QCOMPARE(window.activeFocusItem(), static_cast<QQuickItem *>(&root));

    root.setEnabled(false);
    QCOMPARE(root.isEnabled(), false);
    QCOMPARE(root.hasFocus(), true);
    QCOMPARE(root.hasActiveFocus(), false);
    QCOMPARE(child1.isEnabled(), false);
    QCOMPARE(child1.hasFocus(), false);
    QCOMPARE(child1.hasActiveFocus(), false);
    QCOMPARE(child2.isEnabled(), false);
    QCOMPARE(child2.hasFocus(), true);
    QCOMPARE(child2.hasActiveFocus(), false);
    QCOMPARE(window.activeFocusItem(), window.contentItem());
}

static inline QByteArray msgItem(const QQuickItem *item)
{
    QString result;
    QDebug(&result) << item;
    return result.toLocal8Bit();
}

void tst_qquickitem::mouseGrab()
{
#ifdef Q_OS_WIN
    if (QOpenGLContext::openGLModuleType() == QOpenGLContext::LibGLES)
        QSKIP("Fails in the CI for ANGLE builds on Windows, QTBUG-32664");
#endif
    QQuickWindow window;
    window.setFramePosition(QPoint(100, 100));
    window.resize(200, 200);
    window.show();
    QVERIFY(QTest::qWaitForWindowExposed(&window));

    QScopedPointer<TestItem> child1(new TestItem);
    child1->setObjectName(QStringLiteral("child1"));
    child1->setAcceptedMouseButtons(Qt::LeftButton);
    child1->setSize(QSizeF(200, 100));
    child1->setParentItem(window.contentItem());

    QScopedPointer<TestItem> child2(new TestItem);
    child2->setObjectName(QStringLiteral("child2"));
    child2->setAcceptedMouseButtons(Qt::LeftButton);
    child2->setY(51);
    child2->setSize(QSizeF(200, 100));
    child2->setParentItem(window.contentItem());

    QTest::mousePress(&window, Qt::LeftButton, 0, QPoint(50,50));
    QTest::qWait(100);
    QVERIFY2(window.mouseGrabberItem() == child1.data(), msgItem(window.mouseGrabberItem()).constData());
    QTest::qWait(100);

    QCOMPARE(child1->pressCount, 1);
    QTest::mouseRelease(&window, Qt::LeftButton, 0, QPoint(50,50));
    QTest::qWait(50);
    QVERIFY2(window.mouseGrabberItem() == 0, msgItem(window.mouseGrabberItem()).constData());
    QCOMPARE(child1->releaseCount, 1);

    QTest::mousePress(&window, Qt::LeftButton, 0, QPoint(50,50));
    QTest::qWait(50);
    QVERIFY2(window.mouseGrabberItem() == child1.data(), msgItem(window.mouseGrabberItem()).constData());
    QCOMPARE(child1->pressCount, 2);
    child1->setEnabled(false);
    QVERIFY2(window.mouseGrabberItem() == 0, msgItem(window.mouseGrabberItem()).constData());
    QTest::mouseRelease(&window, Qt::LeftButton, 0, QPoint(50,50));
    QTest::qWait(50);
    QCOMPARE(child1->releaseCount, 1);
    child1->setEnabled(true);

    QTest::mousePress(&window, Qt::LeftButton, 0, QPoint(50,50));
    QTest::qWait(50);
    QVERIFY2(window.mouseGrabberItem() == child1.data(), msgItem(window.mouseGrabberItem()).constData());
    QCOMPARE(child1->pressCount, 3);
    child1->setVisible(false);
    QVERIFY2(window.mouseGrabberItem() == 0, msgItem(window.mouseGrabberItem()));
    QTest::mouseRelease(&window, Qt::LeftButton, 0, QPoint(50,50));
    QCOMPARE(child1->releaseCount, 1);
    child1->setVisible(true);

    QTest::mousePress(&window, Qt::LeftButton, 0, QPoint(50,50));
    QTest::qWait(50);
    QVERIFY2(window.mouseGrabberItem() == child1.data(), msgItem(window.mouseGrabberItem()).constData());
    QCOMPARE(child1->pressCount, 4);
    child2->grabMouse();
    QVERIFY2(window.mouseGrabberItem() == child2.data(), msgItem(window.mouseGrabberItem()).constData());
    QTest::mouseRelease(&window, Qt::LeftButton, 0, QPoint(50,50));
    QTest::qWait(50);
    QCOMPARE(child1->releaseCount, 1);
    QCOMPARE(child2->releaseCount, 1);

    child2->grabMouse();
    QVERIFY2(window.mouseGrabberItem() == child2.data(), msgItem(window.mouseGrabberItem()).constData());
    QTest::mousePress(&window, Qt::LeftButton, 0, QPoint(50,50));
    QTest::qWait(50);
    QCOMPARE(child1->pressCount, 4);
    QCOMPARE(child2->pressCount, 1);
    QTest::mouseRelease(&window, Qt::LeftButton, 0, QPoint(50,50));
    QTest::qWait(50);
    QCOMPARE(child1->releaseCount, 1);
    QCOMPARE(child2->releaseCount, 2);
}

void tst_qquickitem::touchEventAcceptIgnore_data()
{
    QTest::addColumn<bool>("itemSupportsTouch");

    QTest::newRow("with touch") << true;
    QTest::newRow("without touch") << false;
}

void tst_qquickitem::touchEventAcceptIgnore()
{
    QFETCH(bool, itemSupportsTouch);

    TestWindow window;
    window.resize(100, 100);
    window.show();
    QTest::qWaitForWindowExposed(&window);

    QScopedPointer<TestItem> item(new TestItem);
    item->setSize(QSizeF(100, 100));
    item->setParentItem(window.contentItem());
    item->acceptIncomingTouchEvents = itemSupportsTouch;

    static QTouchDevice* device = 0;
    if (!device) {
        device =new QTouchDevice;
        device->setType(QTouchDevice::TouchScreen);
        QWindowSystemInterface::registerTouchDevice(device);
    }

    // Send Begin, Update & End touch sequence
    {
        QTouchEvent::TouchPoint point;
        point.setId(1);
        point.setPos(QPointF(50, 50));
        point.setScreenPos(point.pos());
        point.setState(Qt::TouchPointPressed);

        QTouchEvent event(QEvent::TouchBegin, device,
                          Qt::NoModifier,
                          Qt::TouchPointPressed,
                          QList<QTouchEvent::TouchPoint>() << point);
        event.setAccepted(true);

        item->touchEventReached = false;

        bool accepted = window.event(&event);
        QQuickTouchUtils::flush(&window);

        QVERIFY(item->touchEventReached);

        // always true because QtQuick always eats touch events so as to not
        // allow QtGui to synthesise them for us.
        QCOMPARE(accepted && event.isAccepted(), true);
    }
    {
        QTouchEvent::TouchPoint point;
        point.setId(1);
        point.setPos(QPointF(60, 60));
        point.setScreenPos(point.pos());
        point.setState(Qt::TouchPointMoved);

        QTouchEvent event(QEvent::TouchUpdate, device,
                          Qt::NoModifier,
                          Qt::TouchPointMoved,
                          QList<QTouchEvent::TouchPoint>() << point);
        event.setAccepted(true);

        item->touchEventReached = false;

        bool accepted = window.event(&event);
        QQuickTouchUtils::flush(&window);

        QCOMPARE(item->touchEventReached, itemSupportsTouch);

        // always true because QtQuick always eats touch events so as to not
        // allow QtGui to synthesise them for us.
        QCOMPARE(accepted && event.isAccepted(), true);
    }
    {
        QTouchEvent::TouchPoint point;
        point.setId(1);
        point.setPos(QPointF(60, 60));
        point.setScreenPos(point.pos());
        point.setState(Qt::TouchPointReleased);

        QTouchEvent event(QEvent::TouchEnd, device,
                          Qt::NoModifier,
                          Qt::TouchPointReleased,
                          QList<QTouchEvent::TouchPoint>() << point);
        event.setAccepted(true);

        item->touchEventReached = false;

        bool accepted = window.event(&event);
        QQuickTouchUtils::flush(&window);

        QCOMPARE(item->touchEventReached, itemSupportsTouch);

        // always true because QtQuick always eats touch events so as to not
        // allow QtGui to synthesise them for us.
        QCOMPARE(accepted && event.isAccepted(), true);
    }
}

void tst_qquickitem::polishOutsideAnimation()
{
    QQuickWindow *window = new QQuickWindow;
    window->resize(200, 200);
    window->show();

    TestPolishItem *item = new TestPolishItem(window->contentItem());
    item->setSize(QSizeF(200, 100));
    QTest::qWait(50);

    QTimer::singleShot(10, item, SLOT(doPolish()));
    QTRY_VERIFY(item->wasPolished);

    delete item;
    delete window;
}

void tst_qquickitem::polishOnCompleted()
{
    QQuickView view;
    view.setSource(testFileUrl("polishOnCompleted.qml"));
    view.show();
    QTest::qWaitForWindowExposed(&view);

    TestPolishItem *item = qobject_cast<TestPolishItem*>(view.rootObject());
    QVERIFY(item);

    QTRY_VERIFY(item->wasPolished);
}

void tst_qquickitem::wheelEvent_data()
{
    QTest::addColumn<bool>("visible");
    QTest::addColumn<bool>("enabled");

    QTest::newRow("visible and enabled") << true << true;
    QTest::newRow("visible and disabled") << true << false;
    QTest::newRow("invisible and enabled") << false << true;
    QTest::newRow("invisible and disabled") << false << false;
}

void tst_qquickitem::wheelEvent()
{
    QFETCH(bool, visible);
    QFETCH(bool, enabled);

    const bool shouldReceiveWheelEvents = visible && enabled;

    QQuickWindow window;
    window.resize(200, 200);
    window.show();
    QTest::qWaitForWindowExposed(&window);

    TestItem *item = new TestItem;
    item->setSize(QSizeF(200, 100));
    item->setParentItem(window.contentItem());

    item->setEnabled(enabled);
    item->setVisible(visible);

    QWheelEvent event(QPoint(100, 50), -120, Qt::NoButton, Qt::NoModifier, Qt::Vertical);
    event.setTimestamp(123456UL);
    event.setAccepted(false);
    QGuiApplication::sendEvent(&window, &event);

    if (shouldReceiveWheelEvents) {
        QVERIFY(event.isAccepted());
        QCOMPARE(item->wheelCount, 1);
        QCOMPARE(item->timestamp, 123456UL);
    } else {
        QVERIFY(!event.isAccepted());
        QCOMPARE(item->wheelCount, 0);
    }
}

class HoverItem : public QQuickItem
{
Q_OBJECT
public:
    HoverItem(QQuickItem *parent = 0)
        : QQuickItem(parent), hoverEnterCount(0), hoverMoveCount(0), hoverLeaveCount(0)
    { }
    void resetCounters() {
        hoverEnterCount = 0;
        hoverMoveCount = 0;
        hoverLeaveCount = 0;
    }
    int hoverEnterCount;
    int hoverMoveCount;
    int hoverLeaveCount;
protected:
    virtual void hoverEnterEvent(QHoverEvent *event) {
        event->accept();
        ++hoverEnterCount;
    }
    virtual void hoverMoveEvent(QHoverEvent *event) {
        event->accept();
        ++hoverMoveCount;
    }
    virtual void hoverLeaveEvent(QHoverEvent *event) {
        event->accept();
        ++hoverLeaveCount;
    }
};

void tst_qquickitem::hoverEvent_data()
{
    QTest::addColumn<bool>("visible");
    QTest::addColumn<bool>("enabled");
    QTest::addColumn<bool>("acceptHoverEvents");

    QTest::newRow("visible, enabled, accept hover") << true << true << true;
    QTest::newRow("visible, disabled, accept hover") << true << false << true;
    QTest::newRow("invisible, enabled, accept hover") << false << true << true;
    QTest::newRow("invisible, disabled, accept hover") << false << false << true;

    QTest::newRow("visible, enabled, not accept hover") << true << true << false;
    QTest::newRow("visible, disabled, not accept hover") << true << false << false;
    QTest::newRow("invisible, enabled, not accept hover") << false << true << false;
    QTest::newRow("invisible, disabled, not accept hover") << false << false << false;
}

// ### For some unknown reason QTest::mouseMove() isn't working correctly.
static void sendMouseMove(QObject *object, const QPoint &position)
{
    QMouseEvent moveEvent(QEvent::MouseMove, position, Qt::NoButton, Qt::NoButton, 0);
    QGuiApplication::sendEvent(object, &moveEvent);
}

void tst_qquickitem::hoverEvent()
{
    QFETCH(bool, visible);
    QFETCH(bool, enabled);
    QFETCH(bool, acceptHoverEvents);

    QQuickWindow *window = new QQuickWindow();
    window->resize(200, 200);
    window->show();

    HoverItem *item = new HoverItem;
    item->setSize(QSizeF(100, 100));
    item->setParentItem(window->contentItem());

    item->setEnabled(enabled);
    item->setVisible(visible);
    item->setAcceptHoverEvents(acceptHoverEvents);

    const QPoint outside(150, 150);
    const QPoint inside(50, 50);
    const QPoint anotherInside(51, 51);

    sendMouseMove(window, outside);
    item->resetCounters();

    // Enter, then move twice inside, then leave.
    sendMouseMove(window, inside);
    sendMouseMove(window, anotherInside);
    sendMouseMove(window, inside);
    sendMouseMove(window, outside);

    const bool shouldReceiveHoverEvents = visible && enabled && acceptHoverEvents;
    if (shouldReceiveHoverEvents) {
        QCOMPARE(item->hoverEnterCount, 1);
        QCOMPARE(item->hoverMoveCount, 2);
        QCOMPARE(item->hoverLeaveCount, 1);
    } else {
        QCOMPARE(item->hoverEnterCount, 0);
        QCOMPARE(item->hoverMoveCount, 0);
        QCOMPARE(item->hoverLeaveCount, 0);
    }

    delete window;
}

void tst_qquickitem::hoverEventInParent()
{
    QQuickWindow window;
    window.resize(200, 200);
    window.show();

    HoverItem *parentItem = new HoverItem(window.contentItem());
    parentItem->setSize(QSizeF(200, 200));
    parentItem->setAcceptHoverEvents(true);

    HoverItem *leftItem = new HoverItem(parentItem);
    leftItem->setSize(QSizeF(100, 200));
    leftItem->setAcceptHoverEvents(true);

    HoverItem *rightItem = new HoverItem(parentItem);
    rightItem->setSize(QSizeF(100, 200));
    rightItem->setPosition(QPointF(100, 0));
    rightItem->setAcceptHoverEvents(true);

    const QPoint insideLeft(50, 100);
    const QPoint insideRight(150, 100);

    sendMouseMove(&window, insideLeft);
    parentItem->resetCounters();
    leftItem->resetCounters();
    rightItem->resetCounters();

    sendMouseMove(&window, insideRight);
    QCOMPARE(parentItem->hoverEnterCount, 0);
    QCOMPARE(parentItem->hoverLeaveCount, 0);
    QCOMPARE(leftItem->hoverEnterCount, 0);
    QCOMPARE(leftItem->hoverLeaveCount, 1);
    QCOMPARE(rightItem->hoverEnterCount, 1);
    QCOMPARE(rightItem->hoverLeaveCount, 0);

    sendMouseMove(&window, insideLeft);
    QCOMPARE(parentItem->hoverEnterCount, 0);
    QCOMPARE(parentItem->hoverLeaveCount, 0);
    QCOMPARE(leftItem->hoverEnterCount, 1);
    QCOMPARE(leftItem->hoverLeaveCount, 1);
    QCOMPARE(rightItem->hoverEnterCount, 1);
    QCOMPARE(rightItem->hoverLeaveCount, 1);
}

void tst_qquickitem::paintOrder_data()
{
    const QUrl order1Url = testFileUrl("order.1.qml");
    const QUrl order2Url = testFileUrl("order.2.qml");

    QTest::addColumn<QUrl>("source");
    QTest::addColumn<int>("op");
    QTest::addColumn<QVariant>("param1");
    QTest::addColumn<QVariant>("param2");
    QTest::addColumn<QStringList>("expected");

    QTest::newRow("test 1 noop") << order1Url
        << int(NoOp) << QVariant() << QVariant()
        << (QStringList() << "1" << "2" << "3");
    QTest::newRow("test 1 add") << order1Url
        << int(Append) << QVariant("new") << QVariant()
        << (QStringList() << "1" << "2" << "3" << "new");
    QTest::newRow("test 1 remove") << order1Url
        << int(Remove) << QVariant(1) << QVariant()
        << (QStringList() << "1" << "3");
    QTest::newRow("test 1 stack before") << order1Url
        << int(StackBefore) << QVariant(2) << QVariant(1)
        << (QStringList() << "1" << "3" << "2");
    QTest::newRow("test 1 stack after") << order1Url
        << int(StackAfter) << QVariant(0) << QVariant(1)
        << (QStringList() << "2" << "1" << "3");
    QTest::newRow("test 1 set z") << order1Url
        << int(SetZ) << QVariant(1) << QVariant(qreal(1.))
        << (QStringList() << "1" << "3" << "2");

    QTest::newRow("test 2 noop") << order2Url
        << int(NoOp) << QVariant() << QVariant()
        << (QStringList() << "1" << "3" << "2");
    QTest::newRow("test 2 add") << order2Url
        << int(Append) << QVariant("new") << QVariant()
        << (QStringList() << "1" << "3" << "new" << "2");
    QTest::newRow("test 2 remove 1") << order2Url
        << int(Remove) << QVariant(1) << QVariant()
        << (QStringList() << "1" << "3");
    QTest::newRow("test 2 remove 2") << order2Url
        << int(Remove) << QVariant(2) << QVariant()
        << (QStringList() << "1" << "2");
    QTest::newRow("test 2 stack before 1") << order2Url
        << int(StackBefore) << QVariant(1) << QVariant(0)
        << (QStringList() << "1" << "3" << "2");
    QTest::newRow("test 2 stack before 2") << order2Url
        << int(StackBefore) << QVariant(2) << QVariant(0)
        << (QStringList() << "3" << "1" << "2");
    QTest::newRow("test 2 stack after 1") << order2Url
        << int(StackAfter) << QVariant(0) << QVariant(1)
        << (QStringList() << "1" << "3" << "2");
    QTest::newRow("test 2 stack after 2") << order2Url
        << int(StackAfter) << QVariant(0) << QVariant(2)
        << (QStringList() << "3" << "1" << "2");
    QTest::newRow("test 1 set z") << order1Url
        << int(SetZ) << QVariant(2) << QVariant(qreal(2.))
        << (QStringList() << "1" << "2" << "3");
}

void tst_qquickitem::paintOrder()
{
    QFETCH(QUrl, source);
    QFETCH(int, op);
    QFETCH(QVariant, param1);
    QFETCH(QVariant, param2);
    QFETCH(QStringList, expected);

    QQuickView view;
    view.setSource(source);

    QQuickItem *root = qobject_cast<QQuickItem*>(view.rootObject());
    QVERIFY(root);

    switch (op) {
        case Append: {
                QQuickItem *item = new QQuickItem(root);
                item->setObjectName(param1.toString());
            }
            break;
        case Remove: {
                QQuickItem *item = root->childItems().at(param1.toInt());
                delete item;
            }
            break;
        case StackBefore: {
                QQuickItem *item1 = root->childItems().at(param1.toInt());
                QQuickItem *item2 = root->childItems().at(param2.toInt());
                item1->stackBefore(item2);
            }
            break;
        case StackAfter: {
                QQuickItem *item1 = root->childItems().at(param1.toInt());
                QQuickItem *item2 = root->childItems().at(param2.toInt());
                item1->stackAfter(item2);
            }
            break;
        case SetZ: {
                QQuickItem *item = root->childItems().at(param1.toInt());
                item->setZ(param2.toReal());
            }
            break;
        default:
            break;
    }

    QList<QQuickItem*> list = QQuickItemPrivate::get(root)->paintOrderChildItems();

    QStringList items;
    for (int i = 0; i < list.count(); ++i)
        items << list.at(i)->objectName();

    QCOMPARE(items, expected);
}

void tst_qquickitem::acceptedMouseButtons()
{
    TestItem item;
    QCOMPARE(item.acceptedMouseButtons(), Qt::MouseButtons(Qt::NoButton));

    QQuickWindow window;
    item.setSize(QSizeF(200,100));
    item.setParentItem(window.contentItem());

    QTest::mousePress(&window, Qt::LeftButton, 0, QPoint(50, 50));
    QTest::mouseRelease(&window, Qt::LeftButton, 0, QPoint(50, 50));
    QCOMPARE(item.pressCount, 0);
    QCOMPARE(item.releaseCount, 0);

    QTest::mousePress(&window, Qt::RightButton, 0, QPoint(50, 50));
    QTest::mouseRelease(&window, Qt::RightButton, 0, QPoint(50, 50));
    QCOMPARE(item.pressCount, 0);
    QCOMPARE(item.releaseCount, 0);

    QTest::mousePress(&window, Qt::MiddleButton, 0, QPoint(50, 50));
    QTest::mouseRelease(&window, Qt::MiddleButton, 0, QPoint(50, 50));
    QCOMPARE(item.pressCount, 0);
    QCOMPARE(item.releaseCount, 0);

    item.setAcceptedMouseButtons(Qt::LeftButton);
    QCOMPARE(item.acceptedMouseButtons(), Qt::MouseButtons(Qt::LeftButton));

    QTest::mousePress(&window, Qt::LeftButton, 0, QPoint(50, 50));
    QTest::mouseRelease(&window, Qt::LeftButton, 0, QPoint(50, 50));
    QCOMPARE(item.pressCount, 1);
    QCOMPARE(item.releaseCount, 1);

    QTest::mousePress(&window, Qt::RightButton, 0, QPoint(50, 50));
    QTest::mouseRelease(&window, Qt::RightButton, 0, QPoint(50, 50));
    QCOMPARE(item.pressCount, 1);
    QCOMPARE(item.releaseCount, 1);

    QTest::mousePress(&window, Qt::MiddleButton, 0, QPoint(50, 50));
    QTest::mouseRelease(&window, Qt::MiddleButton, 0, QPoint(50, 50));
    QCOMPARE(item.pressCount, 1);
    QCOMPARE(item.releaseCount, 1);

    item.setAcceptedMouseButtons(Qt::RightButton | Qt::MiddleButton);
    QCOMPARE(item.acceptedMouseButtons(), Qt::MouseButtons(Qt::RightButton | Qt::MiddleButton));

    QTest::mousePress(&window, Qt::LeftButton, 0, QPoint(50, 50));
    QTest::mouseRelease(&window, Qt::LeftButton, 0, QPoint(50, 50));
    QCOMPARE(item.pressCount, 1);
    QCOMPARE(item.releaseCount, 1);

    QTest::mousePress(&window, Qt::RightButton, 0, QPoint(50, 50));
    QTest::mouseRelease(&window, Qt::RightButton, 0, QPoint(50, 50));
    QCOMPARE(item.pressCount, 2);
    QCOMPARE(item.releaseCount, 2);

    QTest::mousePress(&window, Qt::MiddleButton, 0, QPoint(50, 50));
    QTest::mouseRelease(&window, Qt::MiddleButton, 0, QPoint(50, 50));
    QCOMPARE(item.pressCount, 3);
    QCOMPARE(item.releaseCount, 3);
}

static void gc(QQmlEngine &engine)
{
    engine.collectGarbage();
    QCoreApplication::sendPostedEvents(0, QEvent::DeferredDelete);
    QCoreApplication::processEvents();
}

void tst_qquickitem::visualParentOwnership()
{
    QQmlEngine engine;
    QQmlComponent component(&engine, testFileUrl("visualParentOwnership.qml"));

    QScopedPointer<QQuickItem> root(qobject_cast<QQuickItem*>(component.create()));
    QVERIFY(root);

    QVariant newObject;
    {
        QVERIFY(QMetaObject::invokeMethod(root.data(), "createItemWithoutParent", Q_RETURN_ARG(QVariant, newObject)));
        QPointer<QQuickItem> newItem = qvariant_cast<QQuickItem*>(newObject);
        QVERIFY(!newItem.isNull());

        QVERIFY(!newItem->parent());
        QVERIFY(!newItem->parentItem());

        newItem->setParentItem(root.data());

        gc(engine);

        QVERIFY(!newItem.isNull());
        newItem->setParentItem(0);

        gc(engine);
        QVERIFY(newItem.isNull());
    }
    {
        QVERIFY(QMetaObject::invokeMethod(root.data(), "createItemWithoutParent", Q_RETURN_ARG(QVariant, newObject)));
        QPointer<QQuickItem> firstItem = qvariant_cast<QQuickItem*>(newObject);
        QVERIFY(!firstItem.isNull());

        firstItem->setParentItem(root.data());

        QVERIFY(QMetaObject::invokeMethod(root.data(), "createItemWithoutParent", Q_RETURN_ARG(QVariant, newObject)));
        QPointer<QQuickItem> secondItem = qvariant_cast<QQuickItem*>(newObject);
        QVERIFY(!firstItem.isNull());

        secondItem->setParentItem(firstItem);

        gc(engine);

        delete firstItem;

        root->setProperty("keepAliveProperty", newObject);

        gc(engine);
        QVERIFY(!secondItem.isNull());

        root->setProperty("keepAliveProperty", QVariant());

        gc(engine);
        QVERIFY(secondItem.isNull());
    }
}

void tst_qquickitem::visualParentOwnershipWindow()
{
    QQmlEngine engine;
    QQmlComponent component(&engine, testFileUrl("visualParentOwnershipWindow.qml"));

    QQuickWindow *window = qobject_cast<QQuickWindow*>(component.create());
    QVERIFY(window);
    QQuickItem *root = window->contentItem();

    QVariant newObject;
    {
        QVERIFY(QMetaObject::invokeMethod(window, "createItemWithoutParent", Q_RETURN_ARG(QVariant, newObject)));
        QPointer<QQuickItem> newItem = qvariant_cast<QQuickItem*>(newObject);
        QVERIFY(!newItem.isNull());

        QVERIFY(!newItem->parent());
        QVERIFY(!newItem->parentItem());

        newItem->setParentItem(root);

        gc(engine);

        QVERIFY(!newItem.isNull());
        newItem->setParentItem(0);

        gc(engine);
        QVERIFY(newItem.isNull());
    }
    {
        QVERIFY(QMetaObject::invokeMethod(window, "createItemWithoutParent", Q_RETURN_ARG(QVariant, newObject)));
        QPointer<QQuickItem> firstItem = qvariant_cast<QQuickItem*>(newObject);
        QVERIFY(!firstItem.isNull());

        firstItem->setParentItem(root);

        QVERIFY(QMetaObject::invokeMethod(window, "createItemWithoutParent", Q_RETURN_ARG(QVariant, newObject)));
        QPointer<QQuickItem> secondItem = qvariant_cast<QQuickItem*>(newObject);
        QVERIFY(!firstItem.isNull());

        secondItem->setParentItem(firstItem);

        gc(engine);

        delete firstItem;

        window->setProperty("keepAliveProperty", newObject);

        gc(engine);
        QVERIFY(!secondItem.isNull());

        window->setProperty("keepAliveProperty", QVariant());

        gc(engine);
        QVERIFY(secondItem.isNull());
    }
}

class InvalidatedItem : public QQuickItem {
    Q_OBJECT
signals:
    void invalidated();
public slots:
    void invalidateSceneGraph() { emit invalidated(); }
};

void tst_qquickitem::testSGInvalidate()
{
    for (int i=0; i<2; ++i) {
        QScopedPointer<QQuickView> view(new QQuickView());

        InvalidatedItem *item = new InvalidatedItem();

        int expected = 0;
        if (i == 0) {
            // First iteration, item has contents and should get signals
            expected = 1;
            item->setFlag(QQuickItem::ItemHasContents, true);
        } else {
            // Second iteration, item does not have content and will not get signals
        }

        QSignalSpy invalidateSpy(item, SIGNAL(invalidated()));
        item->setParentItem(view->contentItem());
        view->show();

        QVERIFY(QTest::qWaitForWindowExposed(view.data()));

        delete view.take();
        QCOMPARE(invalidateSpy.size(), expected);
    }
}

void tst_qquickitem::objectChildTransform()
{
    QQuickView view;
    view.setSource(testFileUrl("objectChildTransform.qml"));

    QQuickItem *root = qobject_cast<QQuickItem*>(view.rootObject());
    QVERIFY(root);

    root->setProperty("source", QString());
    // Shouldn't crash.
}

void tst_qquickitem::contains_data()
{
    QTest::addColumn<int>("x");
    QTest::addColumn<int>("y");
    QTest::addColumn<bool>("contains");

    QTest::newRow("(0, 0) = false") << 0 << 0 << false;
    QTest::newRow("(50, 0) = false") << 50 << 0 << false;
    QTest::newRow("(0, 50) = false") << 0 << 50 << false;
    QTest::newRow("(50, 50) = true") << 50 << 50 << true;
    QTest::newRow("(100, 100) = true") << 100 << 100 << true;
    QTest::newRow("(150, 150) = false") << 150 << 150 << false;
}

void tst_qquickitem::contains()
{
    // Tests that contains works, but also checks that mapToItem/mapFromItem
    // return the correct type (point or rect, not a JS object with those properties),
    // as this is a common combination of calls.

    QFETCH(int, x);
    QFETCH(int, y);
    QFETCH(bool, contains);

    QQuickView view;
    view.setSource(testFileUrl("contains.qml"));

    QQuickItem *root = qobject_cast<QQuickItem*>(view.rootObject());
    QVERIFY(root);

    QVariant result = false;
    QVERIFY(QMetaObject::invokeMethod(root, "childContainsViaMapToItem",
        Q_RETURN_ARG(QVariant, result), Q_ARG(QVariant, qreal(x)), Q_ARG(QVariant, qreal(y))));
    QCOMPARE(result.toBool(), contains);

    result = false;
    QVERIFY(QMetaObject::invokeMethod(root, "childContainsViaMapFromItem",
        Q_RETURN_ARG(QVariant, result), Q_ARG(QVariant, qreal(x)), Q_ARG(QVariant, qreal(y))));
    QCOMPARE(result.toBool(), contains);
}

void tst_qquickitem::childAt()
{
    QQuickView view;
    view.setSource(testFileUrl("childAtRectangle.qml"));
    QQuickItem *root = qobject_cast<QQuickItem*>(view.rootObject());

    int found = 0;
    for (int i = 0; i < 16; i++)
    {
        if (root->childAt(i, 0))
            found++;
    }
    QCOMPARE(found, 16);

    found = 0;
    for (int i = 0; i < 16; i++)
    {
        if (root->childAt(0, i))
            found++;
    }
    QCOMPARE(found, 16);

    found = 0;
    for (int i = 0; i < 2; i++)
    {
        if (root->childAt(18 + i, 0))
            found++;
    }
    QCOMPARE(found, 1);

    found = 0;
    for (int i = 0; i < 16; i++)
    {
        if (root->childAt(18, i))
            found++;
    }
    QCOMPARE(found, 1);

    QVERIFY(!root->childAt(19,19));
}

void tst_qquickitem::ignoreButtonPressNotInAcceptedMouseButtons()
{
    // Verify the fix for QTBUG-31861
    TestItem item;
    QCOMPARE(item.acceptedMouseButtons(), Qt::MouseButtons(Qt::NoButton));

    QQuickWindow window;
    item.setSize(QSizeF(200,100));
    item.setParentItem(window.contentItem());

    item.setAcceptedMouseButtons(Qt::LeftButton);
    QCOMPARE(item.acceptedMouseButtons(), Qt::MouseButtons(Qt::LeftButton));

    QTest::mousePress(&window, Qt::LeftButton, 0, QPoint(50, 50));
    QTest::mousePress(&window, Qt::RightButton, 0, QPoint(50, 50)); // ignored because it's not LeftButton
    QTest::mouseRelease(&window, Qt::RightButton, 0, QPoint(50, 50)); // ignored because it didn't grab the RightButton press
    QTest::mouseRelease(&window, Qt::LeftButton, 0, QPoint(50, 50));

    QCOMPARE(item.pressCount, 1);
    QCOMPARE(item.releaseCount, 1);
}

QTEST_MAIN(tst_qquickitem)

#include "tst_qquickitem.moc"
