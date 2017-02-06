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
#include <QtTest/QSignalSpy>
#include <QtQml/qqmlengine.h>
#include <QtQml/qqmlcomponent.h>
#include <QtQml/qqmlcontext.h>
#include <QtQuick/qquickitemgrabresult.h>
#include <QtQuick/qquickview.h>
#include <QtGui/private/qinputmethod_p.h>
#include <QtQuick/private/qquickrectangle_p.h>
#include <QtQuick/private/qquicktextinput_p.h>
#include <QtQuick/private/qquickitemchangelistener_p.h>
#include <QtGui/qstylehints.h>
#include <private/qquickitem_p.h>
#include "../../shared/util.h"
#include "../shared/visualtestutil.h"
#include "../../shared/platforminputcontext.h"

using namespace QQuickVisualTestUtil;

class tst_QQuickItem : public QQmlDataTest
{
    Q_OBJECT
public:
    tst_QQuickItem();

private slots:
    void initTestCase();
    void cleanup();

    void activeFocusOnTab();
    void activeFocusOnTab2();
    void activeFocusOnTab3();
    void activeFocusOnTab4();
    void activeFocusOnTab5();
    void activeFocusOnTab6();
    void activeFocusOnTab7();
    void activeFocusOnTab8();
    void activeFocusOnTab9();
    void activeFocusOnTab10();

    void nextItemInFocusChain();
    void nextItemInFocusChain2();
    void nextItemInFocusChain3();

    void tabFence();
    void qtbug_50516();
    void qtbug_50516_2_data();
    void qtbug_50516_2();

    void keys();
    void standardKeys_data();
    void standardKeys();
    void keysProcessingOrder();
    void keysim();
    void keysForward();
    void keyNavigation_data();
    void keyNavigation();
    void keyNavigation_RightToLeft();
    void keyNavigation_skipNotVisible();
    void keyNavigation_implicitSetting();
    void keyNavigation_focusReason();
    void keyNavigation_loop();
    void layoutMirroring();
    void layoutMirroringWindow();
    void layoutMirroringIllegalParent();
    void smooth();
    void antialiasing();
    void clip();
    void mapCoordinates();
    void mapCoordinates_data();
    void mapCoordinatesRect();
    void mapCoordinatesRect_data();
    void propertyChanges();
    void transforms();
    void transforms_data();
    void childrenRect();
    void childrenRectBug();
    void childrenRectBug2();
    void childrenRectBug3();
    void childrenRectBottomRightCorner();

    void childrenProperty();
    void resourcesProperty();

    void changeListener();
    void transformCrash();
    void implicitSize();
    void qtbug_16871();
    void visibleChildren();
    void parentLoop();
    void contains_data();
    void contains();
    void childAt();
    void isAncestorOf();

    void grab();

private:
    QQmlEngine engine;
    bool qt_tab_all_widgets() {
        return QGuiApplication::styleHints()->tabFocusBehavior() == Qt::TabFocusAllControls;
    }
};

class KeysTestObject : public QObject
{
    Q_OBJECT

    Q_PROPERTY(bool processLast READ processLast NOTIFY processLastChanged)

public:
    KeysTestObject() : mKey(0), mModifiers(0), mForwardedKey(0), mLast(false), mNativeScanCode(0) {}

    void reset() {
        mKey = 0;
        mText = QString();
        mModifiers = 0;
        mForwardedKey = 0;
        mNativeScanCode = 0;
    }

    bool processLast() const { return mLast; }
    void setProcessLast(bool b) {
        if (b != mLast) {
            mLast = b;
            emit processLastChanged();
        }
    }

public slots:
    void keyPress(int key, QString text, int modifiers) {
        mKey = key;
        mText = text;
        mModifiers = modifiers;
    }
    void keyRelease(int key, QString text, int modifiers) {
        mKey = key;
        mText = text;
        mModifiers = modifiers;
    }
    void forwardedKey(int key) {
        mForwardedKey = key;
    }
    void specialKey(int key, QString text, quint32 nativeScanCode) {
        mKey = key;
        mText = text;
        mNativeScanCode = nativeScanCode;
    }

signals:
    void processLastChanged();

public:
    int mKey;
    QString mText;
    int mModifiers;
    int mForwardedKey;
    bool mLast;
    quint32 mNativeScanCode;

private:
};

class KeyTestItem : public QQuickItem
{
    Q_OBJECT
public:
    KeyTestItem(QQuickItem *parent=0) : QQuickItem(parent), mKey(0) {}

protected:
    void keyPressEvent(QKeyEvent *e) {
        mKey = e->key();

        if (e->key() == Qt::Key_A)
            e->accept();
        else
            e->ignore();
    }

    void keyReleaseEvent(QKeyEvent *e) {
        if (e->key() == Qt::Key_B)
            e->accept();
        else
            e->ignore();
    }

public:
    int mKey;
};

class FocusEventFilter : public QObject
{
protected:
    bool eventFilter(QObject *watched, QEvent *event) {
        if ((event->type() == QEvent::FocusIn) ||  (event->type() == QEvent::FocusOut)) {
            QFocusEvent *focusEvent = static_cast<QFocusEvent *>(event);
            lastFocusReason = focusEvent->reason();
            return false;
        } else
            return QObject::eventFilter(watched, event);
    }
public:
    Qt::FocusReason lastFocusReason;
};

QML_DECLARE_TYPE(KeyTestItem);

class HollowTestItem : public QQuickItem
{
    Q_OBJECT
    Q_PROPERTY(bool circle READ isCircle WRITE setCircle)
    Q_PROPERTY(qreal holeRadius READ holeRadius WRITE setHoleRadius)

public:
    HollowTestItem(QQuickItem *parent = 0)
        : QQuickItem(parent),
          m_isPressed(false),
          m_isHovered(false),
          m_isCircle(false),
          m_holeRadius(50)
    {
        setAcceptHoverEvents(true);
        setAcceptedMouseButtons(Qt::LeftButton);
    }

    bool isPressed() const { return m_isPressed; }
    bool isHovered() const { return m_isHovered; }

    bool isCircle() const { return m_isCircle; }
    void setCircle(bool circle) { m_isCircle = circle; }

    qreal holeRadius() const { return m_holeRadius; }
    void setHoleRadius(qreal radius) { m_holeRadius = radius; }

    bool contains(const QPointF &point) const {
        const qreal w = width();
        const qreal h = height();
        const qreal r = m_holeRadius;

        // check boundaries
        if (!QRectF(0, 0, w, h).contains(point))
            return false;

        // square shape
        if (!m_isCircle)
            return !QRectF(w / 2 - r, h / 2 - r, r * 2, r * 2).contains(point);

        // circle shape
        const qreal dx = point.x() - (w / 2);
        const qreal dy = point.y() - (h / 2);
        const qreal dd = (dx * dx) + (dy * dy);
        const qreal outerRadius = qMin<qreal>(w / 2, h / 2);
        return dd > (r * r) && dd <= outerRadius * outerRadius;
    }

protected:
    void hoverEnterEvent(QHoverEvent *) { m_isHovered = true; }
    void hoverLeaveEvent(QHoverEvent *) { m_isHovered = false; }
    void mousePressEvent(QMouseEvent *) { m_isPressed = true; }
    void mouseReleaseEvent(QMouseEvent *) { m_isPressed = false; }

private:
    bool m_isPressed;
    bool m_isHovered;
    bool m_isCircle;
    qreal m_holeRadius;
};

QML_DECLARE_TYPE(HollowTestItem);

class TabFenceItem : public QQuickItem
{
    Q_OBJECT

public:
    TabFenceItem(QQuickItem *parent = Q_NULLPTR)
        : QQuickItem(parent)
    {
        QQuickItemPrivate *d = QQuickItemPrivate::get(this);
        d->isTabFence = true;
    }
};

QML_DECLARE_TYPE(TabFenceItem);

class TabFenceItem2 : public QQuickItem
{
    Q_OBJECT

public:
    TabFenceItem2(QQuickItem *parent = Q_NULLPTR)
        : QQuickItem(parent)
    {
        QQuickItemPrivate *d = QQuickItemPrivate::get(this);
        d->isTabFence = true;
        setFlag(ItemIsFocusScope);
    }
};

QML_DECLARE_TYPE(TabFenceItem2);

tst_QQuickItem::tst_QQuickItem()
{
}

void tst_QQuickItem::initTestCase()
{
    QQmlDataTest::initTestCase();
    qmlRegisterType<KeyTestItem>("Test",1,0,"KeyTestItem");
    qmlRegisterType<HollowTestItem>("Test", 1, 0, "HollowTestItem");
    qmlRegisterType<TabFenceItem>("Test", 1, 0, "TabFence");
    qmlRegisterType<TabFenceItem2>("Test", 1, 0, "TabFence2");
}

void tst_QQuickItem::cleanup()
{
    QInputMethodPrivate *inputMethodPrivate = QInputMethodPrivate::get(qApp->inputMethod());
    inputMethodPrivate->testContext = 0;
}

void tst_QQuickItem::activeFocusOnTab()
{
    if (!qt_tab_all_widgets())
        QSKIP("This function doesn't support NOT iterating all.");

    QQuickView *window = new QQuickView(0);
    window->setBaseSize(QSize(800,600));

    window->setSource(testFileUrl("activeFocusOnTab.qml"));
    window->show();
    window->requestActivate();
    QVERIFY(QTest::qWaitForWindowActive(window));
    QCOMPARE(QGuiApplication::focusWindow(), window);

    // original: button12
    QQuickItem *item = findItem<QQuickItem>(window->rootObject(), "button12");
    QVERIFY(item);
    QVERIFY(item->hasActiveFocus());

    // Tab: button12->sub2
    QKeyEvent key(QEvent::KeyPress, Qt::Key_Tab, Qt::NoModifier, "", false, 1);
    QGuiApplication::sendEvent(window, &key);
    QVERIFY(key.isAccepted());

    item = findItem<QQuickItem>(window->rootObject(), "sub2");
    QVERIFY(item);
    QVERIFY(item->hasActiveFocus());

    // Tab: sub2->button21
    key = QKeyEvent(QEvent::KeyPress, Qt::Key_Tab, Qt::NoModifier, "", false, 1);
    QGuiApplication::sendEvent(window, &key);
    QVERIFY(key.isAccepted());

    item = findItem<QQuickItem>(window->rootObject(), "button21");
    QVERIFY(item);
    QVERIFY(item->hasActiveFocus());

    // Tab: button21->button22
    key = QKeyEvent(QEvent::KeyPress, Qt::Key_Tab, Qt::NoModifier, "", false, 1);
    QGuiApplication::sendEvent(window, &key);
    QVERIFY(key.isAccepted());

    item = findItem<QQuickItem>(window->rootObject(), "button22");
    QVERIFY(item);
    QVERIFY(item->hasActiveFocus());

    // Tab: button22->edit
    key = QKeyEvent(QEvent::KeyPress, Qt::Key_Tab, Qt::NoModifier, "", false, 1);
    QGuiApplication::sendEvent(window, &key);
    QVERIFY(key.isAccepted());

    item = findItem<QQuickItem>(window->rootObject(), "edit");
    QVERIFY(item);
    QVERIFY(item->hasActiveFocus());

    // BackTab: edit->button22
    key = QKeyEvent(QEvent::KeyPress, Qt::Key_Tab, Qt::ShiftModifier, "", false, 1);
    QGuiApplication::sendEvent(window, &key);
    QVERIFY(key.isAccepted());

    item = findItem<QQuickItem>(window->rootObject(), "button22");
    QVERIFY(item);
    QVERIFY(item->hasActiveFocus());

    // BackTab: button22->button21
    key = QKeyEvent(QEvent::KeyPress, Qt::Key_Tab, Qt::ShiftModifier, "", false, 1);
    QGuiApplication::sendEvent(window, &key);
    QVERIFY(key.isAccepted());

    item = findItem<QQuickItem>(window->rootObject(), "button21");
    QVERIFY(item);
    QVERIFY(item->hasActiveFocus());

    // BackTab: button21->sub2
    key = QKeyEvent(QEvent::KeyPress, Qt::Key_Tab, Qt::ShiftModifier, "", false, 1);
    QGuiApplication::sendEvent(window, &key);
    QVERIFY(key.isAccepted());

    item = findItem<QQuickItem>(window->rootObject(), "sub2");
    QVERIFY(item);
    QVERIFY(item->hasActiveFocus());

    // BackTab: sub2->button12
    key = QKeyEvent(QEvent::KeyPress, Qt::Key_Tab, Qt::ShiftModifier, "", false, 1);
    QGuiApplication::sendEvent(window, &key);
    QVERIFY(key.isAccepted());

    item = findItem<QQuickItem>(window->rootObject(), "button12");
    QVERIFY(item);
    QVERIFY(item->hasActiveFocus());

    // BackTab: button12->button11
    key = QKeyEvent(QEvent::KeyPress, Qt::Key_Tab, Qt::ShiftModifier, "", false, 1);
    QGuiApplication::sendEvent(window, &key);
    QVERIFY(key.isAccepted());

    item = findItem<QQuickItem>(window->rootObject(), "button11");
    QVERIFY(item);
    QVERIFY(item->hasActiveFocus());

    // BackTab: button11->edit
    key = QKeyEvent(QEvent::KeyPress, Qt::Key_Tab, Qt::ShiftModifier, "", false, 1);
    QGuiApplication::sendEvent(window, &key);
    QVERIFY(key.isAccepted());

    item = findItem<QQuickItem>(window->rootObject(), "edit");
    QVERIFY(item);
    QVERIFY(item->hasActiveFocus());

    delete window;
}

void tst_QQuickItem::activeFocusOnTab2()
{
    if (!qt_tab_all_widgets())
        QSKIP("This function doesn't support NOT iterating all.");

    QQuickView *window = new QQuickView(0);
    window->setBaseSize(QSize(800,600));

    window->setSource(testFileUrl("activeFocusOnTab.qml"));
    window->show();
    window->requestActivate();
    QVERIFY(QTest::qWaitForWindowActive(window));
    QCOMPARE(QGuiApplication::focusWindow(), window);

    // original: button12
    QQuickItem *item = findItem<QQuickItem>(window->rootObject(), "button12");
    QVERIFY(item);
    QVERIFY(item->hasActiveFocus());

    // BackTab: button12->button11
    QKeyEvent key(QEvent::KeyPress, Qt::Key_Tab, Qt::ShiftModifier, "", false, 1);
    QGuiApplication::sendEvent(window, &key);
    QVERIFY(key.isAccepted());

    item = findItem<QQuickItem>(window->rootObject(), "button11");
    QVERIFY(item);
    QVERIFY(item->hasActiveFocus());

    // BackTab: button11->edit
    key = QKeyEvent(QEvent::KeyPress, Qt::Key_Tab, Qt::ShiftModifier, "", false, 1);
    QGuiApplication::sendEvent(window, &key);
    QVERIFY(key.isAccepted());

    item = findItem<QQuickItem>(window->rootObject(), "edit");
    QVERIFY(item);
    QVERIFY(item->hasActiveFocus());

    delete window;
}

void tst_QQuickItem::activeFocusOnTab3()
{
    if (!qt_tab_all_widgets())
        QSKIP("This function doesn't support NOT iterating all.");

    QQuickView *window = new QQuickView(0);
    window->setBaseSize(QSize(800,600));

    window->setSource(testFileUrl("activeFocusOnTab3.qml"));
    window->show();
    window->requestActivate();
    QVERIFY(QTest::qWaitForWindowActive(window));
    QCOMPARE(QGuiApplication::focusWindow(), window);

    // original: button1
    QQuickItem *item = findItem<QQuickItem>(window->rootObject(), "button1");
    QVERIFY(item);
    QVERIFY(item->hasActiveFocus());

    // 4 Tabs: button1->button2, through a repeater
    QKeyEvent key(QEvent::KeyPress, Qt::Key_Tab, Qt::NoModifier, "", false, 1);;
    for (int i = 0; i < 4; ++i) {
        QGuiApplication::sendEvent(window, &key);
        QVERIFY(key.isAccepted());
    }

    item = findItem<QQuickItem>(window->rootObject(), "button2");
    QVERIFY(item);
    QVERIFY(item->hasActiveFocus());

    // 4 Tabs: button2->button3, through a row
    key = QKeyEvent(QEvent::KeyPress, Qt::Key_Tab, Qt::NoModifier, "", false, 1);;
    for (int i = 0; i < 4; ++i) {
        QGuiApplication::sendEvent(window, &key);
        QVERIFY(key.isAccepted());
    }

    item = findItem<QQuickItem>(window->rootObject(), "button3");
    QVERIFY(item);
    QVERIFY(item->hasActiveFocus());

    // 4 Tabs: button3->button4, through a flow
    key = QKeyEvent(QEvent::KeyPress, Qt::Key_Tab, Qt::NoModifier, "", false, 1);;
    for (int i = 0; i < 4; ++i) {
        QGuiApplication::sendEvent(window, &key);
        QVERIFY(key.isAccepted());
    }

    item = findItem<QQuickItem>(window->rootObject(), "button4");
    QVERIFY(item);
    QVERIFY(item->hasActiveFocus());

    // 4 Tabs: button4->button5, through a focusscope
    // parent is activeFocusOnTab:false, one of children is focus:true
    key = QKeyEvent(QEvent::KeyPress, Qt::Key_Tab, Qt::NoModifier, "", false, 1);;
    for (int i = 0; i < 4; ++i) {
        QGuiApplication::sendEvent(window, &key);
        QVERIFY(key.isAccepted());
    }

    item = findItem<QQuickItem>(window->rootObject(), "button5");
    QVERIFY(item);
    QVERIFY(item->hasActiveFocus());

    // 4 Tabs: button5->button6, through a focusscope
    // parent is activeFocusOnTab:true, one of children is focus:true
    key = QKeyEvent(QEvent::KeyPress, Qt::Key_Tab, Qt::NoModifier, "", false, 1);;
    for (int i = 0; i < 4; ++i) {
        QGuiApplication::sendEvent(window, &key);
        QVERIFY(key.isAccepted());
    }

    item = findItem<QQuickItem>(window->rootObject(), "button6");
    QVERIFY(item);
    QVERIFY(item->hasActiveFocus());

    // 5 Tabs: button6->button7, through a focusscope
    // parent is activeFocusOnTab:true, none of children is focus:true
    key = QKeyEvent(QEvent::KeyPress, Qt::Key_Tab, Qt::NoModifier, "", false, 1);;
    for (int i = 0; i < 5; ++i) {
        QGuiApplication::sendEvent(window, &key);
        QVERIFY(key.isAccepted());
    }

    item = findItem<QQuickItem>(window->rootObject(), "button7");
    QVERIFY(item);
    QVERIFY(item->hasActiveFocus());

    // 4 BackTabs: button7->button6, through a focusscope
    // parent is activeFocusOnTab:true, one of children got focus:true in previous code
    key = QKeyEvent(QEvent::KeyPress, Qt::Key_Tab, Qt::ShiftModifier, "", false, 1);
    for (int i = 0; i < 4; ++i) {
        QGuiApplication::sendEvent(window, &key);
        QVERIFY(key.isAccepted());
    }

    item = findItem<QQuickItem>(window->rootObject(), "button6");
    QVERIFY(item);
    QVERIFY(item->hasActiveFocus());

    // 4 Tabs: button6->button7, through a focusscope
    // parent is activeFocusOnTab:true, one of children got focus:true in previous code
    key = QKeyEvent(QEvent::KeyPress, Qt::Key_Tab, Qt::NoModifier, "", false, 1);;
    for (int i = 0; i < 4; ++i) {
        QGuiApplication::sendEvent(window, &key);
        QVERIFY(key.isAccepted());
    }

    item = findItem<QQuickItem>(window->rootObject(), "button7");
    QVERIFY(item);
    QVERIFY(item->hasActiveFocus());

    // 4 BackTabs: button7->button6, through a focusscope
    // parent is activeFocusOnTab:true, one of children got focus:true in previous code
    key = QKeyEvent(QEvent::KeyPress, Qt::Key_Tab, Qt::ShiftModifier, "", false, 1);
    for (int i = 0; i < 4; ++i) {
        QGuiApplication::sendEvent(window, &key);
        QVERIFY(key.isAccepted());
    }

    item = findItem<QQuickItem>(window->rootObject(), "button6");
    QVERIFY(item);
    QVERIFY(item->hasActiveFocus());

    // 4 BackTabs: button6->button5, through a focusscope(parent is activeFocusOnTab: false)
    key = QKeyEvent(QEvent::KeyPress, Qt::Key_Tab, Qt::ShiftModifier, "", false, 1);
    for (int i = 0; i < 4; ++i) {
        QGuiApplication::sendEvent(window, &key);
        QVERIFY(key.isAccepted());
    }

    item = findItem<QQuickItem>(window->rootObject(), "button5");
    QVERIFY(item);
    QVERIFY(item->hasActiveFocus());

    // 4 BackTabs: button5->button4, through a focusscope(parent is activeFocusOnTab: false)
    key = QKeyEvent(QEvent::KeyPress, Qt::Key_Tab, Qt::ShiftModifier, "", false, 1);
    for (int i = 0; i < 4; ++i) {
        QGuiApplication::sendEvent(window, &key);
        QVERIFY(key.isAccepted());
    }

    item = findItem<QQuickItem>(window->rootObject(), "button4");
    QVERIFY(item);
    QVERIFY(item->hasActiveFocus());

    // 4 BackTabs: button4->button3, through a flow
    key = QKeyEvent(QEvent::KeyPress, Qt::Key_Tab, Qt::ShiftModifier, "", false, 1);
    for (int i = 0; i < 4; ++i) {
        QGuiApplication::sendEvent(window, &key);
        QVERIFY(key.isAccepted());
    }

    item = findItem<QQuickItem>(window->rootObject(), "button3");
    QVERIFY(item);
    QVERIFY(item->hasActiveFocus());

    // 4 BackTabs: button3->button2, through a row
    key = QKeyEvent(QEvent::KeyPress, Qt::Key_Tab, Qt::ShiftModifier, "", false, 1);
    for (int i = 0; i < 4; ++i) {
        QGuiApplication::sendEvent(window, &key);
        QVERIFY(key.isAccepted());
    }

    item = findItem<QQuickItem>(window->rootObject(), "button2");
    QVERIFY(item);
    QVERIFY(item->hasActiveFocus());

    // 4 BackTabs: button2->button1, through a repeater
    key = QKeyEvent(QEvent::KeyPress, Qt::Key_Tab, Qt::ShiftModifier, "", false, 1);
    for (int i = 0; i < 4; ++i) {
        QGuiApplication::sendEvent(window, &key);
        QVERIFY(key.isAccepted());
    }

    item = findItem<QQuickItem>(window->rootObject(), "button1");
    QVERIFY(item);
    QVERIFY(item->hasActiveFocus());

    delete window;
}

void tst_QQuickItem::activeFocusOnTab4()
{
    if (!qt_tab_all_widgets())
        QSKIP("This function doesn't support NOT iterating all.");

    QQuickView *window = new QQuickView(0);
    window->setBaseSize(QSize(800,600));

    window->setSource(testFileUrl("activeFocusOnTab4.qml"));
    window->show();
    window->requestActivate();
    QVERIFY(QTest::qWaitForWindowActive(window));
    QCOMPARE(QGuiApplication::focusWindow(), window);

    // original: button11
    QQuickItem *item = findItem<QQuickItem>(window->rootObject(), "button11");
    item->setActiveFocusOnTab(true);
    QVERIFY(item);
    QVERIFY(item->hasActiveFocus());

    // Tab: button11->button21
    QKeyEvent key(QEvent::KeyPress, Qt::Key_Tab, Qt::NoModifier, "", false, 1);
    QGuiApplication::sendEvent(window, &key);
    QVERIFY(key.isAccepted());

    item = findItem<QQuickItem>(window->rootObject(), "button21");
    QVERIFY(item);
    QVERIFY(item->hasActiveFocus());

    delete window;
}

void tst_QQuickItem::activeFocusOnTab5()
{
    if (!qt_tab_all_widgets())
        QSKIP("This function doesn't support NOT iterating all.");

    QQuickView *window = new QQuickView(0);
    window->setBaseSize(QSize(800,600));

    window->setSource(testFileUrl("activeFocusOnTab4.qml"));
    window->show();
    window->requestActivate();
    QVERIFY(QTest::qWaitForWindowActive(window));
    QCOMPARE(QGuiApplication::focusWindow(), window);

    // original: button11 in sub1
    QQuickItem *item = findItem<QQuickItem>(window->rootObject(), "button11");
    QVERIFY(item);
    QVERIFY(item->hasActiveFocus());

    QQuickItem *item2 = findItem<QQuickItem>(window->rootObject(), "sub1");
    item2->setActiveFocusOnTab(true);

    // Tab: button11->button21
    QKeyEvent key(QEvent::KeyPress, Qt::Key_Tab, Qt::NoModifier, "", false, 1);
    QGuiApplication::sendEvent(window, &key);
    QVERIFY(key.isAccepted());

    item = findItem<QQuickItem>(window->rootObject(), "button21");
    QVERIFY(item);
    QVERIFY(item->hasActiveFocus());

    delete window;
}

void tst_QQuickItem::activeFocusOnTab6()
{
    if (qt_tab_all_widgets())
        QSKIP("This function doesn't support iterating all.");

    QQuickView *window = new QQuickView(0);
    window->setBaseSize(QSize(800,600));

    window->setSource(testFileUrl("activeFocusOnTab6.qml"));
    window->show();
    window->requestActivate();
    QVERIFY(QTest::qWaitForWindowActive(window));
    QCOMPARE(QGuiApplication::focusWindow(), window);

    // original: button12
    QQuickItem *item = findItem<QQuickItem>(window->rootObject(), "button12");
    QVERIFY(item);
    QVERIFY(item->hasActiveFocus());

    // Tab: button12->edit
    QKeyEvent key(QEvent::KeyPress, Qt::Key_Tab, Qt::NoModifier, "", false, 1);
    QGuiApplication::sendEvent(window, &key);
    QVERIFY(key.isAccepted());

    item = findItem<QQuickItem>(window->rootObject(), "edit");
    QVERIFY(item);
    QVERIFY(item->hasActiveFocus());

    // BackTab: edit->button12
    key = QKeyEvent(QEvent::KeyPress, Qt::Key_Tab, Qt::ShiftModifier, "", false, 1);
    QGuiApplication::sendEvent(window, &key);
    QVERIFY(key.isAccepted());

    item = findItem<QQuickItem>(window->rootObject(), "button12");
    QVERIFY(item);
    QVERIFY(item->hasActiveFocus());

    // BackTab: button12->button11
    key = QKeyEvent(QEvent::KeyPress, Qt::Key_Tab, Qt::ShiftModifier, "", false, 1);
    QGuiApplication::sendEvent(window, &key);
    QVERIFY(key.isAccepted());

    item = findItem<QQuickItem>(window->rootObject(), "button11");
    QVERIFY(item);
    QVERIFY(item->hasActiveFocus());

    // BackTab: button11->edit
    key = QKeyEvent(QEvent::KeyPress, Qt::Key_Tab, Qt::ShiftModifier, "", false, 1);
    QGuiApplication::sendEvent(window, &key);
    QVERIFY(key.isAccepted());

    item = findItem<QQuickItem>(window->rootObject(), "edit");
    QVERIFY(item);
    QVERIFY(item->hasActiveFocus());

    delete window;
}

void tst_QQuickItem::activeFocusOnTab7()
{
    if (qt_tab_all_widgets())
        QSKIP("This function doesn't support iterating all.");

    QQuickView *window = new QQuickView(0);
    window->setBaseSize(QSize(300,300));

    window->setSource(testFileUrl("activeFocusOnTab7.qml"));
    window->show();
    window->requestActivate();
    QVERIFY(QTest::qWaitForWindowActive(window));
    QCOMPARE(QGuiApplication::focusWindow(), window);

    QQuickItem *item = findItem<QQuickItem>(window->rootObject(), "button1");
    QVERIFY(item);
    item->forceActiveFocus();
    QVERIFY(item->hasActiveFocus());

    // Tab: button1->button1
    QKeyEvent key(QEvent::KeyPress, Qt::Key_Tab, Qt::NoModifier, "", false, 1);
    QGuiApplication::sendEvent(window, &key);
    QVERIFY(!key.isAccepted());

    QVERIFY(item->hasActiveFocus());

    // BackTab: button1->button1
    key = QKeyEvent(QEvent::KeyPress, Qt::Key_Tab, Qt::ShiftModifier, "", false, 1);
    QGuiApplication::sendEvent(window, &key);
    QVERIFY(!key.isAccepted());

    QVERIFY(item->hasActiveFocus());

    delete window;
}

void tst_QQuickItem::activeFocusOnTab8()
{
    QQuickView *window = new QQuickView(0);
    window->setBaseSize(QSize(300,300));

    window->setSource(testFileUrl("activeFocusOnTab8.qml"));
    window->show();
    window->requestActivate();
    QVERIFY(QTest::qWaitForWindowActive(window));
    QCOMPARE(QGuiApplication::focusWindow(), window);

    QQuickItem *content = window->contentItem();
    QVERIFY(content);
    QVERIFY(content->hasActiveFocus());

    QQuickItem *button1 = findItem<QQuickItem>(window->rootObject(), "button1");
    QVERIFY(button1);
    QVERIFY(!button1->hasActiveFocus());

    QQuickItem *button2 = findItem<QQuickItem>(window->rootObject(), "button2");
    QVERIFY(button2);
    QVERIFY(!button2->hasActiveFocus());

    // Tab: contentItem->button1
    QKeyEvent key(QEvent::KeyPress, Qt::Key_Tab, Qt::NoModifier, "", false, 1);
    QGuiApplication::sendEvent(window, &key);
    QVERIFY(key.isAccepted());

    QVERIFY(button1->hasActiveFocus());

    // Tab: button1->button2
    key = QKeyEvent(QEvent::KeyPress, Qt::Key_Tab, Qt::NoModifier, "", false, 1);
    QGuiApplication::sendEvent(window, &key);
    QVERIFY(key.isAccepted());

    QVERIFY(button2->hasActiveFocus());
    QVERIFY(!button1->hasActiveFocus());

    // BackTab: button2->button1
    key = QKeyEvent(QEvent::KeyPress, Qt::Key_Tab, Qt::ShiftModifier, "", false, 1);
    QGuiApplication::sendEvent(window, &key);
    QVERIFY(key.isAccepted());

    QVERIFY(button1->hasActiveFocus());
    QVERIFY(!button2->hasActiveFocus());

    delete window;
}

void tst_QQuickItem::activeFocusOnTab9()
{
    if (qt_tab_all_widgets())
        QSKIP("This function doesn't support iterating all.");

    QQuickView *window = new QQuickView(0);
    window->setBaseSize(QSize(300,300));

    window->setSource(testFileUrl("activeFocusOnTab9.qml"));
    window->show();
    window->requestActivate();
    QVERIFY(QTest::qWaitForWindowActive(window));
    QCOMPARE(QGuiApplication::focusWindow(), window);

    QQuickItem *content = window->contentItem();
    QVERIFY(content);
    QVERIFY(content->hasActiveFocus());

    QQuickItem *textinput1 = findItem<QQuickItem>(window->rootObject(), "textinput1");
    QVERIFY(textinput1);
    QQuickItem *textedit1 = findItem<QQuickItem>(window->rootObject(), "textedit1");
    QVERIFY(textedit1);

    QVERIFY(!textinput1->hasActiveFocus());
    textinput1->forceActiveFocus();
    QVERIFY(textinput1->hasActiveFocus());

    // Tab: textinput1->textedit1
    QKeyEvent key(QEvent::KeyPress, Qt::Key_Tab, Qt::NoModifier, "", false, 1);
    QGuiApplication::sendEvent(window, &key);
    QVERIFY(key.isAccepted());

    QVERIFY(textedit1->hasActiveFocus());

    // BackTab: textedit1->textinput1
    key = QKeyEvent(QEvent::KeyPress, Qt::Key_Tab, Qt::ShiftModifier, "", false, 1);
    QGuiApplication::sendEvent(window, &key);
    QVERIFY(key.isAccepted());

    QVERIFY(textinput1->hasActiveFocus());

    // BackTab: textinput1->textedit1
    key = QKeyEvent(QEvent::KeyPress, Qt::Key_Tab, Qt::ShiftModifier, "", false, 1);
    QGuiApplication::sendEvent(window, &key);
    QVERIFY(key.isAccepted());

    QVERIFY(textedit1->hasActiveFocus());

    delete window;
}

void tst_QQuickItem::activeFocusOnTab10()
{
    if (!qt_tab_all_widgets())
        QSKIP("This function doesn't support NOT iterating all.");

    QQuickView *window = new QQuickView(0);
    window->setBaseSize(QSize(300,300));

    window->setSource(testFileUrl("activeFocusOnTab9.qml"));
    window->show();
    window->requestActivate();
    QVERIFY(QTest::qWaitForWindowActive(window));
    QCOMPARE(QGuiApplication::focusWindow(), window);

    QQuickItem *content = window->contentItem();
    QVERIFY(content);
    QVERIFY(content->hasActiveFocus());

    QQuickItem *textinput1 = findItem<QQuickItem>(window->rootObject(), "textinput1");
    QVERIFY(textinput1);
    QQuickItem *textedit1 = findItem<QQuickItem>(window->rootObject(), "textedit1");
    QVERIFY(textedit1);
    QQuickItem *textinput2 = findItem<QQuickItem>(window->rootObject(), "textinput2");
    QVERIFY(textinput2);
    QQuickItem *textedit2 = findItem<QQuickItem>(window->rootObject(), "textedit2");
    QVERIFY(textedit2);

    QVERIFY(!textinput1->hasActiveFocus());
    textinput1->forceActiveFocus();
    QVERIFY(textinput1->hasActiveFocus());

    // Tab: textinput1->textinput2
    QKeyEvent key(QEvent::KeyPress, Qt::Key_Tab, Qt::NoModifier, "", false, 1);
    QGuiApplication::sendEvent(window, &key);
    QVERIFY(key.isAccepted());

    QVERIFY(textinput2->hasActiveFocus());

    // Tab: textinput2->textedit1
    key = QKeyEvent(QEvent::KeyPress, Qt::Key_Tab, Qt::NoModifier, "", false, 1);
    QGuiApplication::sendEvent(window, &key);
    QVERIFY(key.isAccepted());

    QVERIFY(textedit1->hasActiveFocus());

    // BackTab: textedit1->textinput2
    key = QKeyEvent(QEvent::KeyPress, Qt::Key_Tab, Qt::ShiftModifier, "", false, 1);
    QGuiApplication::sendEvent(window, &key);
    QVERIFY(key.isAccepted());

    QVERIFY(textinput2->hasActiveFocus());

    // BackTab: textinput2->textinput1
    key = QKeyEvent(QEvent::KeyPress, Qt::Key_Tab, Qt::ShiftModifier, "", false, 1);
    QGuiApplication::sendEvent(window, &key);
    QVERIFY(key.isAccepted());

    QVERIFY(textinput1->hasActiveFocus());

    // BackTab: textinput1->textedit2
    key = QKeyEvent(QEvent::KeyPress, Qt::Key_Tab, Qt::ShiftModifier, "", false, 1);
    QGuiApplication::sendEvent(window, &key);
    QVERIFY(key.isAccepted());

    QVERIFY(textedit2->hasActiveFocus());

    // BackTab: textedit2->textedit1
    key = QKeyEvent(QEvent::KeyPress, Qt::Key_Tab, Qt::ShiftModifier, "", false, 1);
    QGuiApplication::sendEvent(window, &key);
    QVERIFY(key.isAccepted());

    QVERIFY(textedit1->hasActiveFocus());

    // BackTab: textedit1->textinput2
    key = QKeyEvent(QEvent::KeyPress, Qt::Key_Tab, Qt::ShiftModifier, "", false, 1);
    QGuiApplication::sendEvent(window, &key);
    QVERIFY(key.isAccepted());

    QVERIFY(textinput2->hasActiveFocus());

    delete window;
}

void tst_QQuickItem::nextItemInFocusChain()
{
    if (!qt_tab_all_widgets())
        QSKIP("This function doesn't support NOT iterating all.");

    QQuickView *window = new QQuickView(0);
    window->setBaseSize(QSize(800,600));

    window->setSource(testFileUrl("activeFocusOnTab.qml"));
    window->show();
    window->requestActivate();
    QVERIFY(QTest::qWaitForWindowActive(window));
    QCOMPARE(QGuiApplication::focusWindow(), window);

    QQuickItem *button11 = findItem<QQuickItem>(window->rootObject(), "button11");
    QVERIFY(button11);
    QQuickItem *button12 = findItem<QQuickItem>(window->rootObject(), "button12");
    QVERIFY(button12);

    QQuickItem *sub2 = findItem<QQuickItem>(window->rootObject(), "sub2");
    QVERIFY(sub2);
    QQuickItem *button21 = findItem<QQuickItem>(window->rootObject(), "button21");
    QVERIFY(button21);
    QQuickItem *button22 = findItem<QQuickItem>(window->rootObject(), "button22");
    QVERIFY(button22);

    QQuickItem *edit = findItem<QQuickItem>(window->rootObject(), "edit");
    QVERIFY(edit);

    QQuickItem *next, *prev;

    next = button11->nextItemInFocusChain(true);
    QVERIFY(next);
    QCOMPARE(next, button12);
    prev = button11->nextItemInFocusChain(false);
    QVERIFY(prev);
    QCOMPARE(prev, edit);

    next = button12->nextItemInFocusChain();
    QVERIFY(next);
    QCOMPARE(next, sub2);
    prev = button12->nextItemInFocusChain(false);
    QVERIFY(prev);
    QCOMPARE(prev, button11);

    next = sub2->nextItemInFocusChain(true);
    QVERIFY(next);
    QCOMPARE(next, button21);
    prev = sub2->nextItemInFocusChain(false);
    QVERIFY(prev);
    QCOMPARE(prev, button12);

    next = button21->nextItemInFocusChain();
    QVERIFY(next);
    QCOMPARE(next, button22);
    prev = button21->nextItemInFocusChain(false);
    QVERIFY(prev);
    QCOMPARE(prev, sub2);

    next = button22->nextItemInFocusChain(true);
    QVERIFY(next);
    QCOMPARE(next, edit);
    prev = button22->nextItemInFocusChain(false);
    QVERIFY(prev);
    QCOMPARE(prev, button21);

    next = edit->nextItemInFocusChain();
    QVERIFY(next);
    QCOMPARE(next, button11);
    prev = edit->nextItemInFocusChain(false);
    QVERIFY(prev);
    QCOMPARE(prev, button22);

    delete window;
}

void tst_QQuickItem::nextItemInFocusChain2()
{
    if (qt_tab_all_widgets())
        QSKIP("This function doesn't support iterating all.");

    QQuickView *window = new QQuickView(0);
    window->setBaseSize(QSize(800,600));

    window->setSource(testFileUrl("activeFocusOnTab6.qml"));
    window->show();
    window->requestActivate();
    QVERIFY(QTest::qWaitForWindowActive(window));
    QCOMPARE(QGuiApplication::focusWindow(), window);

    QQuickItem *button11 = findItem<QQuickItem>(window->rootObject(), "button11");
    QVERIFY(button11);
    QQuickItem *button12 = findItem<QQuickItem>(window->rootObject(), "button12");
    QVERIFY(button12);

    QQuickItem *edit = findItem<QQuickItem>(window->rootObject(), "edit");
    QVERIFY(edit);

    QQuickItem *next, *prev;

    next = button11->nextItemInFocusChain(true);
    QVERIFY(next);
    QCOMPARE(next, button12);
    prev = button11->nextItemInFocusChain(false);
    QVERIFY(prev);
    QCOMPARE(prev, edit);

    next = button12->nextItemInFocusChain();
    QVERIFY(next);
    QCOMPARE(next, edit);
    prev = button12->nextItemInFocusChain(false);
    QVERIFY(prev);
    QCOMPARE(prev, button11);

    next = edit->nextItemInFocusChain();
    QVERIFY(next);
    QCOMPARE(next, button11);
    prev = edit->nextItemInFocusChain(false);
    QVERIFY(prev);
    QCOMPARE(prev, button12);

    delete window;
}

void tst_QQuickItem::nextItemInFocusChain3()
{
    QQuickView *window = new QQuickView(0);
    window->setBaseSize(QSize(800,600));

    window->setSource(testFileUrl("nextItemInFocusChain3.qml"));
    window->show();
    window->requestActivate();
    QVERIFY(QTest::qWaitForWindowActive(window));
    QCOMPARE(QGuiApplication::focusWindow(), window);
}

void verifyTabFocusChain(QQuickView *window, const char **focusChain, bool forward)
{
    int idx = 0;
    for (const char **objectName = focusChain; *objectName; ++objectName, ++idx) {
        const QString &descrStr = QString("idx=%1 objectName=\"%2\"").arg(idx).arg(*objectName);
        const char *descr = descrStr.toLocal8Bit().data();
        QKeyEvent key(QEvent::KeyPress, Qt::Key_Tab, forward ? Qt::NoModifier : Qt::ShiftModifier);
        QGuiApplication::sendEvent(window, &key);
        QVERIFY2(key.isAccepted(), descr);

        QQuickItem *item = findItem<QQuickItem>(window->rootObject(), *objectName);
        QVERIFY2(item, descr);
        QVERIFY2(item->hasActiveFocus(), descr);
    }
}

void tst_QQuickItem::tabFence()
{
    QQuickView *window = new QQuickView(0);
    window->setBaseSize(QSize(800,600));

    window->setSource(testFileUrl("tabFence.qml"));
    window->show();
    window->requestActivate();
    QVERIFY(QTest::qWaitForWindowActive(window));
    QVERIFY(QGuiApplication::focusWindow() == window);
    QVERIFY(window->rootObject()->hasActiveFocus());

    const char *rootTabFocusChain[] = {
          "input1", "input2", "input3", "input1", Q_NULLPTR
    };
    verifyTabFocusChain(window, rootTabFocusChain, true /* forward */);

    const char *rootBacktabFocusChain[] = {
          "input3", "input2", "input1", "input3", Q_NULLPTR
    };
    verifyTabFocusChain(window, rootBacktabFocusChain, false /* forward */);

    // Give focus to input11 in fence1
    QQuickItem *item = findItem<QQuickItem>(window->rootObject(), "input11");
    item->setFocus(true);
    QVERIFY(item);
    QVERIFY(item->hasActiveFocus());

    const char *fence1TabFocusChain[] = {
          "input12", "input13", "input11", "input12", Q_NULLPTR
    };
    verifyTabFocusChain(window, fence1TabFocusChain, true /* forward */);

    const char *fence1BacktabFocusChain[] = {
          "input11", "input13", "input12", "input11", Q_NULLPTR
    };
    verifyTabFocusChain(window, fence1BacktabFocusChain, false /* forward */);
}

void tst_QQuickItem::qtbug_50516()
{
    QQuickView *window = new QQuickView(0);
    window->setBaseSize(QSize(800,600));

    window->setSource(testFileUrl("qtbug_50516.qml"));
    window->show();
    window->requestActivate();
    QVERIFY(QTest::qWaitForWindowActive(window));
    QVERIFY(QGuiApplication::focusWindow() == window);
    QVERIFY(window->rootObject()->hasActiveFocus());

    QQuickItem *contentItem = window->rootObject();
    QQuickItem *next = contentItem->nextItemInFocusChain(true);
    QCOMPARE(next, contentItem);
    next = contentItem->nextItemInFocusChain(false);
    QCOMPARE(next, contentItem);

    delete window;
}

void tst_QQuickItem::qtbug_50516_2_data()
{
    QTest::addColumn<QString>("filename");
    QTest::addColumn<QString>("item1");
    QTest::addColumn<QString>("item2");

    QTest::newRow("FocusScope TabFence with one Item(focused)")
            << QStringLiteral("qtbug_50516_2_1.qml") << QStringLiteral("root") << QStringLiteral("root");
    QTest::newRow("FocusScope TabFence with one Item(unfocused)")
            << QStringLiteral("qtbug_50516_2_2.qml") << QStringLiteral("root") << QStringLiteral("root");
    QTest::newRow("FocusScope TabFence with two Items(focused)")
            << QStringLiteral("qtbug_50516_2_3.qml") << QStringLiteral("root") << QStringLiteral("root");
    QTest::newRow("FocusScope TabFence with two Items(unfocused)")
            << QStringLiteral("qtbug_50516_2_4.qml") << QStringLiteral("root") << QStringLiteral("root");
    QTest::newRow("FocusScope TabFence with one Item and one TextInput(unfocused)")
            << QStringLiteral("qtbug_50516_2_5.qml") << QStringLiteral("item1") << QStringLiteral("item1");
    QTest::newRow("FocusScope TabFence with two TextInputs(unfocused)")
            << QStringLiteral("qtbug_50516_2_6.qml") << QStringLiteral("item1") << QStringLiteral("item2");
}

void tst_QQuickItem::qtbug_50516_2()
{
    QFETCH(QString, filename);
    QFETCH(QString, item1);
    QFETCH(QString, item2);

    QQuickView *window = new QQuickView(0);
    window->setBaseSize(QSize(800,600));

    window->setSource(testFileUrl(filename));
    window->show();
    window->requestActivate();
    QVERIFY(QTest::qWaitForWindowActive(window));
    QVERIFY(QGuiApplication::focusWindow() == window);
    QVERIFY(window->rootObject()->hasActiveFocus());

    QQuickItem *contentItem = window->rootObject();
    QQuickItem *next = contentItem->nextItemInFocusChain(true);
    QCOMPARE(next->objectName(), item1);
    next = contentItem->nextItemInFocusChain(false);
    QCOMPARE(next->objectName(), item2);

    delete window;
}

void tst_QQuickItem::keys()
{
    QQuickView *window = new QQuickView(0);
    window->setBaseSize(QSize(240,320));

    KeysTestObject *testObject = new KeysTestObject;
    window->rootContext()->setContextProperty("keysTestObject", testObject);

    window->rootContext()->setContextProperty("enableKeyHanding", QVariant(true));
    window->rootContext()->setContextProperty("forwardeeVisible", QVariant(true));

    window->setSource(testFileUrl("keystest.qml"));
    window->show();
    window->requestActivate();
    QVERIFY(QTest::qWaitForWindowActive(window));
    QCOMPARE(QGuiApplication::focusWindow(), window);

    QVERIFY(window->rootObject());
    QCOMPARE(window->rootObject()->property("isEnabled").toBool(), true);

    QKeyEvent key(QEvent::KeyPress, Qt::Key_A, Qt::NoModifier, "A", false, 1);
    QGuiApplication::sendEvent(window, &key);
    QCOMPARE(testObject->mKey, int(Qt::Key_A));
    QCOMPARE(testObject->mForwardedKey, int(Qt::Key_A));
    QCOMPARE(testObject->mText, QLatin1String("A"));
    QCOMPARE(testObject->mModifiers, int(Qt::NoModifier));
    QVERIFY(!key.isAccepted());

    testObject->reset();

    key = QKeyEvent(QEvent::KeyRelease, Qt::Key_A, Qt::ShiftModifier, "A", false, 1);
    QGuiApplication::sendEvent(window, &key);
    QCOMPARE(testObject->mKey, int(Qt::Key_A));
    QCOMPARE(testObject->mForwardedKey, int(Qt::Key_A));
    QCOMPARE(testObject->mText, QLatin1String("A"));
    QCOMPARE(testObject->mModifiers, int(Qt::ShiftModifier));
    QVERIFY(key.isAccepted());

    testObject->reset();

    key = QKeyEvent(QEvent::KeyPress, Qt::Key_Return, Qt::NoModifier, "", false, 1);
    QGuiApplication::sendEvent(window, &key);
    QCOMPARE(testObject->mKey, int(Qt::Key_Return));
    QCOMPARE(testObject->mForwardedKey, int(Qt::Key_Return));
    QCOMPARE(testObject->mText, QLatin1String("Return"));
    QCOMPARE(testObject->mModifiers, int(Qt::NoModifier));
    QVERIFY(key.isAccepted());

    testObject->reset();

    key = QKeyEvent(QEvent::KeyPress, Qt::Key_0, Qt::NoModifier, "0", false, 1);
    QGuiApplication::sendEvent(window, &key);
    QCOMPARE(testObject->mKey, int(Qt::Key_0));
    QCOMPARE(testObject->mForwardedKey, int(Qt::Key_0));
    QCOMPARE(testObject->mText, QLatin1String("0"));
    QCOMPARE(testObject->mModifiers, int(Qt::NoModifier));
    QVERIFY(key.isAccepted());

    testObject->reset();

    key = QKeyEvent(QEvent::KeyPress, Qt::Key_9, Qt::NoModifier, "9", false, 1);
    QGuiApplication::sendEvent(window, &key);
    QCOMPARE(testObject->mKey, int(Qt::Key_9));
    QCOMPARE(testObject->mForwardedKey, int(Qt::Key_9));
    QCOMPARE(testObject->mText, QLatin1String("9"));
    QCOMPARE(testObject->mModifiers, int(Qt::NoModifier));
    QVERIFY(!key.isAccepted());

    testObject->reset();

    key = QKeyEvent(QEvent::KeyPress, Qt::Key_Tab, Qt::NoModifier, "", false, 1);
    QGuiApplication::sendEvent(window, &key);
    QCOMPARE(testObject->mKey, int(Qt::Key_Tab));
    QCOMPARE(testObject->mForwardedKey, int(Qt::Key_Tab));
    QCOMPARE(testObject->mText, QLatin1String("Tab"));
    QCOMPARE(testObject->mModifiers, int(Qt::NoModifier));
    QVERIFY(key.isAccepted());

    testObject->reset();

    key = QKeyEvent(QEvent::KeyPress, Qt::Key_Backtab, Qt::NoModifier, "", false, 1);
    QGuiApplication::sendEvent(window, &key);
    QCOMPARE(testObject->mKey, int(Qt::Key_Backtab));
    QCOMPARE(testObject->mForwardedKey, int(Qt::Key_Backtab));
    QCOMPARE(testObject->mText, QLatin1String("Backtab"));
    QCOMPARE(testObject->mModifiers, int(Qt::NoModifier));
    QVERIFY(key.isAccepted());

    testObject->reset();

    key = QKeyEvent(QEvent::KeyPress, Qt::Key_VolumeUp, Qt::NoModifier, 1234, 0, 0);
    QGuiApplication::sendEvent(window, &key);
    QCOMPARE(testObject->mKey, int(Qt::Key_VolumeUp));
    QCOMPARE(testObject->mForwardedKey, int(Qt::Key_VolumeUp));
    QCOMPARE(testObject->mModifiers, int(Qt::NoModifier));
    QCOMPARE(testObject->mNativeScanCode, quint32(1234));
    QVERIFY(key.isAccepted());

    testObject->reset();

    window->rootContext()->setContextProperty("forwardeeVisible", QVariant(false));
    key = QKeyEvent(QEvent::KeyPress, Qt::Key_A, Qt::NoModifier, "A", false, 1);
    QGuiApplication::sendEvent(window, &key);
    QCOMPARE(testObject->mKey, int(Qt::Key_A));
    QCOMPARE(testObject->mForwardedKey, 0);
    QCOMPARE(testObject->mText, QLatin1String("A"));
    QCOMPARE(testObject->mModifiers, int(Qt::NoModifier));
    QVERIFY(!key.isAccepted());

    testObject->reset();

    window->rootContext()->setContextProperty("enableKeyHanding", QVariant(false));
    QCOMPARE(window->rootObject()->property("isEnabled").toBool(), false);

    key = QKeyEvent(QEvent::KeyPress, Qt::Key_Return, Qt::NoModifier, "", false, 1);
    QGuiApplication::sendEvent(window, &key);
    QCOMPARE(testObject->mKey, 0);
    QVERIFY(!key.isAccepted());

    window->rootContext()->setContextProperty("enableKeyHanding", QVariant(true));
    QCOMPARE(window->rootObject()->property("isEnabled").toBool(), true);

    key = QKeyEvent(QEvent::KeyPress, Qt::Key_Return, Qt::NoModifier, "", false, 1);
    QGuiApplication::sendEvent(window, &key);
    QCOMPARE(testObject->mKey, int(Qt::Key_Return));
    QVERIFY(key.isAccepted());

    delete window;
    delete testObject;
}

Q_DECLARE_METATYPE(QEvent::Type);
Q_DECLARE_METATYPE(QKeySequence::StandardKey);

void tst_QQuickItem::standardKeys_data()
{
    QTest::addColumn<QKeySequence::StandardKey>("standardKey");
    QTest::addColumn<QKeySequence::StandardKey>("contextProperty");
    QTest::addColumn<QEvent::Type>("eventType");
    QTest::addColumn<bool>("pressed");
    QTest::addColumn<bool>("released");

    QTest::newRow("Press: Open") << QKeySequence::Open << QKeySequence::Open << QEvent::KeyPress << true << false;
    QTest::newRow("Press: Close") << QKeySequence::Close << QKeySequence::Close << QEvent::KeyPress << true << false;
    QTest::newRow("Press: Save") << QKeySequence::Save << QKeySequence::Save << QEvent::KeyPress << true << false;
    QTest::newRow("Press: Quit") << QKeySequence::Quit << QKeySequence::Quit << QEvent::KeyPress << true << false;

    QTest::newRow("Release: New") << QKeySequence::New << QKeySequence::New << QEvent::KeyRelease << false << true;
    QTest::newRow("Release: Delete") << QKeySequence::Delete << QKeySequence::Delete << QEvent::KeyRelease << false << true;
    QTest::newRow("Release: Undo") << QKeySequence::Undo << QKeySequence::Undo << QEvent::KeyRelease << false << true;
    QTest::newRow("Release: Redo") << QKeySequence::Redo << QKeySequence::Redo << QEvent::KeyRelease << false << true;

    QTest::newRow("Mismatch: Cut") << QKeySequence::Cut << QKeySequence::Copy << QEvent::KeyPress << false << false;
    QTest::newRow("Mismatch: Copy") << QKeySequence::Copy << QKeySequence::Paste << QEvent::KeyPress << false << false;
    QTest::newRow("Mismatch: Paste") << QKeySequence::Paste << QKeySequence::Cut << QEvent::KeyRelease << false << false;
    QTest::newRow("Mismatch: Quit") << QKeySequence::Quit << QKeySequence::New << QEvent::KeyRelease << false << false;
}

void tst_QQuickItem::standardKeys()
{
    QFETCH(QKeySequence::StandardKey, standardKey);
    QFETCH(QKeySequence::StandardKey, contextProperty);
    QFETCH(QEvent::Type, eventType);
    QFETCH(bool, pressed);
    QFETCH(bool, released);

    QKeySequence keySequence(standardKey);
    if (keySequence.isEmpty())
        QSKIP("Undefined key sequence.");

    QQuickView view;
    view.rootContext()->setContextProperty("standardKey", contextProperty);
    view.setSource(testFileUrl("standardkeys.qml"));
    view.show();
    view.requestActivate();
    QVERIFY(QTest::qWaitForWindowActive(&view));

    QQuickItem *item = qobject_cast<QQuickItem*>(view.rootObject());
    QVERIFY(item);

    const int key = keySequence[0] & Qt::Key_unknown;
    const int modifiers = keySequence[0] & Qt::KeyboardModifierMask;
    QKeyEvent keyEvent(eventType, key, static_cast<Qt::KeyboardModifiers>(modifiers));
    QGuiApplication::sendEvent(&view, &keyEvent);

    QCOMPARE(item->property("pressed").toBool(), pressed);
    QCOMPARE(item->property("released").toBool(), released);
}

void tst_QQuickItem::keysProcessingOrder()
{
    QQuickView *window = new QQuickView(0);
    window->setBaseSize(QSize(240,320));

    KeysTestObject *testObject = new KeysTestObject;
    window->rootContext()->setContextProperty("keysTestObject", testObject);

    window->setSource(testFileUrl("keyspriority.qml"));
    window->show();
    window->requestActivate();
    QVERIFY(QTest::qWaitForWindowActive(window));
    QCOMPARE(QGuiApplication::focusWindow(), window);

    KeyTestItem *testItem = qobject_cast<KeyTestItem*>(window->rootObject());
    QVERIFY(testItem);

    QCOMPARE(testItem->property("priorityTest").toInt(), 0);

    QKeyEvent key(QEvent::KeyPress, Qt::Key_A, Qt::NoModifier, "A", false, 1);
    QGuiApplication::sendEvent(window, &key);
    QCOMPARE(testObject->mKey, int(Qt::Key_A));
    QCOMPARE(testObject->mText, QLatin1String("A"));
    QCOMPARE(testObject->mModifiers, int(Qt::NoModifier));
    QVERIFY(key.isAccepted());

    testObject->reset();

    testObject->setProcessLast(true);

    QCOMPARE(testItem->property("priorityTest").toInt(), 1);

    key = QKeyEvent(QEvent::KeyPress, Qt::Key_A, Qt::NoModifier, "A", false, 1);
    QGuiApplication::sendEvent(window, &key);
    QCOMPARE(testObject->mKey, 0);
    QVERIFY(key.isAccepted());

    testObject->reset();

    key = QKeyEvent(QEvent::KeyPress, Qt::Key_B, Qt::NoModifier, "B", false, 1);
    QGuiApplication::sendEvent(window, &key);
    QCOMPARE(testObject->mKey, int(Qt::Key_B));
    QCOMPARE(testObject->mText, QLatin1String("B"));
    QCOMPARE(testObject->mModifiers, int(Qt::NoModifier));
    QVERIFY(!key.isAccepted());

    testObject->reset();

    key = QKeyEvent(QEvent::KeyRelease, Qt::Key_B, Qt::NoModifier, "B", false, 1);
    QGuiApplication::sendEvent(window, &key);
    QCOMPARE(testObject->mKey, 0);
    QVERIFY(key.isAccepted());

    delete window;
    delete testObject;
}

void tst_QQuickItem::keysim()
{
    PlatformInputContext platformInputContext;
    QInputMethodPrivate *inputMethodPrivate = QInputMethodPrivate::get(qApp->inputMethod());
    inputMethodPrivate->testContext = &platformInputContext;

    QQuickView *window = new QQuickView(0);
    window->setBaseSize(QSize(240,320));

    window->setSource(testFileUrl("keysim.qml"));
    window->show();
    window->requestActivate();
    QVERIFY(QTest::qWaitForWindowActive(window));
    QCOMPARE(QGuiApplication::focusWindow(), window);

    QVERIFY(window->rootObject());
    QVERIFY(window->rootObject()->hasFocus() && window->rootObject()->hasActiveFocus());

    QQuickTextInput *input = window->rootObject()->findChild<QQuickTextInput*>();
    QVERIFY(input);

    QInputMethodEvent ev("Hello world!", QList<QInputMethodEvent::Attribute>());
    QGuiApplication::sendEvent(qGuiApp->focusObject(), &ev);

    QEXPECT_FAIL("", "QTBUG-24280", Continue);
    QCOMPARE(input->text(), QLatin1String("Hello world!"));

    delete window;
}

void tst_QQuickItem::keysForward()
{
    QQuickView window;
    window.setBaseSize(QSize(240,320));

    window.setSource(testFileUrl("keysforward.qml"));
    window.show();
    window.requestActivate();
    QVERIFY(QTest::qWaitForWindowActive(&window));
    QCOMPARE(QGuiApplication::focusWindow(), &window);

    QQuickItem *rootItem = qobject_cast<QQuickItem *>(window.rootObject());
    QVERIFY(rootItem);
    QQuickItem *sourceItem = rootItem->property("source").value<QQuickItem *>();
    QVERIFY(sourceItem);
    QQuickItem *primaryTarget = rootItem->property("primaryTarget").value<QQuickItem *>();
    QVERIFY(primaryTarget);
    QQuickItem *secondaryTarget = rootItem->property("secondaryTarget").value<QQuickItem *>();
    QVERIFY(secondaryTarget);

    // primary target accepts/consumes Key_P
    QKeyEvent pressKeyP(QEvent::KeyPress, Qt::Key_P, Qt::NoModifier, "P");
    QCoreApplication::sendEvent(sourceItem, &pressKeyP);
    QCOMPARE(rootItem->property("pressedKeys").toList(), QVariantList());
    QCOMPARE(sourceItem->property("pressedKeys").toList(), QVariantList());
    QCOMPARE(primaryTarget->property("pressedKeys").toList(), QVariantList() << Qt::Key_P);
    QCOMPARE(secondaryTarget->property("pressedKeys").toList(), QVariantList() << Qt::Key_P);
    QVERIFY(pressKeyP.isAccepted());

    QKeyEvent releaseKeyP(QEvent::KeyRelease, Qt::Key_P, Qt::NoModifier, "P");
    QCoreApplication::sendEvent(sourceItem, &releaseKeyP);
    QCOMPARE(rootItem->property("releasedKeys").toList(), QVariantList());
    QCOMPARE(sourceItem->property("releasedKeys").toList(), QVariantList());
    QCOMPARE(primaryTarget->property("releasedKeys").toList(), QVariantList() << Qt::Key_P);
    QCOMPARE(secondaryTarget->property("releasedKeys").toList(), QVariantList() << Qt::Key_P);
    QVERIFY(releaseKeyP.isAccepted());

    // secondary target accepts/consumes Key_S
    QKeyEvent pressKeyS(QEvent::KeyPress, Qt::Key_S, Qt::NoModifier, "S");
    QCoreApplication::sendEvent(sourceItem, &pressKeyS);
    QCOMPARE(rootItem->property("pressedKeys").toList(), QVariantList());
    QCOMPARE(sourceItem->property("pressedKeys").toList(), QVariantList());
    QCOMPARE(primaryTarget->property("pressedKeys").toList(), QVariantList() << Qt::Key_P);
    QCOMPARE(secondaryTarget->property("pressedKeys").toList(), QVariantList() << Qt::Key_P << Qt::Key_S);
    QVERIFY(pressKeyS.isAccepted());

    QKeyEvent releaseKeyS(QEvent::KeyRelease, Qt::Key_S, Qt::NoModifier, "S");
    QCoreApplication::sendEvent(sourceItem, &releaseKeyS);
    QCOMPARE(rootItem->property("releasedKeys").toList(), QVariantList());
    QCOMPARE(sourceItem->property("releasedKeys").toList(), QVariantList());
    QCOMPARE(primaryTarget->property("releasedKeys").toList(), QVariantList() << Qt::Key_P);
    QCOMPARE(secondaryTarget->property("releasedKeys").toList(), QVariantList() << Qt::Key_P << Qt::Key_S);
    QVERIFY(releaseKeyS.isAccepted());

    // neither target accepts/consumes Key_Q
    QKeyEvent pressKeyQ(QEvent::KeyPress, Qt::Key_Q, Qt::NoModifier, "Q");
    QCoreApplication::sendEvent(sourceItem, &pressKeyQ);
    QCOMPARE(rootItem->property("pressedKeys").toList(), QVariantList());
    QCOMPARE(sourceItem->property("pressedKeys").toList(), QVariantList() << Qt::Key_Q);
    QCOMPARE(primaryTarget->property("pressedKeys").toList(), QVariantList() << Qt::Key_P << Qt::Key_Q);
    QCOMPARE(secondaryTarget->property("pressedKeys").toList(), QVariantList() << Qt::Key_P << Qt::Key_S << Qt::Key_Q);
    QVERIFY(!pressKeyQ.isAccepted());

    QKeyEvent releaseKeyQ(QEvent::KeyRelease, Qt::Key_Q, Qt::NoModifier, "Q");
    QCoreApplication::sendEvent(sourceItem, &releaseKeyQ);
    QCOMPARE(rootItem->property("releasedKeys").toList(), QVariantList());
    QCOMPARE(sourceItem->property("releasedKeys").toList(), QVariantList() << Qt::Key_Q);
    QCOMPARE(primaryTarget->property("releasedKeys").toList(), QVariantList() << Qt::Key_P << Qt::Key_Q);
    QCOMPARE(secondaryTarget->property("releasedKeys").toList(), QVariantList() << Qt::Key_P << Qt::Key_S << Qt::Key_Q);
    QVERIFY(!releaseKeyQ.isAccepted());
}

QQuickItemPrivate *childPrivate(QQuickItem *rootItem, const char * itemString)
{
    QQuickItem *item = findItem<QQuickItem>(rootItem, QString(QLatin1String(itemString)));
    QQuickItemPrivate* itemPrivate = QQuickItemPrivate::get(item);
    return itemPrivate;
}

QVariant childProperty(QQuickItem *rootItem, const char * itemString, const char * property)
{
    QQuickItem *item = findItem<QQuickItem>(rootItem, QString(QLatin1String(itemString)));
    return item->property(property);
}

bool anchorsMirrored(QQuickItem *rootItem, const char * itemString)
{
    QQuickItem *item = findItem<QQuickItem>(rootItem, QString(QLatin1String(itemString)));
    QQuickItemPrivate* itemPrivate = QQuickItemPrivate::get(item);
    return itemPrivate->anchors()->mirrored();
}

void tst_QQuickItem::layoutMirroring()
{
    QQuickView *window = new QQuickView(0);
    window->setSource(testFileUrl("layoutmirroring.qml"));
    window->show();

    QQuickItem *rootItem = qobject_cast<QQuickItem*>(window->rootObject());
    QVERIFY(rootItem);
    QQuickItemPrivate *rootPrivate = QQuickItemPrivate::get(rootItem);
    QVERIFY(rootPrivate);

    QCOMPARE(childPrivate(rootItem, "mirrored1")->effectiveLayoutMirror, true);
    QCOMPARE(childPrivate(rootItem, "mirrored2")->effectiveLayoutMirror, true);
    QCOMPARE(childPrivate(rootItem, "notMirrored1")->effectiveLayoutMirror, false);
    QCOMPARE(childPrivate(rootItem, "notMirrored2")->effectiveLayoutMirror, false);
    QCOMPARE(childPrivate(rootItem, "inheritedMirror1")->effectiveLayoutMirror, true);
    QCOMPARE(childPrivate(rootItem, "inheritedMirror2")->effectiveLayoutMirror, true);

    QCOMPARE(anchorsMirrored(rootItem, "mirrored1"), true);
    QCOMPARE(anchorsMirrored(rootItem, "mirrored2"), true);
    QCOMPARE(anchorsMirrored(rootItem, "notMirrored1"), false);
    QCOMPARE(anchorsMirrored(rootItem, "notMirrored2"), false);
    QCOMPARE(anchorsMirrored(rootItem, "inheritedMirror1"), true);
    QCOMPARE(anchorsMirrored(rootItem, "inheritedMirror2"), true);

    QCOMPARE(childPrivate(rootItem, "mirrored1")->inheritedLayoutMirror, true);
    QCOMPARE(childPrivate(rootItem, "mirrored2")->inheritedLayoutMirror, false);
    QCOMPARE(childPrivate(rootItem, "notMirrored1")->inheritedLayoutMirror, true);
    QCOMPARE(childPrivate(rootItem, "notMirrored2")->inheritedLayoutMirror, false);
    QCOMPARE(childPrivate(rootItem, "inheritedMirror1")->inheritedLayoutMirror, true);
    QCOMPARE(childPrivate(rootItem, "inheritedMirror2")->inheritedLayoutMirror, true);

    QCOMPARE(childPrivate(rootItem, "mirrored1")->isMirrorImplicit, false);
    QCOMPARE(childPrivate(rootItem, "mirrored2")->isMirrorImplicit, false);
    QCOMPARE(childPrivate(rootItem, "notMirrored1")->isMirrorImplicit, false);
    QCOMPARE(childPrivate(rootItem, "notMirrored2")->isMirrorImplicit, true);
    QCOMPARE(childPrivate(rootItem, "inheritedMirror1")->isMirrorImplicit, true);
    QCOMPARE(childPrivate(rootItem, "inheritedMirror2")->isMirrorImplicit, true);

    QCOMPARE(childPrivate(rootItem, "mirrored1")->inheritMirrorFromParent, true);
    QCOMPARE(childPrivate(rootItem, "mirrored2")->inheritMirrorFromParent, false);
    QCOMPARE(childPrivate(rootItem, "notMirrored1")->inheritMirrorFromParent, true);
    QCOMPARE(childPrivate(rootItem, "notMirrored2")->inheritMirrorFromParent, false);
    QCOMPARE(childPrivate(rootItem, "inheritedMirror1")->inheritMirrorFromParent, true);
    QCOMPARE(childPrivate(rootItem, "inheritedMirror2")->inheritMirrorFromParent, true);

    QCOMPARE(childPrivate(rootItem, "mirrored1")->inheritMirrorFromItem, true);
    QCOMPARE(childPrivate(rootItem, "mirrored2")->inheritMirrorFromItem, false);
    QCOMPARE(childPrivate(rootItem, "notMirrored1")->inheritMirrorFromItem, false);
    QCOMPARE(childPrivate(rootItem, "notMirrored2")->inheritMirrorFromItem, false);
    QCOMPARE(childPrivate(rootItem, "inheritedMirror1")->inheritMirrorFromItem, false);
    QCOMPARE(childPrivate(rootItem, "inheritedMirror2")->inheritMirrorFromItem, false);

    // load dynamic content using Loader that needs to inherit mirroring
    rootItem->setProperty("state", "newContent");
    QCOMPARE(childPrivate(rootItem, "notMirrored3")->effectiveLayoutMirror, false);
    QCOMPARE(childPrivate(rootItem, "inheritedMirror3")->effectiveLayoutMirror, true);

    QCOMPARE(childPrivate(rootItem, "notMirrored3")->inheritedLayoutMirror, true);
    QCOMPARE(childPrivate(rootItem, "inheritedMirror3")->inheritedLayoutMirror, true);

    QCOMPARE(childPrivate(rootItem, "notMirrored3")->isMirrorImplicit, false);
    QCOMPARE(childPrivate(rootItem, "inheritedMirror3")->isMirrorImplicit, true);

    QCOMPARE(childPrivate(rootItem, "notMirrored3")->inheritMirrorFromParent, true);
    QCOMPARE(childPrivate(rootItem, "inheritedMirror3")->inheritMirrorFromParent, true);

    QCOMPARE(childPrivate(rootItem, "notMirrored3")->inheritMirrorFromItem, false);
    QCOMPARE(childPrivate(rootItem, "notMirrored3")->inheritMirrorFromItem, false);

    // disable inheritance
    rootItem->setProperty("childrenInherit", false);

    QCOMPARE(childPrivate(rootItem, "inheritedMirror1")->effectiveLayoutMirror, false);
    QCOMPARE(childPrivate(rootItem, "inheritedMirror2")->effectiveLayoutMirror, false);
    QCOMPARE(childPrivate(rootItem, "mirrored1")->effectiveLayoutMirror, true);
    QCOMPARE(childPrivate(rootItem, "notMirrored1")->effectiveLayoutMirror, false);

    QCOMPARE(childPrivate(rootItem, "inheritedMirror1")->inheritedLayoutMirror, false);
    QCOMPARE(childPrivate(rootItem, "inheritedMirror2")->inheritedLayoutMirror, false);
    QCOMPARE(childPrivate(rootItem, "mirrored1")->inheritedLayoutMirror, false);
    QCOMPARE(childPrivate(rootItem, "notMirrored1")->inheritedLayoutMirror, false);

    // re-enable inheritance
    rootItem->setProperty("childrenInherit", true);

    QCOMPARE(childPrivate(rootItem, "inheritedMirror1")->effectiveLayoutMirror, true);
    QCOMPARE(childPrivate(rootItem, "inheritedMirror2")->effectiveLayoutMirror, true);
    QCOMPARE(childPrivate(rootItem, "mirrored1")->effectiveLayoutMirror, true);
    QCOMPARE(childPrivate(rootItem, "notMirrored1")->effectiveLayoutMirror, false);

    QCOMPARE(childPrivate(rootItem, "inheritedMirror1")->inheritedLayoutMirror, true);
    QCOMPARE(childPrivate(rootItem, "inheritedMirror2")->inheritedLayoutMirror, true);
    QCOMPARE(childPrivate(rootItem, "mirrored1")->inheritedLayoutMirror, true);
    QCOMPARE(childPrivate(rootItem, "notMirrored1")->inheritedLayoutMirror, true);

    //
    // dynamic parenting
    //
    QQuickItem *parentItem1 = new QQuickItem();
    QQuickItemPrivate::get(parentItem1)->effectiveLayoutMirror = true; // LayoutMirroring.enabled: true
    QQuickItemPrivate::get(parentItem1)->isMirrorImplicit = false;
    QQuickItemPrivate::get(parentItem1)->inheritMirrorFromItem = true; // LayoutMirroring.childrenInherit: true
    QQuickItemPrivate::get(parentItem1)->resolveLayoutMirror();

    // inherit in constructor
    QQuickItem *childItem1 = new QQuickItem(parentItem1);
    QCOMPARE(QQuickItemPrivate::get(childItem1)->effectiveLayoutMirror, true);
    QCOMPARE(QQuickItemPrivate::get(childItem1)->inheritMirrorFromParent, true);

    // inherit through a parent change
    QQuickItem *childItem2 = new QQuickItem();
    QCOMPARE(QQuickItemPrivate::get(childItem2)->effectiveLayoutMirror, false);
    QCOMPARE(QQuickItemPrivate::get(childItem2)->inheritMirrorFromParent, false);
    childItem2->setParentItem(parentItem1);
    QCOMPARE(QQuickItemPrivate::get(childItem2)->effectiveLayoutMirror, true);
    QCOMPARE(QQuickItemPrivate::get(childItem2)->inheritMirrorFromParent, true);

    // stop inherting through a parent change
    QQuickItem *parentItem2 = new QQuickItem();
    QQuickItemPrivate::get(parentItem2)->effectiveLayoutMirror = true; // LayoutMirroring.enabled: true
    QQuickItemPrivate::get(parentItem2)->resolveLayoutMirror();
    childItem2->setParentItem(parentItem2);
    QCOMPARE(QQuickItemPrivate::get(childItem2)->effectiveLayoutMirror, false);
    QCOMPARE(QQuickItemPrivate::get(childItem2)->inheritMirrorFromParent, false);

    delete parentItem1;
    delete parentItem2;
}

void tst_QQuickItem::layoutMirroringWindow()
{
    QQmlComponent component(&engine);
    component.loadUrl(testFileUrl("layoutmirroring_window.qml"));
    QScopedPointer<QObject> object(component.create());
    QQuickWindow *window = qobject_cast<QQuickWindow *>(object.data());
    QVERIFY(window);
    window->show();

    QQuickItemPrivate *content = QQuickItemPrivate::get(window->contentItem());
    QCOMPARE(content->effectiveLayoutMirror, true);
    QCOMPARE(content->inheritedLayoutMirror, true);
    QCOMPARE(content->isMirrorImplicit, false);
    QCOMPARE(content->inheritMirrorFromParent, true);
    QCOMPARE(content->inheritMirrorFromItem, true);
}

void tst_QQuickItem::layoutMirroringIllegalParent()
{
    QQmlComponent component(&engine);
    component.setData("import QtQuick 2.0; QtObject { LayoutMirroring.enabled: true; LayoutMirroring.childrenInherit: true }", QUrl::fromLocalFile(""));
    QTest::ignoreMessage(QtWarningMsg, "<Unknown File>:1:21: QML QtObject: LayoutDirection attached property only works with Items and Windows");
    QObject *object = component.create();
    QVERIFY(object != 0);
}

void tst_QQuickItem::keyNavigation_data()
{
    QTest::addColumn<QString>("source");
    QTest::newRow("KeyNavigation") << QStringLiteral("keynavigationtest.qml");
    QTest::newRow("KeyNavigation_FocusScope") << QStringLiteral("keynavigationtest_focusscope.qml");
}

void tst_QQuickItem::keyNavigation()
{
    QFETCH(QString, source);

    QQuickView *window = new QQuickView(0);
    window->setBaseSize(QSize(240,320));

    window->setSource(testFileUrl(source));
    window->show();
    window->requestActivate();
    QVERIFY(QTest::qWaitForWindowActive(window));
    QCOMPARE(QGuiApplication::focusWindow(), window);

    QQuickItem *item = findItem<QQuickItem>(window->rootObject(), "item1");
    QVERIFY(item);
    QVERIFY(item->hasActiveFocus());

    QVariant result;
    QVERIFY(QMetaObject::invokeMethod(window->rootObject(), "verify",
            Q_RETURN_ARG(QVariant, result)));
    QVERIFY(result.toBool());

    // right
    QKeyEvent key(QEvent::KeyPress, Qt::Key_Right, Qt::NoModifier, "", false, 1);
    QGuiApplication::sendEvent(window, &key);
    QVERIFY(key.isAccepted());

    item = findItem<QQuickItem>(window->rootObject(), "item2");
    QVERIFY(item);
    QVERIFY(item->hasActiveFocus());

    // down
    key = QKeyEvent(QEvent::KeyPress, Qt::Key_Down, Qt::NoModifier, "", false, 1);
    QGuiApplication::sendEvent(window, &key);
    QVERIFY(key.isAccepted());

    item = findItem<QQuickItem>(window->rootObject(), "item4");
    QVERIFY(item);
    QVERIFY(item->hasActiveFocus());

    // left
    key = QKeyEvent(QEvent::KeyPress, Qt::Key_Left, Qt::NoModifier, "", false, 1);
    QGuiApplication::sendEvent(window, &key);
    QVERIFY(key.isAccepted());

    item = findItem<QQuickItem>(window->rootObject(), "item3");
    QVERIFY(item);
    QVERIFY(item->hasActiveFocus());

    // up
    key = QKeyEvent(QEvent::KeyPress, Qt::Key_Up, Qt::NoModifier, "", false, 1);
    QGuiApplication::sendEvent(window, &key);
    QVERIFY(key.isAccepted());

    item = findItem<QQuickItem>(window->rootObject(), "item1");
    QVERIFY(item);
    QVERIFY(item->hasActiveFocus());

    // tab
    key = QKeyEvent(QEvent::KeyPress, Qt::Key_Tab, Qt::NoModifier, "", false, 1);
    QGuiApplication::sendEvent(window, &key);
    QVERIFY(key.isAccepted());

    item = findItem<QQuickItem>(window->rootObject(), "item2");
    QVERIFY(item);
    QVERIFY(item->hasActiveFocus());

    // backtab
    key = QKeyEvent(QEvent::KeyPress, Qt::Key_Backtab, Qt::NoModifier, "", false, 1);
    QGuiApplication::sendEvent(window, &key);
    QVERIFY(key.isAccepted());

    item = findItem<QQuickItem>(window->rootObject(), "item1");
    QVERIFY(item);
    QVERIFY(item->hasActiveFocus());

    delete window;
}

void tst_QQuickItem::keyNavigation_RightToLeft()
{
    QQuickView *window = new QQuickView(0);
    window->setBaseSize(QSize(240,320));

    window->setSource(testFileUrl("keynavigationtest.qml"));
    window->show();
    window->requestActivate();
    QVERIFY(QTest::qWaitForWindowActive(window));
    QCOMPARE(QGuiApplication::focusWindow(), window);

    QQuickItem *rootItem = qobject_cast<QQuickItem*>(window->rootObject());
    QVERIFY(rootItem);
    QQuickItemPrivate* rootItemPrivate = QQuickItemPrivate::get(rootItem);

    rootItemPrivate->effectiveLayoutMirror = true; // LayoutMirroring.mirror: true
    rootItemPrivate->isMirrorImplicit = false;
    rootItemPrivate->inheritMirrorFromItem = true; // LayoutMirroring.inherit: true
    rootItemPrivate->resolveLayoutMirror();

    QEvent wa(QEvent::WindowActivate);
    QGuiApplication::sendEvent(window, &wa);
    QFocusEvent fe(QEvent::FocusIn);
    QGuiApplication::sendEvent(window, &fe);

    QQuickItem *item = findItem<QQuickItem>(window->rootObject(), "item1");
    QVERIFY(item);
    QVERIFY(item->hasActiveFocus());

    QVariant result;
    QVERIFY(QMetaObject::invokeMethod(window->rootObject(), "verify",
            Q_RETURN_ARG(QVariant, result)));
    QVERIFY(result.toBool());

    // right
    QKeyEvent key(QEvent::KeyPress, Qt::Key_Left, Qt::NoModifier, "", false, 1);
    QGuiApplication::sendEvent(window, &key);
    QVERIFY(key.isAccepted());

    item = findItem<QQuickItem>(window->rootObject(), "item2");
    QVERIFY(item);
    QVERIFY(item->hasActiveFocus());

    // left
    key = QKeyEvent(QEvent::KeyPress, Qt::Key_Right, Qt::NoModifier, "", false, 1);
    QGuiApplication::sendEvent(window, &key);
    QVERIFY(key.isAccepted());

    item = findItem<QQuickItem>(window->rootObject(), "item1");
    QVERIFY(item);
    QVERIFY(item->hasActiveFocus());

    delete window;
}

void tst_QQuickItem::keyNavigation_skipNotVisible()
{
    QQuickView *window = new QQuickView(0);
    window->setBaseSize(QSize(240,320));

    window->setSource(testFileUrl("keynavigationtest.qml"));
    window->show();
    window->requestActivate();
    QVERIFY(QTest::qWaitForWindowActive(window));
    QCOMPARE(QGuiApplication::focusWindow(), window);

    QQuickItem *item = findItem<QQuickItem>(window->rootObject(), "item1");
    QVERIFY(item);
    QVERIFY(item->hasActiveFocus());

    // Set item 2 to not visible
    item = findItem<QQuickItem>(window->rootObject(), "item2");
    QVERIFY(item);
    item->setVisible(false);
    QVERIFY(!item->isVisible());

    // right
    QKeyEvent key(QEvent::KeyPress, Qt::Key_Right, Qt::NoModifier, "", false, 1);
    QGuiApplication::sendEvent(window, &key);
    QVERIFY(key.isAccepted());

    item = findItem<QQuickItem>(window->rootObject(), "item1");
    QVERIFY(item);
    QVERIFY(item->hasActiveFocus());

    // tab
    key = QKeyEvent(QEvent::KeyPress, Qt::Key_Tab, Qt::NoModifier, "", false, 1);
    QGuiApplication::sendEvent(window, &key);
    QVERIFY(key.isAccepted());

    item = findItem<QQuickItem>(window->rootObject(), "item3");
    QVERIFY(item);
    QVERIFY(item->hasActiveFocus());

    // backtab
    key = QKeyEvent(QEvent::KeyPress, Qt::Key_Backtab, Qt::NoModifier, "", false, 1);
    QGuiApplication::sendEvent(window, &key);
    QVERIFY(key.isAccepted());

    item = findItem<QQuickItem>(window->rootObject(), "item1");
    QVERIFY(item);
    QVERIFY(item->hasActiveFocus());

    //Set item 3 to not visible
    item = findItem<QQuickItem>(window->rootObject(), "item3");
    QVERIFY(item);
    item->setVisible(false);
    QVERIFY(!item->isVisible());

    // tab
    key = QKeyEvent(QEvent::KeyPress, Qt::Key_Tab, Qt::NoModifier, "", false, 1);
    QGuiApplication::sendEvent(window, &key);
    QVERIFY(key.isAccepted());

    item = findItem<QQuickItem>(window->rootObject(), "item4");
    QVERIFY(item);
    QVERIFY(item->hasActiveFocus());

    // backtab
    key = QKeyEvent(QEvent::KeyPress, Qt::Key_Backtab, Qt::NoModifier, "", false, 1);
    QGuiApplication::sendEvent(window, &key);
    QVERIFY(key.isAccepted());

    item = findItem<QQuickItem>(window->rootObject(), "item1");
    QVERIFY(item);
    QVERIFY(item->hasActiveFocus());

    delete window;
}

void tst_QQuickItem::keyNavigation_implicitSetting()
{
    QQuickView *window = new QQuickView(0);
    window->setBaseSize(QSize(240,320));

    window->setSource(testFileUrl("keynavigationtest_implicit.qml"));
    window->show();
    window->requestActivate();
    QVERIFY(QTest::qWaitForWindowActive(window));
    QCOMPARE(QGuiApplication::focusWindow(), window);

    QEvent wa(QEvent::WindowActivate);
    QGuiApplication::sendEvent(window, &wa);
    QFocusEvent fe(QEvent::FocusIn);
    QGuiApplication::sendEvent(window, &fe);

    QQuickItem *item = findItem<QQuickItem>(window->rootObject(), "item1");
    QVERIFY(item);
    QVERIFY(item->hasActiveFocus());

    QVariant result;
    QVERIFY(QMetaObject::invokeMethod(window->rootObject(), "verify",
            Q_RETURN_ARG(QVariant, result)));
    QVERIFY(result.toBool());

    // right
    QKeyEvent key(QEvent::KeyPress, Qt::Key_Right, Qt::NoModifier, "", false, 1);
    QGuiApplication::sendEvent(window, &key);
    QVERIFY(key.isAccepted());

    item = findItem<QQuickItem>(window->rootObject(), "item2");
    QVERIFY(item);
    QVERIFY(item->hasActiveFocus());

    // back to item1
    key = QKeyEvent(QEvent::KeyPress, Qt::Key_Left, Qt::NoModifier, "", false, 1);
    QGuiApplication::sendEvent(window, &key);
    QVERIFY(key.isAccepted());

    item = findItem<QQuickItem>(window->rootObject(), "item1");
    QVERIFY(item);
    QVERIFY(item->hasActiveFocus());

    // down
    key = QKeyEvent(QEvent::KeyPress, Qt::Key_Down, Qt::NoModifier, "", false, 1);
    QGuiApplication::sendEvent(window, &key);
    QVERIFY(key.isAccepted());

    item = findItem<QQuickItem>(window->rootObject(), "item3");
    QVERIFY(item);
    QVERIFY(item->hasActiveFocus());

    // move to item4
    key = QKeyEvent(QEvent::KeyPress, Qt::Key_Right, Qt::NoModifier, "", false, 1);
    QGuiApplication::sendEvent(window, &key);
    QVERIFY(key.isAccepted());

    item = findItem<QQuickItem>(window->rootObject(), "item4");
    QVERIFY(item);
    QVERIFY(item->hasActiveFocus());

    // left
    key = QKeyEvent(QEvent::KeyPress, Qt::Key_Left, Qt::NoModifier, "", false, 1);
    QGuiApplication::sendEvent(window, &key);
    QVERIFY(key.isAccepted());

    item = findItem<QQuickItem>(window->rootObject(), "item3");
    QVERIFY(item);
    QVERIFY(item->hasActiveFocus());

    // back to item4
    key = QKeyEvent(QEvent::KeyPress, Qt::Key_Right, Qt::NoModifier, "", false, 1);
    QGuiApplication::sendEvent(window, &key);
    QVERIFY(key.isAccepted());

    item = findItem<QQuickItem>(window->rootObject(), "item4");
    QVERIFY(item);
    QVERIFY(item->hasActiveFocus());

    // up
    key = QKeyEvent(QEvent::KeyPress, Qt::Key_Up, Qt::NoModifier, "", false, 1);
    QGuiApplication::sendEvent(window, &key);
    QVERIFY(key.isAccepted());

    item = findItem<QQuickItem>(window->rootObject(), "item2");
    QVERIFY(item);
    QVERIFY(item->hasActiveFocus());

    // back to item4
    key = QKeyEvent(QEvent::KeyPress, Qt::Key_Down, Qt::NoModifier, "", false, 1);
    QGuiApplication::sendEvent(window, &key);
    QVERIFY(key.isAccepted());

    item = findItem<QQuickItem>(window->rootObject(), "item4");
    QVERIFY(item);
    QVERIFY(item->hasActiveFocus());

    // tab
    key = QKeyEvent(QEvent::KeyPress, Qt::Key_Tab, Qt::NoModifier, "", false, 1);
    QGuiApplication::sendEvent(window, &key);
    QVERIFY(key.isAccepted());

    item = findItem<QQuickItem>(window->rootObject(), "item1");
    QVERIFY(item);
    QVERIFY(item->hasActiveFocus());

    // back to item4
    key = QKeyEvent(QEvent::KeyPress, Qt::Key_Backtab, Qt::NoModifier, "", false, 1);
    QGuiApplication::sendEvent(window, &key);
    QVERIFY(key.isAccepted());

    item = findItem<QQuickItem>(window->rootObject(), "item4");
    QVERIFY(item);
    QVERIFY(item->hasActiveFocus());

    // backtab
    key = QKeyEvent(QEvent::KeyPress, Qt::Key_Backtab, Qt::NoModifier, "", false, 1);
    QGuiApplication::sendEvent(window, &key);
    QVERIFY(key.isAccepted());

    item = findItem<QQuickItem>(window->rootObject(), "item3");
    QVERIFY(item);
    QVERIFY(item->hasActiveFocus());

    delete window;
}

void tst_QQuickItem::keyNavigation_focusReason()
{
    QQuickView *window = new QQuickView(0);
    window->setBaseSize(QSize(240,320));

    FocusEventFilter focusEventFilter;

    window->setSource(testFileUrl("keynavigationtest.qml"));
    window->show();
    window->requestActivate();

    QVERIFY(QTest::qWaitForWindowActive(window));
    QCOMPARE(QGuiApplication::focusWindow(), window);

    // install event filter on first item
    QQuickItem *item = findItem<QQuickItem>(window->rootObject(), "item1");
    QVERIFY(item);
    QVERIFY(item->hasActiveFocus());
    item->installEventFilter(&focusEventFilter);

    //install event filter on second item
    item = findItem<QQuickItem>(window->rootObject(), "item2");
    QVERIFY(item);
    item->installEventFilter(&focusEventFilter);

    //install event filter on third item
    item = findItem<QQuickItem>(window->rootObject(), "item3");
    QVERIFY(item);
    item->installEventFilter(&focusEventFilter);

    //install event filter on last item
    item = findItem<QQuickItem>(window->rootObject(), "item4");
    QVERIFY(item);
    item->installEventFilter(&focusEventFilter);

    // tab
    QKeyEvent key = QKeyEvent(QEvent::KeyPress, Qt::Key_Tab, Qt::NoModifier, "", false, 1);
    QGuiApplication::sendEvent(window, &key);
    QVERIFY(key.isAccepted());
    QCOMPARE(focusEventFilter.lastFocusReason, Qt::TabFocusReason);

    // backtab
    key = QKeyEvent(QEvent::KeyPress, Qt::Key_Backtab, Qt::NoModifier, "", false, 1);
    QGuiApplication::sendEvent(window, &key);
    QVERIFY(key.isAccepted());
    QCOMPARE(focusEventFilter.lastFocusReason, Qt::BacktabFocusReason);

    // right - it's also one kind of key navigation
    key = QKeyEvent(QEvent::KeyPress, Qt::Key_Right, Qt::NoModifier, "", false, 1);
    QGuiApplication::sendEvent(window, &key);
    QVERIFY(key.isAccepted());
    QCOMPARE(focusEventFilter.lastFocusReason, Qt::TabFocusReason);

    item->setFocus(true, Qt::OtherFocusReason);
    QVERIFY(item->hasActiveFocus());
    QCOMPARE(focusEventFilter.lastFocusReason, Qt::OtherFocusReason);

    delete window;
}

void tst_QQuickItem::keyNavigation_loop()
{
    // QTBUG-47229
    QQuickView *window = new QQuickView(0);
    window->setBaseSize(QSize(240,320));

    window->setSource(testFileUrl("keynavigationtest_loop.qml"));
    window->show();
    window->requestActivate();

    QVERIFY(QTest::qWaitForWindowActive(window));
    QCOMPARE(QGuiApplication::focusWindow(), window);

    QQuickItem *item = findItem<QQuickItem>(window->rootObject(), "item1");
    QVERIFY(item);
    QVERIFY(item->hasActiveFocus());

    QKeyEvent key = QKeyEvent(QEvent::KeyPress, Qt::Key_Down, Qt::NoModifier, "", false, 1);
    QGuiApplication::sendEvent(window, &key);
    QVERIFY(key.isAccepted());
    QVERIFY(item->hasActiveFocus());

    delete window;
}

void tst_QQuickItem::smooth()
{
    QQmlComponent component(&engine);
    component.setData("import QtQuick 2.0; Item { smooth: false; }", QUrl::fromLocalFile(""));
    QQuickItem *item = qobject_cast<QQuickItem*>(component.create());
    QSignalSpy spy(item, SIGNAL(smoothChanged(bool)));

    QVERIFY(item);
    QVERIFY(!item->smooth());

    item->setSmooth(true);
    QVERIFY(item->smooth());
    QCOMPARE(spy.count(),1);
    QList<QVariant> arguments = spy.first();
    QCOMPARE(arguments.count(), 1);
    QVERIFY(arguments.at(0).toBool());

    item->setSmooth(true);
    QCOMPARE(spy.count(),1);

    item->setSmooth(false);
    QVERIFY(!item->smooth());
    QCOMPARE(spy.count(),2);
    item->setSmooth(false);
    QCOMPARE(spy.count(),2);

    delete item;
}

void tst_QQuickItem::antialiasing()
{
    QQmlComponent component(&engine);
    component.setData("import QtQuick 2.0; Item { antialiasing: false; }", QUrl::fromLocalFile(""));
    QQuickItem *item = qobject_cast<QQuickItem*>(component.create());
    QSignalSpy spy(item, SIGNAL(antialiasingChanged(bool)));

    QVERIFY(item);
    QVERIFY(!item->antialiasing());

    item->setAntialiasing(true);
    QVERIFY(item->antialiasing());
    QCOMPARE(spy.count(),1);
    QList<QVariant> arguments = spy.first();
    QCOMPARE(arguments.count(), 1);
    QVERIFY(arguments.at(0).toBool());

    item->setAntialiasing(true);
    QCOMPARE(spy.count(),1);

    item->setAntialiasing(false);
    QVERIFY(!item->antialiasing());
    QCOMPARE(spy.count(),2);
    item->setAntialiasing(false);
    QCOMPARE(spy.count(),2);

    delete item;
}

void tst_QQuickItem::clip()
{
    QQmlComponent component(&engine);
    component.setData("import QtQuick 2.0\nItem { clip: false\n }", QUrl::fromLocalFile(""));
    QQuickItem *item = qobject_cast<QQuickItem*>(component.create());
    QSignalSpy spy(item, SIGNAL(clipChanged(bool)));

    QVERIFY(item);
    QVERIFY(!item->clip());

    item->setClip(true);
    QVERIFY(item->clip());

    QList<QVariant> arguments = spy.first();
    QCOMPARE(arguments.count(), 1);
    QVERIFY(arguments.at(0).toBool());

    QCOMPARE(spy.count(),1);
    item->setClip(true);
    QCOMPARE(spy.count(),1);

    item->setClip(false);
    QVERIFY(!item->clip());
    QCOMPARE(spy.count(),2);
    item->setClip(false);
    QCOMPARE(spy.count(),2);

    delete item;
}

void tst_QQuickItem::mapCoordinates()
{
    QFETCH(int, x);
    QFETCH(int, y);

    QQuickView *window = new QQuickView(0);
    window->setBaseSize(QSize(300, 300));
    window->setSource(testFileUrl("mapCoordinates.qml"));
    window->show();
    qApp->processEvents();

    QQuickItem *root = qobject_cast<QQuickItem*>(window->rootObject());
    QVERIFY(root != 0);
    QQuickItem *a = findItem<QQuickItem>(window->rootObject(), "itemA");
    QVERIFY(a != 0);
    QQuickItem *b = findItem<QQuickItem>(window->rootObject(), "itemB");
    QVERIFY(b != 0);

    QVariant result;

    QVERIFY(QMetaObject::invokeMethod(root, "mapAToB",
            Q_RETURN_ARG(QVariant, result), Q_ARG(QVariant, x), Q_ARG(QVariant, y)));
    QCOMPARE(result.value<QPointF>(), qobject_cast<QQuickItem*>(a)->mapToItem(b, QPointF(x, y)));

    QVERIFY(QMetaObject::invokeMethod(root, "mapAFromB",
            Q_RETURN_ARG(QVariant, result), Q_ARG(QVariant, x), Q_ARG(QVariant, y)));
    QCOMPARE(result.value<QPointF>(), qobject_cast<QQuickItem*>(a)->mapFromItem(b, QPointF(x, y)));

    QVERIFY(QMetaObject::invokeMethod(root, "mapAToNull",
            Q_RETURN_ARG(QVariant, result), Q_ARG(QVariant, x), Q_ARG(QVariant, y)));
    QCOMPARE(result.value<QPointF>(), qobject_cast<QQuickItem*>(a)->mapToScene(QPointF(x, y)));

    QVERIFY(QMetaObject::invokeMethod(root, "mapAFromNull",
            Q_RETURN_ARG(QVariant, result), Q_ARG(QVariant, x), Q_ARG(QVariant, y)));
    QCOMPARE(result.value<QPointF>(), qobject_cast<QQuickItem*>(a)->mapFromScene(QPointF(x, y)));

    QVERIFY(QMetaObject::invokeMethod(root, "mapAToGlobal",
            Q_RETURN_ARG(QVariant, result), Q_ARG(QVariant, x), Q_ARG(QVariant, y)));
    QCOMPARE(result.value<QPointF>(), qobject_cast<QQuickItem*>(a)->mapToGlobal(QPointF(x, y)));

    QVERIFY(QMetaObject::invokeMethod(root, "mapAFromGlobal",
            Q_RETURN_ARG(QVariant, result), Q_ARG(QVariant, x), Q_ARG(QVariant, y)));
    QCOMPARE(result.value<QPointF>(), qobject_cast<QQuickItem*>(a)->mapFromGlobal(QPointF(x, y)));

    QString warning1 = testFileUrl("mapCoordinates.qml").toString() + ":35:5: QML Item: mapToItem() given argument \"1122\" which is neither null nor an Item";
    QString warning2 = testFileUrl("mapCoordinates.qml").toString() + ":35:5: QML Item: mapFromItem() given argument \"1122\" which is neither null nor an Item";

    QTest::ignoreMessage(QtWarningMsg, qPrintable(warning1));
    QVERIFY(QMetaObject::invokeMethod(root, "checkMapAToInvalid",
            Q_RETURN_ARG(QVariant, result), Q_ARG(QVariant, x), Q_ARG(QVariant, y)));
    QVERIFY(result.toBool());

    QTest::ignoreMessage(QtWarningMsg, qPrintable(warning2));
    QVERIFY(QMetaObject::invokeMethod(root, "checkMapAFromInvalid",
            Q_RETURN_ARG(QVariant, result), Q_ARG(QVariant, x), Q_ARG(QVariant, y)));
    QVERIFY(result.toBool());

    delete window;
}

void tst_QQuickItem::mapCoordinates_data()
{
    QTest::addColumn<int>("x");
    QTest::addColumn<int>("y");

    for (int i=-20; i<=20; i+=10)
        QTest::newRow(QTest::toString(i)) << i << i;
}

void tst_QQuickItem::mapCoordinatesRect()
{
    QFETCH(int, x);
    QFETCH(int, y);
    QFETCH(int, width);
    QFETCH(int, height);

    QQuickView *window = new QQuickView(0);
    window->setBaseSize(QSize(300, 300));
    window->setSource(testFileUrl("mapCoordinatesRect.qml"));
    window->show();
    qApp->processEvents();

    QQuickItem *root = qobject_cast<QQuickItem*>(window->rootObject());
    QVERIFY(root != 0);
    QQuickItem *a = findItem<QQuickItem>(window->rootObject(), "itemA");
    QVERIFY(a != 0);
    QQuickItem *b = findItem<QQuickItem>(window->rootObject(), "itemB");
    QVERIFY(b != 0);

    QVariant result;

    QVERIFY(QMetaObject::invokeMethod(root, "mapAToB",
            Q_RETURN_ARG(QVariant, result), Q_ARG(QVariant, x), Q_ARG(QVariant, y), Q_ARG(QVariant, width), Q_ARG(QVariant, height)));
    QCOMPARE(result.value<QRectF>(), qobject_cast<QQuickItem*>(a)->mapRectToItem(b, QRectF(x, y, width, height)));

    QVERIFY(QMetaObject::invokeMethod(root, "mapAFromB",
            Q_RETURN_ARG(QVariant, result), Q_ARG(QVariant, x), Q_ARG(QVariant, y), Q_ARG(QVariant, width), Q_ARG(QVariant, height)));
    QCOMPARE(result.value<QRectF>(), qobject_cast<QQuickItem*>(a)->mapRectFromItem(b, QRectF(x, y, width, height)));

    QVERIFY(QMetaObject::invokeMethod(root, "mapAToNull",
            Q_RETURN_ARG(QVariant, result), Q_ARG(QVariant, x), Q_ARG(QVariant, y), Q_ARG(QVariant, width), Q_ARG(QVariant, height)));
    QCOMPARE(result.value<QRectF>(), qobject_cast<QQuickItem*>(a)->mapRectToScene(QRectF(x, y, width, height)));

    QVERIFY(QMetaObject::invokeMethod(root, "mapAFromNull",
            Q_RETURN_ARG(QVariant, result), Q_ARG(QVariant, x), Q_ARG(QVariant, y), Q_ARG(QVariant, width), Q_ARG(QVariant, height)));
    QCOMPARE(result.value<QRectF>(), qobject_cast<QQuickItem*>(a)->mapRectFromScene(QRectF(x, y, width, height)));

    QString warning1 = testFileUrl("mapCoordinatesRect.qml").toString() + ":35:5: QML Item: mapToItem() given argument \"1122\" which is neither null nor an Item";
    QString warning2 = testFileUrl("mapCoordinatesRect.qml").toString() + ":35:5: QML Item: mapFromItem() given argument \"1122\" which is neither null nor an Item";

    QTest::ignoreMessage(QtWarningMsg, qPrintable(warning1));
    QVERIFY(QMetaObject::invokeMethod(root, "checkMapAToInvalid",
            Q_RETURN_ARG(QVariant, result), Q_ARG(QVariant, x), Q_ARG(QVariant, y), Q_ARG(QVariant, width), Q_ARG(QVariant, height)));
    QVERIFY(result.toBool());

    QTest::ignoreMessage(QtWarningMsg, qPrintable(warning2));
    QVERIFY(QMetaObject::invokeMethod(root, "checkMapAFromInvalid",
            Q_RETURN_ARG(QVariant, result), Q_ARG(QVariant, x), Q_ARG(QVariant, y), Q_ARG(QVariant, width), Q_ARG(QVariant, height)));
    QVERIFY(result.toBool());

    delete window;
}

void tst_QQuickItem::mapCoordinatesRect_data()
{
    QTest::addColumn<int>("x");
    QTest::addColumn<int>("y");
    QTest::addColumn<int>("width");
    QTest::addColumn<int>("height");

    for (int i=-20; i<=20; i+=5)
        QTest::newRow(QTest::toString(i)) << i << i << i << i;
}

void tst_QQuickItem::transforms_data()
{
    QTest::addColumn<QByteArray>("qml");
    QTest::addColumn<QTransform>("transform");
    QTest::newRow("translate") << QByteArray("Translate { x: 10; y: 20 }")
        << QTransform(1,0,0,0,1,0,10,20,1);
    QTest::newRow("matrix4x4") << QByteArray("Matrix4x4 { matrix: Qt.matrix4x4(1,0,0,10, 0,1,0,15, 0,0,1,0, 0,0,0,1) }")
        << QTransform(1,0,0,0,1,0,10,15,1);
    QTest::newRow("rotation") << QByteArray("Rotation { angle: 90 }")
        << QTransform(0,1,0,-1,0,0,0,0,1);
    QTest::newRow("scale") << QByteArray("Scale { xScale: 1.5; yScale: -2  }")
        << QTransform(1.5,0,0,0,-2,0,0,0,1);
    QTest::newRow("sequence") << QByteArray("[ Translate { x: 10; y: 20 }, Scale { xScale: 1.5; yScale: -2  } ]")
        << QTransform(1,0,0,0,1,0,10,20,1) * QTransform(1.5,0,0,0,-2,0,0,0,1);
}

void tst_QQuickItem::transforms()
{
    QFETCH(QByteArray, qml);
    QFETCH(QTransform, transform);
    QQmlComponent component(&engine);
    component.setData("import QtQuick 2.3\nItem { transform: "+qml+"}", QUrl::fromLocalFile(""));
    QQuickItem *item = qobject_cast<QQuickItem*>(component.create());
    QVERIFY(item);
    QCOMPARE(item->itemTransform(0,0), transform);
}

void tst_QQuickItem::childrenProperty()
{
    QQmlComponent component(&engine, testFileUrl("childrenProperty.qml"));

    QObject *o = component.create();
    QVERIFY(o != 0);

    QCOMPARE(o->property("test1").toBool(), true);
    QCOMPARE(o->property("test2").toBool(), true);
    QCOMPARE(o->property("test3").toBool(), true);
    QCOMPARE(o->property("test4").toBool(), true);
    QCOMPARE(o->property("test5").toBool(), true);
    delete o;
}

void tst_QQuickItem::resourcesProperty()
{
    QQmlComponent component(&engine, testFileUrl("resourcesProperty.qml"));

    QObject *object = component.create();
    QVERIFY(object != 0);

    QQmlProperty property(object, "resources", component.creationContext());

    QVERIFY(property.isValid());
    QQmlListReference list = qvariant_cast<QQmlListReference>(property.read());
    QVERIFY(list.isValid());

    QCOMPARE(list.count(), 4);

    QCOMPARE(object->property("test1").toBool(), true);
    QCOMPARE(object->property("test2").toBool(), true);
    QCOMPARE(object->property("test3").toBool(), true);
    QCOMPARE(object->property("test4").toBool(), true);
    QCOMPARE(object->property("test5").toBool(), true);
    QCOMPARE(object->property("test6").toBool(), true);

    QObject *subObject = object->findChild<QObject *>("subObject");

    QVERIFY(subObject);

    QCOMPARE(object, subObject->parent());

    delete subObject;

    QCOMPARE(list.count(), 3);

    delete object;
}

void tst_QQuickItem::propertyChanges()
{
    QQuickView *window = new QQuickView(0);
    window->setBaseSize(QSize(300, 300));
    window->setSource(testFileUrl("propertychanges.qml"));
    window->show();
    window->requestActivate();
    QVERIFY(QTest::qWaitForWindowActive(window));
    QCOMPARE(QGuiApplication::focusWindow(), window);

    QQuickItem *item = findItem<QQuickItem>(window->rootObject(), "item");
    QQuickItem *parentItem = findItem<QQuickItem>(window->rootObject(), "parentItem");

    QVERIFY(item);
    QVERIFY(parentItem);

    QSignalSpy parentSpy(item, SIGNAL(parentChanged(QQuickItem*)));
    QSignalSpy widthSpy(item, SIGNAL(widthChanged()));
    QSignalSpy heightSpy(item, SIGNAL(heightChanged()));
    QSignalSpy baselineOffsetSpy(item, SIGNAL(baselineOffsetChanged(qreal)));
    QSignalSpy childrenRectSpy(parentItem, SIGNAL(childrenRectChanged(QRectF)));
    QSignalSpy focusSpy(item, SIGNAL(focusChanged(bool)));
    QSignalSpy wantsFocusSpy(parentItem, SIGNAL(activeFocusChanged(bool)));
    QSignalSpy childrenChangedSpy(parentItem, SIGNAL(childrenChanged()));
    QSignalSpy xSpy(item, SIGNAL(xChanged()));
    QSignalSpy ySpy(item, SIGNAL(yChanged()));

    item->setParentItem(parentItem);
    item->setWidth(100.0);
    item->setHeight(200.0);
    item->setFocus(true);
    item->setBaselineOffset(10.0);

    QCOMPARE(item->parentItem(), parentItem);
    QCOMPARE(parentSpy.count(),1);
    QList<QVariant> parentArguments = parentSpy.first();
    QCOMPARE(parentArguments.count(), 1);
    QCOMPARE(item->parentItem(), qvariant_cast<QQuickItem *>(parentArguments.at(0)));
    QCOMPARE(childrenChangedSpy.count(),1);

    item->setParentItem(parentItem);
    QCOMPARE(childrenChangedSpy.count(),1);

    QCOMPARE(item->width(), 100.0);
    QCOMPARE(widthSpy.count(),1);

    QCOMPARE(item->height(), 200.0);
    QCOMPARE(heightSpy.count(),1);

    QCOMPARE(item->baselineOffset(), 10.0);
    QCOMPARE(baselineOffsetSpy.count(),1);
    QList<QVariant> baselineOffsetArguments = baselineOffsetSpy.first();
    QCOMPARE(baselineOffsetArguments.count(), 1);
    QCOMPARE(item->baselineOffset(), baselineOffsetArguments.at(0).toReal());

    QCOMPARE(parentItem->childrenRect(), QRectF(0.0,0.0,100.0,200.0));
    QCOMPARE(childrenRectSpy.count(),1);
    QList<QVariant> childrenRectArguments = childrenRectSpy.at(0);
    QCOMPARE(childrenRectArguments.count(), 1);
    QCOMPARE(parentItem->childrenRect(), childrenRectArguments.at(0).toRectF());

    QCOMPARE(item->hasActiveFocus(), true);
    QCOMPARE(focusSpy.count(),1);
    QList<QVariant> focusArguments = focusSpy.first();
    QCOMPARE(focusArguments.count(), 1);
    QCOMPARE(focusArguments.at(0).toBool(), true);

    QCOMPARE(parentItem->hasActiveFocus(), false);
    QCOMPARE(parentItem->hasFocus(), false);
    QCOMPARE(wantsFocusSpy.count(),0);

    item->setX(10.0);
    QCOMPARE(item->x(), 10.0);
    QCOMPARE(xSpy.count(), 1);

    item->setY(10.0);
    QCOMPARE(item->y(), 10.0);
    QCOMPARE(ySpy.count(), 1);

    delete window;
}

void tst_QQuickItem::childrenRect()
{
    QQuickView *window = new QQuickView(0);
    window->setSource(testFileUrl("childrenRect.qml"));
    window->setBaseSize(QSize(240,320));
    window->show();

    QQuickItem *o = window->rootObject();
    QQuickItem *item = o->findChild<QQuickItem*>("testItem");
    QCOMPARE(item->width(), qreal(0));
    QCOMPARE(item->height(), qreal(0));

    o->setProperty("childCount", 1);
    QCOMPARE(item->width(), qreal(10));
    QCOMPARE(item->height(), qreal(20));

    o->setProperty("childCount", 5);
    QCOMPARE(item->width(), qreal(50));
    QCOMPARE(item->height(), qreal(100));

    o->setProperty("childCount", 0);
    QCOMPARE(item->width(), qreal(0));
    QCOMPARE(item->height(), qreal(0));

    delete o;
    delete window;
}

// QTBUG-11383
void tst_QQuickItem::childrenRectBug()
{
    QQuickView *window = new QQuickView(0);

    QString warning = testFileUrl("childrenRectBug.qml").toString() + ":7:5: QML Item: Binding loop detected for property \"height\"";
    QTest::ignoreMessage(QtWarningMsg, qPrintable(warning));
    QTest::ignoreMessage(QtWarningMsg, qPrintable(warning));

    window->setSource(testFileUrl("childrenRectBug.qml"));
    window->show();

    QQuickItem *o = window->rootObject();
    QQuickItem *item = o->findChild<QQuickItem*>("theItem");
    QCOMPARE(item->width(), qreal(200));
    QCOMPARE(item->height(), qreal(100));
    QCOMPARE(item->x(), qreal(100));

    delete window;
}

// QTBUG-11465
void tst_QQuickItem::childrenRectBug2()
{
    QQuickView *window = new QQuickView(0);

    QString warning1 = testFileUrl("childrenRectBug2.qml").toString() + ":7:5: QML Item: Binding loop detected for property \"width\"";
    QTest::ignoreMessage(QtWarningMsg, qPrintable(warning1));
    QTest::ignoreMessage(QtWarningMsg, qPrintable(warning1));

    QString warning2 = testFileUrl("childrenRectBug2.qml").toString() + ":7:5: QML Item: Binding loop detected for property \"height\"";
    QTest::ignoreMessage(QtWarningMsg, qPrintable(warning2));
    QTest::ignoreMessage(QtWarningMsg, qPrintable(warning2));
    QTest::ignoreMessage(QtWarningMsg, qPrintable(warning2));
    QTest::ignoreMessage(QtWarningMsg, qPrintable(warning2));

    window->setSource(testFileUrl("childrenRectBug2.qml"));
    window->show();

    QQuickRectangle *rect = qobject_cast<QQuickRectangle*>(window->rootObject());
    QVERIFY(rect);
    QQuickItem *item = rect->findChild<QQuickItem*>("theItem");
    QCOMPARE(item->width(), qreal(100));
    QCOMPARE(item->height(), qreal(110));
    QCOMPARE(item->x(), qreal(130));

    QQuickItemPrivate *rectPrivate = QQuickItemPrivate::get(rect);
    rectPrivate->setState("row");
    QCOMPARE(item->width(), qreal(210));
    QCOMPARE(item->height(), qreal(50));
    QCOMPARE(item->x(), qreal(75));

    delete window;
}

// QTBUG-12722
void tst_QQuickItem::childrenRectBug3()
{
    QQuickView *window = new QQuickView(0);
    window->setSource(testFileUrl("childrenRectBug3.qml"));
    window->show();

    //don't crash on delete
    delete window;
}

// QTBUG-38732
void tst_QQuickItem::childrenRectBottomRightCorner()
{
    QQuickView *window = new QQuickView(0);
    window->setSource(testFileUrl("childrenRectBottomRightCorner.qml"));
    window->show();

    QQuickItem *rect = window->rootObject()->findChild<QQuickItem*>("childrenRectProxy");
    QCOMPARE(rect->x(), qreal(-100));
    QCOMPARE(rect->y(), qreal(-100));
    QCOMPARE(rect->width(), qreal(50));
    QCOMPARE(rect->height(), qreal(50));

    delete window;
}

struct TestListener : public QQuickItemChangeListener
{
    TestListener(bool remove = false) : remove(remove) { }

    void itemGeometryChanged(QQuickItem *item, QQuickGeometryChange, const QRectF &oldGeometry) override
    {
        record(item, QQuickItemPrivate::Geometry, oldGeometry);
    }
    void itemSiblingOrderChanged(QQuickItem *item) override
    {
        record(item, QQuickItemPrivate::SiblingOrder);
    }
    void itemVisibilityChanged(QQuickItem *item) override
    {
        record(item, QQuickItemPrivate::Visibility);
    }
    void itemOpacityChanged(QQuickItem *item) override
    {
        record(item, QQuickItemPrivate::Opacity);
    }
    void itemRotationChanged(QQuickItem *item) override
    {
        record(item, QQuickItemPrivate::Rotation);
    }
    void itemImplicitWidthChanged(QQuickItem *item) override
    {
        record(item, QQuickItemPrivate::ImplicitWidth);
    }
    void itemImplicitHeightChanged(QQuickItem *item) override
    {
        record(item, QQuickItemPrivate::ImplicitHeight);
    }
    void itemDestroyed(QQuickItem *item) override
    {
        record(item, QQuickItemPrivate::Destroyed);
    }
    void itemChildAdded(QQuickItem *item, QQuickItem *child) override
    {
        record(item, QQuickItemPrivate::Children, QVariant::fromValue(child));
    }
    void itemChildRemoved(QQuickItem *item, QQuickItem *child) override
    {
        record(item, QQuickItemPrivate::Children, QVariant::fromValue(child));
    }
    void itemParentChanged(QQuickItem *item, QQuickItem *parent) override
    {
        record(item, QQuickItemPrivate::Parent, QVariant::fromValue(parent));
    }

    QQuickAnchorsPrivate *anchorPrivate() override { return nullptr; }

    void record(QQuickItem *item, QQuickItemPrivate::ChangeType change, const QVariant &value = QVariant())
    {
        changes += change;
        values[change] = value;
        // QTBUG-54732
        if (remove)
            QQuickItemPrivate::get(item)->removeItemChangeListener(this, change);
    }

    int count(QQuickItemPrivate::ChangeType change) const
    {
        return changes.count(change);
    }

    QVariant value(QQuickItemPrivate::ChangeType change) const
    {
        return values.value(change);
    }

    bool remove;
    QList<QQuickItemPrivate::ChangeType> changes;
    QHash<QQuickItemPrivate::ChangeType, QVariant> values;
};

void tst_QQuickItem::changeListener()
{
    QQuickWindow window;
    window.show();
    QTest::qWaitForWindowExposed(&window);

    QQuickItem *item = new QQuickItem;
    TestListener itemListener;
    QQuickItemPrivate::get(item)->addItemChangeListener(&itemListener, QQuickItemPrivate::ChangeTypes(0xffff));

    item->setImplicitWidth(10);
    QCOMPARE(itemListener.count(QQuickItemPrivate::ImplicitWidth), 1);
    QCOMPARE(itemListener.count(QQuickItemPrivate::Geometry), 1);
    QCOMPARE(itemListener.value(QQuickItemPrivate::Geometry), QVariant(QRectF(0,0,0,0)));

    item->setImplicitHeight(20);
    QCOMPARE(itemListener.count(QQuickItemPrivate::ImplicitHeight), 1);
    QCOMPARE(itemListener.count(QQuickItemPrivate::Geometry), 2);
    QCOMPARE(itemListener.value(QQuickItemPrivate::Geometry), QVariant(QRectF(0,0,10,0)));

    item->setWidth(item->width() + 30);
    QCOMPARE(itemListener.count(QQuickItemPrivate::Geometry), 3);
    QCOMPARE(itemListener.value(QQuickItemPrivate::Geometry), QVariant(QRectF(0,0,10,20)));

    item->setHeight(item->height() + 40);
    QCOMPARE(itemListener.count(QQuickItemPrivate::Geometry), 4);
    QCOMPARE(itemListener.value(QQuickItemPrivate::Geometry), QVariant(QRectF(0,0,40,20)));

    item->setOpacity(0.5);
    QCOMPARE(itemListener.count(QQuickItemPrivate::Opacity), 1);

    item->setRotation(90);
    QCOMPARE(itemListener.count(QQuickItemPrivate::Rotation), 1);

    item->setParentItem(window.contentItem());
    QCOMPARE(itemListener.count(QQuickItemPrivate::Parent), 1);

    item->setVisible(false);
    QCOMPARE(itemListener.count(QQuickItemPrivate::Visibility), 1);

    QQuickItemPrivate::get(item)->removeItemChangeListener(&itemListener, QQuickItemPrivate::ChangeTypes(0xffff));

    QQuickItem *parent = new QQuickItem(window.contentItem());
    TestListener parentListener;
    QQuickItemPrivate::get(parent)->addItemChangeListener(&parentListener, QQuickItemPrivate::Children);

    QQuickItem *child1 = new QQuickItem;
    QQuickItem *child2 = new QQuickItem;
    TestListener child1Listener;
    TestListener child2Listener;
    QQuickItemPrivate::get(child1)->addItemChangeListener(&child1Listener, QQuickItemPrivate::Parent | QQuickItemPrivate::SiblingOrder | QQuickItemPrivate::Destroyed);
    QQuickItemPrivate::get(child2)->addItemChangeListener(&child2Listener, QQuickItemPrivate::Parent | QQuickItemPrivate::SiblingOrder | QQuickItemPrivate::Destroyed);

    child1->setParentItem(parent);
    QCOMPARE(parentListener.count(QQuickItemPrivate::Children), 1);
    QCOMPARE(parentListener.value(QQuickItemPrivate::Children), QVariant::fromValue(child1));
    QCOMPARE(child1Listener.count(QQuickItemPrivate::Parent), 1);
    QCOMPARE(child1Listener.value(QQuickItemPrivate::Parent), QVariant::fromValue(parent));

    child2->setParentItem(parent);
    QCOMPARE(parentListener.count(QQuickItemPrivate::Children), 2);
    QCOMPARE(parentListener.value(QQuickItemPrivate::Children), QVariant::fromValue(child2));
    QCOMPARE(child2Listener.count(QQuickItemPrivate::Parent), 1);
    QCOMPARE(child2Listener.value(QQuickItemPrivate::Parent), QVariant::fromValue(parent));

    child2->stackBefore(child1);
    QCOMPARE(child1Listener.count(QQuickItemPrivate::SiblingOrder), 1);
    QCOMPARE(child2Listener.count(QQuickItemPrivate::SiblingOrder), 1);

    child1->setParentItem(nullptr);
    QCOMPARE(parentListener.count(QQuickItemPrivate::Children), 3);
    QCOMPARE(parentListener.value(QQuickItemPrivate::Children), QVariant::fromValue(child1));
    QCOMPARE(child1Listener.count(QQuickItemPrivate::Parent), 2);
    QCOMPARE(child1Listener.value(QQuickItemPrivate::Parent), QVariant::fromValue<QQuickItem *>(nullptr));

    delete child1;
    QCOMPARE(child1Listener.count(QQuickItemPrivate::Destroyed), 1);

    delete child2;
    QCOMPARE(parentListener.count(QQuickItemPrivate::Children), 4);
    QCOMPARE(parentListener.value(QQuickItemPrivate::Children), QVariant::fromValue(child2));
    QCOMPARE(child2Listener.count(QQuickItemPrivate::Parent), 2);
    QCOMPARE(child2Listener.value(QQuickItemPrivate::Parent), QVariant::fromValue<QQuickItem *>(nullptr));
    QCOMPARE(child2Listener.count(QQuickItemPrivate::Destroyed), 1);

    QQuickItemPrivate::get(parent)->removeItemChangeListener(&parentListener, QQuickItemPrivate::Children);
    QCOMPARE(QQuickItemPrivate::get(parent)->changeListeners.count(), 0);

    // QTBUG-54732: all listeners should get invoked even if they remove themselves while iterating the listeners
    QList<TestListener *> listeners;
    for (int i = 0; i < 5; ++i)
        listeners << new TestListener(true);

    // itemVisibilityChanged x 5
    foreach (TestListener *listener, listeners)
        QQuickItemPrivate::get(parent)->addItemChangeListener(listener, QQuickItemPrivate::Visibility);
    QCOMPARE(QQuickItemPrivate::get(parent)->changeListeners.count(), listeners.count());
    parent->setVisible(false);
    foreach (TestListener *listener, listeners)
        QCOMPARE(listener->count(QQuickItemPrivate::Visibility), 1);
    QCOMPARE(QQuickItemPrivate::get(parent)->changeListeners.count(), 0);

    // itemRotationChanged x 5
    foreach (TestListener *listener, listeners)
        QQuickItemPrivate::get(parent)->addItemChangeListener(listener, QQuickItemPrivate::Rotation);
    QCOMPARE(QQuickItemPrivate::get(parent)->changeListeners.count(), listeners.count());
    parent->setRotation(90);
    foreach (TestListener *listener, listeners)
        QCOMPARE(listener->count(QQuickItemPrivate::Rotation), 1);
    QCOMPARE(QQuickItemPrivate::get(parent)->changeListeners.count(), 0);

    // itemOpacityChanged x 5
    foreach (TestListener *listener, listeners)
        QQuickItemPrivate::get(parent)->addItemChangeListener(listener, QQuickItemPrivate::Opacity);
    QCOMPARE(QQuickItemPrivate::get(parent)->changeListeners.count(), listeners.count());
    parent->setOpacity(0.5);
    foreach (TestListener *listener, listeners)
        QCOMPARE(listener->count(QQuickItemPrivate::Opacity), 1);
    QCOMPARE(QQuickItemPrivate::get(parent)->changeListeners.count(), 0);

    // itemChildAdded() x 5
    foreach (TestListener *listener, listeners)
        QQuickItemPrivate::get(parent)->addItemChangeListener(listener, QQuickItemPrivate::Children);
    QCOMPARE(QQuickItemPrivate::get(parent)->changeListeners.count(), listeners.count());
    child1 = new QQuickItem(parent);
    foreach (TestListener *listener, listeners)
        QCOMPARE(listener->count(QQuickItemPrivate::Children), 1);
    QCOMPARE(QQuickItemPrivate::get(parent)->changeListeners.count(), 0);

    // itemParentChanged() x 5
    foreach (TestListener *listener, listeners)
        QQuickItemPrivate::get(child1)->addItemChangeListener(listener, QQuickItemPrivate::Parent);
    QCOMPARE(QQuickItemPrivate::get(child1)->changeListeners.count(), listeners.count());
    child1->setParentItem(nullptr);
    foreach (TestListener *listener, listeners)
        QCOMPARE(listener->count(QQuickItemPrivate::Parent), 1);
    QCOMPARE(QQuickItemPrivate::get(child1)->changeListeners.count(), 0);

    // itemImplicitWidthChanged() x 5
    foreach (TestListener *listener, listeners)
        QQuickItemPrivate::get(parent)->addItemChangeListener(listener, QQuickItemPrivate::ImplicitWidth);
    QCOMPARE(QQuickItemPrivate::get(parent)->changeListeners.count(), listeners.count());
    parent->setImplicitWidth(parent->implicitWidth() + 1);
    foreach (TestListener *listener, listeners)
        QCOMPARE(listener->count(QQuickItemPrivate::ImplicitWidth), 1);
    QCOMPARE(QQuickItemPrivate::get(parent)->changeListeners.count(), 0);

    // itemImplicitHeightChanged() x 5
    foreach (TestListener *listener, listeners)
        QQuickItemPrivate::get(parent)->addItemChangeListener(listener, QQuickItemPrivate::ImplicitHeight);
    QCOMPARE(QQuickItemPrivate::get(parent)->changeListeners.count(), listeners.count());
    parent->setImplicitHeight(parent->implicitHeight() + 1);
    foreach (TestListener *listener, listeners)
        QCOMPARE(listener->count(QQuickItemPrivate::ImplicitHeight), 1);
    QCOMPARE(QQuickItemPrivate::get(parent)->changeListeners.count(), 0);

    // itemGeometryChanged() x 5
    foreach (TestListener *listener, listeners)
        QQuickItemPrivate::get(parent)->addItemChangeListener(listener, QQuickItemPrivate::Geometry);
    QCOMPARE(QQuickItemPrivate::get(parent)->changeListeners.count(), listeners.count());
    parent->setWidth(parent->width() + 1);
    foreach (TestListener *listener, listeners)
        QCOMPARE(listener->count(QQuickItemPrivate::Geometry), 1);
    QCOMPARE(QQuickItemPrivate::get(parent)->changeListeners.count(), 0);

    // itemChildRemoved() x 5
    child1->setParentItem(parent);
    foreach (TestListener *listener, listeners)
        QQuickItemPrivate::get(parent)->addItemChangeListener(listener, QQuickItemPrivate::Children);
    QCOMPARE(QQuickItemPrivate::get(parent)->changeListeners.count(), listeners.count());
    delete child1;
    foreach (TestListener *listener, listeners)
        QCOMPARE(listener->count(QQuickItemPrivate::Children), 2);
    QCOMPARE(QQuickItemPrivate::get(parent)->changeListeners.count(), 0);

    // itemDestroyed() x 5
    foreach (TestListener *listener, listeners)
        QQuickItemPrivate::get(parent)->addItemChangeListener(listener, QQuickItemPrivate::Destroyed);
    QCOMPARE(QQuickItemPrivate::get(parent)->changeListeners.count(), listeners.count());
    delete parent;
    foreach (TestListener *listener, listeners)
        QCOMPARE(listener->count(QQuickItemPrivate::Destroyed), 1);
}

// QTBUG-13893
void tst_QQuickItem::transformCrash()
{
    QQuickView *window = new QQuickView(0);
    window->setSource(testFileUrl("transformCrash.qml"));
    window->show();

    delete window;
}

void tst_QQuickItem::implicitSize()
{
    QQuickView *window = new QQuickView(0);
    window->setSource(testFileUrl("implicitsize.qml"));
    window->show();

    QQuickItem *item = qobject_cast<QQuickItem*>(window->rootObject());
    QVERIFY(item);
    QCOMPARE(item->width(), qreal(80));
    QCOMPARE(item->height(), qreal(60));

    QCOMPARE(item->implicitWidth(), qreal(200));
    QCOMPARE(item->implicitHeight(), qreal(100));

    QMetaObject::invokeMethod(item, "resetSize");

    QCOMPARE(item->width(), qreal(200));
    QCOMPARE(item->height(), qreal(100));

    QMetaObject::invokeMethod(item, "changeImplicit");

    QCOMPARE(item->implicitWidth(), qreal(150));
    QCOMPARE(item->implicitHeight(), qreal(80));
    QCOMPARE(item->width(), qreal(150));
    QCOMPARE(item->height(), qreal(80));

    QMetaObject::invokeMethod(item, "assignImplicitBinding");

    QCOMPARE(item->implicitWidth(), qreal(150));
    QCOMPARE(item->implicitHeight(), qreal(80));
    QCOMPARE(item->width(), qreal(150));
    QCOMPARE(item->height(), qreal(80));

    QMetaObject::invokeMethod(item, "increaseImplicit");

    QCOMPARE(item->implicitWidth(), qreal(200));
    QCOMPARE(item->implicitHeight(), qreal(100));
    QCOMPARE(item->width(), qreal(175));
    QCOMPARE(item->height(), qreal(90));

    QMetaObject::invokeMethod(item, "changeImplicit");

    QCOMPARE(item->implicitWidth(), qreal(150));
    QCOMPARE(item->implicitHeight(), qreal(80));
    QCOMPARE(item->width(), qreal(150));
    QCOMPARE(item->height(), qreal(80));

    QMetaObject::invokeMethod(item, "assignUndefinedBinding");

    QCOMPARE(item->implicitWidth(), qreal(150));
    QCOMPARE(item->implicitHeight(), qreal(80));
    QCOMPARE(item->width(), qreal(150));
    QCOMPARE(item->height(), qreal(80));

    QMetaObject::invokeMethod(item, "increaseImplicit");

    QCOMPARE(item->implicitWidth(), qreal(200));
    QCOMPARE(item->implicitHeight(), qreal(100));
    QCOMPARE(item->width(), qreal(175));
    QCOMPARE(item->height(), qreal(90));

    QMetaObject::invokeMethod(item, "changeImplicit");

    QCOMPARE(item->implicitWidth(), qreal(150));
    QCOMPARE(item->implicitHeight(), qreal(80));
    QCOMPARE(item->width(), qreal(150));
    QCOMPARE(item->height(), qreal(80));

    delete window;
}

void tst_QQuickItem::qtbug_16871()
{
    QQmlComponent component(&engine, testFileUrl("qtbug_16871.qml"));
    QObject *o = component.create();
    QVERIFY(o != 0);
    delete o;
}


void tst_QQuickItem::visibleChildren()
{
    QQuickView *window = new QQuickView(0);
    window->setSource(testFileUrl("visiblechildren.qml"));
    window->show();

    QQuickItem *root = qobject_cast<QQuickItem*>(window->rootObject());
    QVERIFY(root);

    QCOMPARE(root->property("test1_1").toBool(), true);
    QCOMPARE(root->property("test1_2").toBool(), true);
    QCOMPARE(root->property("test1_3").toBool(), true);
    QCOMPARE(root->property("test1_4").toBool(), true);

    QMetaObject::invokeMethod(root, "hideFirstAndLastRowChild");
    QCOMPARE(root->property("test2_1").toBool(), true);
    QCOMPARE(root->property("test2_2").toBool(), true);
    QCOMPARE(root->property("test2_3").toBool(), true);
    QCOMPARE(root->property("test2_4").toBool(), true);

    QMetaObject::invokeMethod(root, "showLastRowChildsLastChild");
    QCOMPARE(root->property("test3_1").toBool(), true);
    QCOMPARE(root->property("test3_2").toBool(), true);
    QCOMPARE(root->property("test3_3").toBool(), true);
    QCOMPARE(root->property("test3_4").toBool(), true);

    QMetaObject::invokeMethod(root, "showLastRowChild");
    QCOMPARE(root->property("test4_1").toBool(), true);
    QCOMPARE(root->property("test4_2").toBool(), true);
    QCOMPARE(root->property("test4_3").toBool(), true);
    QCOMPARE(root->property("test4_4").toBool(), true);

    QString warning1 = testFileUrl("visiblechildren.qml").toString() + ":87: TypeError: Cannot read property 'visibleChildren' of null";
    QTest::ignoreMessage(QtWarningMsg, qPrintable(warning1));
    QMetaObject::invokeMethod(root, "tryWriteToReadonlyVisibleChildren");

    QMetaObject::invokeMethod(root, "reparentVisibleItem3");
    QCOMPARE(root->property("test6_1").toBool(), true);
    QCOMPARE(root->property("test6_2").toBool(), true);
    QCOMPARE(root->property("test6_3").toBool(), true);
    QCOMPARE(root->property("test6_4").toBool(), true);

    QMetaObject::invokeMethod(root, "reparentImlicitlyInvisibleItem4_1");
    QCOMPARE(root->property("test7_1").toBool(), true);
    QCOMPARE(root->property("test7_2").toBool(), true);
    QCOMPARE(root->property("test7_3").toBool(), true);
    QCOMPARE(root->property("test7_4").toBool(), true);

    // FINALLY TEST THAT EVERYTHING IS AS EXPECTED
    QCOMPARE(root->property("test8_1").toBool(), true);
    QCOMPARE(root->property("test8_2").toBool(), true);
    QCOMPARE(root->property("test8_3").toBool(), true);
    QCOMPARE(root->property("test8_4").toBool(), true);
    QCOMPARE(root->property("test8_5").toBool(), true);

    delete window;
}

void tst_QQuickItem::parentLoop()
{
    QQuickView *window = new QQuickView(0);

#ifndef QT_NO_REGULAREXPRESSION
    QRegularExpression msgRegexp = QRegularExpression("QQuickItem::setParentItem: Parent QQuickItem\\(.*\\) is already part of the subtree of QQuickItem\\(.*\\)");
    QTest::ignoreMessage(QtWarningMsg, msgRegexp);
#endif
    window->setSource(testFileUrl("parentLoop.qml"));

    QQuickItem *root = qobject_cast<QQuickItem*>(window->rootObject());
    QVERIFY(root);

    QQuickItem *item1 = root->findChild<QQuickItem*>("item1");
    QVERIFY(item1);
    QCOMPARE(item1->parentItem(), root);

    QQuickItem *item2 = root->findChild<QQuickItem*>("item2");
    QVERIFY(item2);
    QCOMPARE(item2->parentItem(), item1);

    delete window;
}

void tst_QQuickItem::contains_data()
{
    QTest::addColumn<bool>("circleTest");
    QTest::addColumn<bool>("insideTarget");
    QTest::addColumn<QList<QPoint> >("points");

    QList<QPoint> points;

    points << QPoint(176, 176)
           << QPoint(176, 226)
           << QPoint(226, 176)
           << QPoint(226, 226)
           << QPoint(150, 200)
           << QPoint(200, 150)
           << QPoint(200, 250)
           << QPoint(250, 200);
    QTest::newRow("hollow square: testing points inside") << false << true << points;

    points.clear();
    points << QPoint(162, 162)
           << QPoint(162, 242)
           << QPoint(242, 162)
           << QPoint(242, 242)
           << QPoint(200, 200)
           << QPoint(175, 200)
           << QPoint(200, 175)
           << QPoint(200, 228)
           << QPoint(228, 200)
           << QPoint(200, 122)
           << QPoint(122, 200)
           << QPoint(200, 280)
           << QPoint(280, 200);
    QTest::newRow("hollow square: testing points outside") << false << false << points;

    points.clear();
    points << QPoint(174, 174)
           << QPoint(174, 225)
           << QPoint(225, 174)
           << QPoint(225, 225)
           << QPoint(165, 200)
           << QPoint(200, 165)
           << QPoint(200, 235)
           << QPoint(235, 200);
    QTest::newRow("hollow circle: testing points inside") << true << true << points;

    points.clear();
    points << QPoint(160, 160)
           << QPoint(160, 240)
           << QPoint(240, 160)
           << QPoint(240, 240)
           << QPoint(200, 200)
           << QPoint(185, 185)
           << QPoint(185, 216)
           << QPoint(216, 185)
           << QPoint(216, 216)
           << QPoint(145, 200)
           << QPoint(200, 145)
           << QPoint(255, 200)
           << QPoint(200, 255);
    QTest::newRow("hollow circle: testing points outside") << true << false << points;
}

void tst_QQuickItem::contains()
{
    QFETCH(bool, circleTest);
    QFETCH(bool, insideTarget);
    QFETCH(QList<QPoint>, points);

    QQuickView *window = new QQuickView(0);
    window->rootContext()->setContextProperty("circleShapeTest", circleTest);
    window->setBaseSize(QSize(400, 400));
    window->setSource(testFileUrl("hollowTestItem.qml"));
    window->show();
    window->requestActivate();
    QVERIFY(QTest::qWaitForWindowActive(window));
    QCOMPARE(QGuiApplication::focusWindow(), window);

    QQuickItem *root = qobject_cast<QQuickItem *>(window->rootObject());
    QVERIFY(root);

    HollowTestItem *hollowItem = root->findChild<HollowTestItem *>("hollowItem");
    QVERIFY(hollowItem);

    foreach (const QPoint &point, points) {
        // check mouse hover
        QTest::mouseMove(window, point);
        QTest::qWait(10);
        QCOMPARE(hollowItem->isHovered(), insideTarget);

        // check mouse press
        QTest::mousePress(window, Qt::LeftButton, 0, point);
        QTest::qWait(10);
        QCOMPARE(hollowItem->isPressed(), insideTarget);

        // check mouse release
        QTest::mouseRelease(window, Qt::LeftButton, 0, point);
        QTest::qWait(10);
        QCOMPARE(hollowItem->isPressed(), false);
    }

    delete window;
}

void tst_QQuickItem::childAt()
{
    QQuickItem parent;

    QQuickItem child1;
    child1.setX(0);
    child1.setY(0);
    child1.setWidth(100);
    child1.setHeight(100);
    child1.setParentItem(&parent);

    QQuickItem child2;
    child2.setX(50);
    child2.setY(50);
    child2.setWidth(100);
    child2.setHeight(100);
    child2.setParentItem(&parent);

    QQuickItem child3;
    child3.setX(0);
    child3.setY(200);
    child3.setWidth(50);
    child3.setHeight(50);
    child3.setParentItem(&parent);

    QCOMPARE(parent.childAt(0, 0), &child1);
    QCOMPARE(parent.childAt(0, 99), &child1);
    QCOMPARE(parent.childAt(25, 25), &child1);
    QCOMPARE(parent.childAt(25, 75), &child1);
    QCOMPARE(parent.childAt(75, 25), &child1);
    QCOMPARE(parent.childAt(75, 75), &child2);
    QCOMPARE(parent.childAt(149, 149), &child2);
    QCOMPARE(parent.childAt(25, 200), &child3);
    QCOMPARE(parent.childAt(0, 150), static_cast<QQuickItem *>(0));
    QCOMPARE(parent.childAt(300, 300), static_cast<QQuickItem *>(0));
}

void tst_QQuickItem::grab()
{
    QQuickView view;
    view.setSource(testFileUrl("grabToImage.qml"));
    view.show();
    QTest::qWaitForWindowExposed(&view);

    QQuickItem *root = qobject_cast<QQuickItem *>(view.rootObject());
    QVERIFY(root);
    QQuickItem *item = root->findChild<QQuickItem *>("myItem");
    QVERIFY(item);
#ifndef QT_NO_OPENGL
    { // Default size (item is 100x100)
        QSharedPointer<QQuickItemGrabResult> result = item->grabToImage();
        QSignalSpy spy(result.data(), SIGNAL(ready()));
        QTRY_VERIFY(spy.size() > 0);
        QVERIFY(!result->url().isEmpty());
        QImage image = result->image();
        QCOMPARE(image.pixel(0, 0), qRgb(255, 0, 0));
        QCOMPARE(image.pixel(99, 99), qRgb(0, 0, 255));
    }

    { // Smaller size
        QSharedPointer<QQuickItemGrabResult> result = item->grabToImage(QSize(50, 50));
        QVERIFY(!result.isNull());
        QSignalSpy spy(result.data(), SIGNAL(ready()));
        QTRY_VERIFY(spy.size() > 0);
        QVERIFY(!result->url().isEmpty());
        QImage image = result->image();
        QCOMPARE(image.pixel(0, 0), qRgb(255, 0, 0));
        QCOMPARE(image.pixel(49, 49), qRgb(0, 0, 255));
    }
#endif
}

void tst_QQuickItem::isAncestorOf()
{
    QQuickItem parent;

    QQuickItem sub1;
    sub1.setParentItem(&parent);

    QQuickItem child1;
    child1.setParentItem(&sub1);
    QQuickItem child2;
    child2.setParentItem(&sub1);

    QQuickItem sub2;
    sub2.setParentItem(&parent);

    QQuickItem child3;
    child3.setParentItem(&sub2);
    QQuickItem child4;
    child4.setParentItem(&sub2);

    QVERIFY(parent.isAncestorOf(&sub1));
    QVERIFY(parent.isAncestorOf(&sub2));
    QVERIFY(parent.isAncestorOf(&child1));
    QVERIFY(parent.isAncestorOf(&child2));
    QVERIFY(parent.isAncestorOf(&child3));
    QVERIFY(parent.isAncestorOf(&child4));
    QVERIFY(sub1.isAncestorOf(&child1));
    QVERIFY(sub1.isAncestorOf(&child2));
    QVERIFY(!sub1.isAncestorOf(&child3));
    QVERIFY(!sub1.isAncestorOf(&child4));
    QVERIFY(sub2.isAncestorOf(&child3));
    QVERIFY(sub2.isAncestorOf(&child4));
    QVERIFY(!sub2.isAncestorOf(&child1));
    QVERIFY(!sub2.isAncestorOf(&child2));
    QVERIFY(!sub1.isAncestorOf(&sub1));
    QVERIFY(!sub2.isAncestorOf(&sub2));
}

QTEST_MAIN(tst_QQuickItem)

#include "tst_qquickitem.moc"
