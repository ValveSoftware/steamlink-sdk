/****************************************************************************
**
** Copyright (C) 2015 The Qt Company Ltd.
** Contact: http://www.qt.io/licensing/
**
** This file is part of the test suite of the Qt Toolkit.
**
** $QT_BEGIN_LICENSE:LGPL3$
** Commercial License Usage
** Licensees holding valid commercial Qt licenses may use this file in
** accordance with the commercial license agreement provided with the
** Software or, alternatively, in accordance with the terms contained in
** a written agreement between you and The Qt Company. For licensing terms
** and conditions see http://www.qt.io/terms-conditions. For further
** information use the contact form at http://www.qt.io/contact-us.
**
** GNU Lesser General Public License Usage
** Alternatively, this file may be used under the terms of the GNU Lesser
** General Public License version 3 as published by the Free Software
** Foundation and appearing in the file LICENSE.LGPLv3 included in the
** packaging of this file. Please review the following information to
** ensure the GNU Lesser General Public License version 3 requirements
** will be met: https://www.gnu.org/licenses/lgpl.html.
**
** GNU General Public License Usage
** Alternatively, this file may be used under the terms of the GNU
** General Public License version 2.0 or later as published by the Free
** Software Foundation and appearing in the file LICENSE.GPL included in
** the packaging of this file. Please review the following information to
** ensure the GNU General Public License version 2.0 requirements will be
** met: http://www.gnu.org/licenses/gpl-2.0.html.
**
** $QT_END_LICENSE$
**
****************************************************************************/

#include <qtest.h>
#include <QtTest/QSignalSpy>
#include <QtQml/qqmlengine.h>
#include <QtQml/qqmlcomponent.h>
#include <QtQml/qqmlcontext.h>
#include <QtQuick/qquickview.h>
#include <QtQuick/private/qquickitem_p.h>
#include <QtGui/private/qguiapplication_p.h>
#include <QtQuickTemplates2/private/qquickapplicationwindow_p.h>
#include <QtQuickTemplates2/private/qquickoverlay_p.h>
#include <QtQuickTemplates2/private/qquickcontrol_p.h>
#include <QtQuickTemplates2/private/qquicklabel_p.h>
#include <QtQuickTemplates2/private/qquicktextarea_p.h>
#include <QtQuickTemplates2/private/qquicktextfield_p.h>
#include <QtQuickControls2/private/qquickproxytheme_p.h>
#include "../shared/util.h"
#include "../shared/visualtestutil.h"

using namespace QQuickVisualTestUtil;

class tst_applicationwindow : public QQmlDataTest
{
    Q_OBJECT
public:

private slots:
    void qmlCreation();
    void activeFocusOnTab1();
    void activeFocusOnTab2();
    void defaultFocus();
    void implicitFill();
    void attachedProperties();
    void font();
    void defaultFont();
    void locale();
    void activeFocusControl_data();
    void activeFocusControl();
    void focusAfterPopupClosed();
    void layout();
};

void tst_applicationwindow::qmlCreation()
{
    QQmlEngine engine;
    QQmlComponent component(&engine);
    component.loadUrl(testFileUrl("basicapplicationwindow.qml"));
    QObject* created = component.create();
    QScopedPointer<QObject> cleanup(created);
    QVERIFY(created);

    QQuickWindow* window = qobject_cast<QQuickWindow*>(created);
    QVERIFY(window);
    QVERIFY(!window->isVisible());

    QCOMPARE(created->property("title"), QVariant("Test Application Window"));

    QQuickItem* statusBar = qvariant_cast<QQuickItem*>(created->property("statusBar"));
    QVERIFY(!statusBar);

    QQuickItem* header = qvariant_cast<QQuickItem*>(created->property("header"));
    QVERIFY(!header);

    QQuickItem* footer = qvariant_cast<QQuickItem*>(created->property("footer"));
    QVERIFY(!footer);
}

void tst_applicationwindow::activeFocusOnTab1()
{
    QQmlEngine engine;
    QQmlComponent component(&engine);
    component.loadUrl(testFileUrl("activefocusontab.qml"));
    QObject* created = component.create();
    QScopedPointer<QObject> cleanup(created);
    QVERIFY(created);

    QQuickWindow* window = qobject_cast<QQuickWindow*>(created);
    QVERIFY(window);
    window->show();
    window->requestActivate();
    QVERIFY(QTest::qWaitForWindowActive(window));
    QVERIFY(QGuiApplication::focusWindow() == window);

    QQuickItem* contentItem = window->contentItem();
    QVERIFY(contentItem);
    QVERIFY(contentItem->hasActiveFocus());

    QQuickItem* item = findItem<QQuickItem>(window->contentItem(), "sub1");
    QVERIFY(item);
    QVERIFY(!item->hasActiveFocus());

    // Tab: contentItem->sub1
    QKeyEvent key(QEvent::KeyPress, Qt::Key_Tab, Qt::NoModifier, "", false, 1);
    QGuiApplication::sendEvent(window, &key);
    QVERIFY(key.isAccepted());

    item = findItem<QQuickItem>(window->contentItem(), "sub1");
    QVERIFY(item);
    QVERIFY(item->hasActiveFocus());

    // Tab: sub1->sub2
    key = QKeyEvent(QEvent::KeyPress, Qt::Key_Tab, Qt::NoModifier, "", false, 1);
    QGuiApplication::sendEvent(window, &key);
    QVERIFY(key.isAccepted());

    item = findItem<QQuickItem>(window->contentItem(), "sub2");
    QVERIFY(item);
    QVERIFY(item->hasActiveFocus());

    // Tab: sub2->sub1
    key = QKeyEvent(QEvent::KeyPress, Qt::Key_Tab, Qt::NoModifier, "", false, 1);
    QGuiApplication::sendEvent(window, &key);
    QVERIFY(key.isAccepted());

    item = findItem<QQuickItem>(window->contentItem(), "sub1");
    QVERIFY(item);
    QVERIFY(item->hasActiveFocus());
}

void tst_applicationwindow::activeFocusOnTab2()
{
    QQmlEngine engine;
    QQmlComponent component(&engine);
    component.loadUrl(testFileUrl("activefocusontab.qml"));
    QObject* created = component.create();
    QScopedPointer<QObject> cleanup(created);
    QVERIFY(created);

    QQuickWindow* window = qobject_cast<QQuickWindow*>(created);
    QVERIFY(window);
    window->show();
    window->requestActivate();
    QVERIFY(QTest::qWaitForWindowActive(window));
    QVERIFY(QGuiApplication::focusWindow() == window);

    QQuickItem* contentItem = window->contentItem();
    QVERIFY(contentItem);
    QVERIFY(contentItem->hasActiveFocus());

    QQuickItem* item = findItem<QQuickItem>(window->contentItem(), "sub2");
    QVERIFY(item);
    QVERIFY(!item->hasActiveFocus());

    // BackTab: contentItem->sub2
    QKeyEvent key(QEvent::KeyPress, Qt::Key_Tab, Qt::ShiftModifier, "", false, 1);
    QGuiApplication::sendEvent(window, &key);
    QVERIFY(key.isAccepted());

    item = findItem<QQuickItem>(window->contentItem(), "sub2");
    QVERIFY(item);
    QVERIFY(item->hasActiveFocus());

    // BackTab: sub2->sub1
    key = QKeyEvent(QEvent::KeyPress, Qt::Key_Tab, Qt::ShiftModifier, "", false, 1);
    QGuiApplication::sendEvent(window, &key);
    QVERIFY(key.isAccepted());

    item = findItem<QQuickItem>(window->contentItem(), "sub1");
    QVERIFY(item);
    QVERIFY(item->hasActiveFocus());

    // BackTab: sub1->sub2
    key = QKeyEvent(QEvent::KeyPress, Qt::Key_Tab, Qt::ShiftModifier, "", false, 1);
    QGuiApplication::sendEvent(window, &key);
    QVERIFY(key.isAccepted());

    item = findItem<QQuickItem>(window->contentItem(), "sub2");
    QVERIFY(item);
    QVERIFY(item->hasActiveFocus());
}

void tst_applicationwindow::defaultFocus()
{
    QQmlEngine engine;
    QQmlComponent component(&engine);
    component.loadUrl(testFileUrl("defaultFocus.qml"));
    QObject* created = component.create();
    QScopedPointer<QObject> cleanup(created);
    Q_UNUSED(cleanup);
    QVERIFY(created);

    QQuickWindow* window = qobject_cast<QQuickWindow*>(created);
    QVERIFY(window);
    window->show();
    window->requestActivate();
    QVERIFY(QTest::qWaitForWindowActive(window));
    QVERIFY(QGuiApplication::focusWindow() == window);

    QQuickItem* contentItem = window->contentItem();
    QVERIFY(contentItem);
    QVERIFY(contentItem->hasActiveFocus());

    // A single item in an ApplicationWindow with focus: true should receive focus.
    QQuickItem* item = findItem<QQuickItem>(window->contentItem(), "item");
    QVERIFY(item);
    QVERIFY(item->hasFocus());
    QVERIFY(item->hasActiveFocus());
}

void tst_applicationwindow::implicitFill()
{
    QQmlEngine engine;
    QQmlComponent component(&engine);
    component.loadUrl(testFileUrl("fill.qml"));
    QObject* created = component.create();
    QScopedPointer<QObject> cleanup(created);
    QVERIFY(created);

    QQuickWindow* window = qobject_cast<QQuickWindow*>(created);
    QVERIFY(window);
    QVERIFY(!window->isVisible());
    QCOMPARE(window->width(), 400);
    QCOMPARE(window->height(), 400);

    window->show();
    QVERIFY(QTest::qWaitForWindowActive(window));

    QQuickItem *stackView = window->property("stackView").value<QQuickItem*>();
    QVERIFY(stackView);
    QCOMPARE(stackView->width(), 400.0);
    QCOMPARE(stackView->height(), 400.0);

    QQuickItem *nextItem = window->property("nextItem").value<QQuickItem*>();
    QVERIFY(nextItem);

    QVERIFY(QMetaObject::invokeMethod(window, "pushNextItem"));
    QCOMPARE(nextItem->width(), 400.0);
    QCOMPARE(nextItem->height(), 400.0);
}

void tst_applicationwindow::attachedProperties()
{
    QQmlEngine engine;
    QQmlComponent component(&engine);
    component.loadUrl(testFileUrl("attachedProperties.qml"));

    QScopedPointer<QObject> object(component.create());
    QVERIFY2(!object.isNull(), qPrintable(component.errorString()));

    QQuickApplicationWindow *window = qobject_cast<QQuickApplicationWindow *>(object.data());
    QVERIFY(window);

    QQuickItem *childControl = object->property("childControl").value<QQuickItem *>();
    QVERIFY(childControl);
    QCOMPARE(childControl->property("attached_window").value<QQuickApplicationWindow *>(), window);
    QCOMPARE(childControl->property("attached_contentItem").value<QQuickItem *>(), window->contentItem());
    QCOMPARE(childControl->property("attached_activeFocusControl").value<QQuickItem *>(), window->activeFocusControl());
    QCOMPARE(childControl->property("attached_header").value<QQuickItem *>(), window->header());
    QCOMPARE(childControl->property("attached_footer").value<QQuickItem *>(), window->footer());
    QCOMPARE(childControl->property("attached_overlay").value<QQuickItem *>(), window->overlay());

    QQuickItem *childItem = object->property("childItem").value<QQuickItem *>();
    QVERIFY(childItem);
    QCOMPARE(childItem->property("attached_window").value<QQuickApplicationWindow *>(), window);
    QCOMPARE(childItem->property("attached_contentItem").value<QQuickItem *>(), window->contentItem());
    QCOMPARE(childItem->property("attached_activeFocusControl").value<QQuickItem *>(), window->activeFocusControl());
    QCOMPARE(childItem->property("attached_header").value<QQuickItem *>(), window->header());
    QCOMPARE(childItem->property("attached_footer").value<QQuickItem *>(), window->footer());
    QCOMPARE(childItem->property("attached_overlay").value<QQuickItem *>(), window->overlay());

    QObject *childObject = object->property("childObject").value<QObject *>();
    QVERIFY(childObject);
    QVERIFY(!childObject->property("attached_window").value<QQuickApplicationWindow *>());
    QVERIFY(!childObject->property("attached_contentItem").value<QQuickItem *>());
    QVERIFY(!childObject->property("attached_activeFocusControl").value<QQuickItem *>());
    QVERIFY(!childObject->property("attached_header").value<QQuickItem *>());
    QVERIFY(!childObject->property("attached_footer").value<QQuickItem *>());
    QVERIFY(!childObject->property("attached_overlay").value<QQuickItem *>());

    QQuickWindow *childWindow = object->property("childWindow").value<QQuickWindow *>();
    QVERIFY(childWindow);
    QVERIFY(!childWindow->property("attached_window").value<QQuickApplicationWindow *>());
    QVERIFY(!childWindow->property("attached_contentItem").value<QQuickItem *>());
    QVERIFY(!childWindow->property("attached_activeFocusControl").value<QQuickItem *>());
    QVERIFY(!childWindow->property("attached_header").value<QQuickItem *>());
    QVERIFY(!childWindow->property("attached_footer").value<QQuickItem *>());
    QVERIFY(!childWindow->property("attached_overlay").value<QQuickItem *>());

    QQuickItem *childWindowControl = object->property("childWindowControl").value<QQuickItem *>();
    QVERIFY(childWindowControl);
    QVERIFY(!childWindowControl->property("attached_window").value<QQuickApplicationWindow *>());
    QVERIFY(!childWindowControl->property("attached_contentItem").value<QQuickItem *>());
    QVERIFY(!childWindowControl->property("attached_activeFocusControl").value<QQuickItem *>());
    QVERIFY(!childWindowControl->property("attached_header").value<QQuickItem *>());
    QVERIFY(!childWindowControl->property("attached_footer").value<QQuickItem *>());
    QCOMPARE(childWindowControl->property("attached_overlay").value<QQuickItem *>(), QQuickOverlay::overlay(childWindow));

    QQuickItem *childWindowItem = object->property("childWindowItem").value<QQuickItem *>();
    QVERIFY(childWindowItem);
    QVERIFY(!childWindowItem->property("attached_window").value<QQuickApplicationWindow *>());
    QVERIFY(!childWindowItem->property("attached_contentItem").value<QQuickItem *>());
    QVERIFY(!childWindowItem->property("attached_activeFocusControl").value<QQuickItem *>());
    QVERIFY(!childWindowItem->property("attached_header").value<QQuickItem *>());
    QVERIFY(!childWindowItem->property("attached_footer").value<QQuickItem *>());
    QCOMPARE(childWindowItem->property("attached_overlay").value<QQuickItem *>(), QQuickOverlay::overlay(childWindow));

    QObject *childWindowObject = object->property("childWindowObject").value<QObject *>();
    QVERIFY(childWindowObject);
    QVERIFY(!childWindowObject->property("attached_window").value<QQuickApplicationWindow *>());
    QVERIFY(!childWindowObject->property("attached_contentItem").value<QQuickItem *>());
    QVERIFY(!childWindowObject->property("attached_activeFocusControl").value<QQuickItem *>());
    QVERIFY(!childWindowObject->property("attached_header").value<QQuickItem *>());
    QVERIFY(!childWindowObject->property("attached_footer").value<QQuickItem *>());
    QVERIFY(!childWindowObject->property("attached_overlay").value<QQuickItem *>());

    QQuickApplicationWindow *childAppWindow = object->property("childAppWindow").value<QQuickApplicationWindow *>();
    QVERIFY(childAppWindow);
    QVERIFY(!childAppWindow->property("attached_window").value<QQuickApplicationWindow *>());
    QVERIFY(!childAppWindow->property("attached_contentItem").value<QQuickItem *>());
    QVERIFY(!childAppWindow->property("attached_activeFocusControl").value<QQuickItem *>());
    QVERIFY(!childAppWindow->property("attached_header").value<QQuickItem *>());
    QVERIFY(!childAppWindow->property("attached_footer").value<QQuickItem *>());
    QVERIFY(!childAppWindow->property("attached_overlay").value<QQuickItem *>());

    QQuickItem *childAppWindowControl = object->property("childAppWindowControl").value<QQuickItem *>();
    QVERIFY(childAppWindowControl);
    QCOMPARE(childAppWindowControl->property("attached_window").value<QQuickApplicationWindow *>(), childAppWindow);
    QCOMPARE(childAppWindowControl->property("attached_contentItem").value<QQuickItem *>(), childAppWindow->contentItem());
    QCOMPARE(childAppWindowControl->property("attached_activeFocusControl").value<QQuickItem *>(), childAppWindow->activeFocusControl());
    QCOMPARE(childAppWindowControl->property("attached_header").value<QQuickItem *>(), childAppWindow->header());
    QCOMPARE(childAppWindowControl->property("attached_footer").value<QQuickItem *>(), childAppWindow->footer());
    QCOMPARE(childAppWindowControl->property("attached_overlay").value<QQuickItem *>(), childAppWindow->overlay());

    QQuickItem *childAppWindowItem = object->property("childAppWindowItem").value<QQuickItem *>();
    QVERIFY(childAppWindowItem);
    QCOMPARE(childAppWindowItem->property("attached_window").value<QQuickApplicationWindow *>(), childAppWindow);
    QCOMPARE(childAppWindowItem->property("attached_contentItem").value<QQuickItem *>(), childAppWindow->contentItem());
    QCOMPARE(childAppWindowItem->property("attached_activeFocusControl").value<QQuickItem *>(), childAppWindow->activeFocusControl());
    QCOMPARE(childAppWindowItem->property("attached_header").value<QQuickItem *>(), childAppWindow->header());
    QCOMPARE(childAppWindowItem->property("attached_footer").value<QQuickItem *>(), childAppWindow->footer());
    QCOMPARE(childAppWindowItem->property("attached_overlay").value<QQuickItem *>(), childAppWindow->overlay());

    QObject *childAppWindowObject = object->property("childAppWindowObject").value<QObject *>();
    QVERIFY(childAppWindowObject);
    QVERIFY(!childAppWindowObject->property("attached_window").value<QQuickApplicationWindow *>());
    QVERIFY(!childAppWindowObject->property("attached_contentItem").value<QQuickItem *>());
    QVERIFY(!childAppWindowObject->property("attached_activeFocusControl").value<QQuickItem *>());
    QVERIFY(!childAppWindowObject->property("attached_header").value<QQuickItem *>());
    QVERIFY(!childAppWindowObject->property("attached_footer").value<QQuickItem *>());
    QVERIFY(!childAppWindowObject->property("attached_overlay").value<QQuickItem *>());

    window->show();
    window->requestActivate();
    QVERIFY(QTest::qWaitForWindowActive(window));

    QVERIFY(!childControl->hasActiveFocus());
    childControl->forceActiveFocus();
    QTRY_VERIFY(childControl->hasActiveFocus());
    QCOMPARE(window->activeFocusItem(), childControl);
    QCOMPARE(childControl->property("attached_activeFocusControl").value<QQuickItem *>(), childControl);

    QQuickItem *header = new QQuickItem;
    window->setHeader(header);
    QCOMPARE(window->header(), header);
    QCOMPARE(childControl->property("attached_header").value<QQuickItem *>(), header);

    QQuickItem *footer = new QQuickItem;
    window->setFooter(footer);
    QCOMPARE(window->footer(), footer);
    QCOMPARE(childControl->property("attached_footer").value<QQuickItem *>(), footer);

    childAppWindow->show();
    childAppWindow->requestActivate();
    QVERIFY(QTest::qWaitForWindowActive(childAppWindow));

    QVERIFY(!childAppWindowControl->hasActiveFocus());
    childAppWindowControl->forceActiveFocus();
    QTRY_VERIFY(childAppWindowControl->hasActiveFocus());
    QCOMPARE(childAppWindow->activeFocusItem(), childAppWindowControl);
    QCOMPARE(childAppWindowControl->property("attached_activeFocusControl").value<QQuickItem *>(), childAppWindowControl);

    childControl->setParentItem(childAppWindow->contentItem());
    QCOMPARE(childControl->window(), childAppWindow);
    QCOMPARE(childControl->property("attached_window").value<QQuickApplicationWindow *>(), childAppWindow);
    QCOMPARE(childControl->property("attached_contentItem").value<QQuickItem *>(), childAppWindow->contentItem());
    QCOMPARE(childControl->property("attached_activeFocusControl").value<QQuickItem *>(), childAppWindow->activeFocusControl());
    QCOMPARE(childControl->property("attached_header").value<QQuickItem *>(), childAppWindow->header());
    QCOMPARE(childControl->property("attached_footer").value<QQuickItem *>(), childAppWindow->footer());
    QCOMPARE(childControl->property("attached_overlay").value<QQuickItem *>(), childAppWindow->overlay());

    childItem->setParentItem(childAppWindow->contentItem());
    QCOMPARE(childItem->window(), childAppWindow);
    QCOMPARE(childItem->property("attached_window").value<QQuickApplicationWindow *>(), childAppWindow);
    QCOMPARE(childItem->property("attached_contentItem").value<QQuickItem *>(), childAppWindow->contentItem());
    QCOMPARE(childItem->property("attached_activeFocusControl").value<QQuickItem *>(), childAppWindow->activeFocusControl());
    QCOMPARE(childItem->property("attached_header").value<QQuickItem *>(), childAppWindow->header());
    QCOMPARE(childItem->property("attached_footer").value<QQuickItem *>(), childAppWindow->footer());
    QCOMPARE(childItem->property("attached_overlay").value<QQuickItem *>(), childAppWindow->overlay());

    childControl->setParentItem(nullptr);
    QVERIFY(!childControl->window());
    QVERIFY(!childControl->property("attached_window").value<QQuickApplicationWindow *>());
    QVERIFY(!childControl->property("attached_contentItem").value<QQuickItem *>());
    QVERIFY(!childControl->property("attached_activeFocusControl").value<QQuickItem *>());
    QVERIFY(!childControl->property("attached_header").value<QQuickItem *>());
    QVERIFY(!childControl->property("attached_footer").value<QQuickItem *>());
    QVERIFY(!childControl->property("attached_overlay").value<QQuickItem *>());

    childItem->setParentItem(nullptr);
    QVERIFY(!childItem->window());
    QVERIFY(!childItem->property("attached_window").value<QQuickApplicationWindow *>());
    QVERIFY(!childItem->property("attached_contentItem").value<QQuickItem *>());
    QVERIFY(!childItem->property("attached_activeFocusControl").value<QQuickItem *>());
    QVERIFY(!childItem->property("attached_header").value<QQuickItem *>());
    QVERIFY(!childItem->property("attached_footer").value<QQuickItem *>());
    QVERIFY(!childItem->property("attached_overlay").value<QQuickItem *>());

    // ### A temporary workaround to unblock the CI until the crash caused
    // by https://codereview.qt-project.org/#/c/108517/ has been fixed...
    window->hide();
    qApp->processEvents();
}

void tst_applicationwindow::font()
{
    QQmlEngine engine;
    QQmlComponent component(&engine);
    component.loadUrl(testFileUrl("font.qml"));
    QObject* created = component.create();
    QScopedPointer<QObject> cleanup(created);
    QVERIFY(created);

    QQuickApplicationWindow* window = qobject_cast<QQuickApplicationWindow*>(created);
    QVERIFY(window);
    QVERIFY(!window->isVisible());
    QCOMPARE(window->width(), 400);
    QCOMPARE(window->height(), 400);

    window->show();
    QVERIFY(QTest::qWaitForWindowActive(window));

    QFont font = window->font();

    QQuickControl *mainItem = window->property("mainItem").value<QQuickControl*>();
    QVERIFY(mainItem);
    QCOMPARE(mainItem->width(), 400.0);
    QCOMPARE(mainItem->height(), 400.0);
    QCOMPARE(mainItem->font(), font);

    QQuickControl *item2 = mainItem->property("item_2").value<QQuickControl*>();
    QVERIFY(item2);
    QQuickControl *item3 = mainItem->property("item_3").value<QQuickControl*>();
    QVERIFY(item3);
    QQuickTextArea *item4 = mainItem->property("item_4").value<QQuickTextArea*>();
    QVERIFY(item4);
    QQuickTextField *item5 = mainItem->property("item_5").value<QQuickTextField*>();
    QVERIFY(item5);
    QQuickLabel *item6 = mainItem->property("item_6").value<QQuickLabel*>();
    QVERIFY(item6);

    QCOMPARE(item2->font(), font);
    QCOMPARE(item3->font(), font);
    QCOMPARE(item4->font(), font);
    QCOMPARE(item5->font(), font);
    QCOMPARE(item6->font(), font);

    int pointSize = font.pointSize();
    font.setPixelSize(pointSize + 5);
    window->setFont(font);

    QCOMPARE(window->font(), font);
    QCOMPARE(mainItem->font(), font);
    QCOMPARE(item2->font(), font);
    QCOMPARE(item3->font(), font);
    QCOMPARE(item4->font(), font);
    QCOMPARE(item5->font(), font);
    QCOMPARE(item6->font(), font);
}

class TestTheme :  public QQuickProxyTheme
{
public:
    TestTheme(QPlatformTheme *theme) : QQuickProxyTheme(theme), m_font("Courier")
    { QGuiApplicationPrivate::platform_theme = this; }

    const QFont *font(Font type = SystemFont) const override
    {
        Q_UNUSED(type);
        return &m_font;
    }

    QFont m_font;
};

void tst_applicationwindow::defaultFont()
{
    TestTheme theme(QGuiApplicationPrivate::platform_theme);

    QQmlEngine engine;
    QQmlComponent component(&engine);
    component.setData("import QtQuick.Controls 2.1; ApplicationWindow { }", QUrl());

    QScopedPointer<QQuickApplicationWindow> window;
    window.reset(static_cast<QQuickApplicationWindow *>(component.create()));
    QVERIFY(!window.isNull());
    QCOMPARE(window->font(), *theme.font());
}

void tst_applicationwindow::locale()
{
    QQmlEngine engine;
    QQmlComponent component(&engine);
    component.loadUrl(testFileUrl("locale.qml"));
    QObject* created = component.create();
    QScopedPointer<QObject> cleanup(created);
    QVERIFY(created);

    QQuickApplicationWindow* window = qobject_cast<QQuickApplicationWindow*>(created);
    QVERIFY(window);
    QVERIFY(!window->isVisible());
    QCOMPARE(window->width(), 400);
    QCOMPARE(window->height(), 400);

    window->show();
    QVERIFY(QTest::qWaitForWindowActive(window));

    QLocale l = window->locale();

    QQuickControl *mainItem = window->property("mainItem").value<QQuickControl*>();
    QVERIFY(mainItem);
    QCOMPARE(mainItem->width(), 400.0);
    QCOMPARE(mainItem->height(), 400.0);
    QCOMPARE(mainItem->locale(), l);

    QQuickControl *item2 = mainItem->property("item_2").value<QQuickControl*>();
    QVERIFY(item2);
    QQuickControl *item3 = mainItem->property("item_3").value<QQuickControl*>();
    QVERIFY(item3);

    QCOMPARE(item2->locale(), l);
    QCOMPARE(item3->locale(), l);

    l = QLocale("en_US");
    window->setLocale(l);

    QCOMPARE(window->locale(), l);
    QCOMPARE(mainItem->locale(), l);
    QCOMPARE(item2->locale(), l);
    QCOMPARE(item3->locale(), l);

    l = QLocale("ar_EG");
    window->setLocale(l);

    QCOMPARE(window->locale(), l);
    QCOMPARE(mainItem->locale(), l);
    QCOMPARE(item2->locale(), l);
    QCOMPARE(item3->locale(), l);
}

void tst_applicationwindow::activeFocusControl_data()
{
    QTest::addColumn<QByteArray>("containerName");
    QTest::addColumn<QByteArray>("activeFocusItemName");
    QTest::addColumn<QByteArray>("activeFocusControlName");

    QTest::newRow("Column:TextInput") << QByteArray("container_column") << QByteArray("textInput_column") << QByteArray();
    QTest::newRow("Column:TextEdit") << QByteArray("container_column") << QByteArray("textEdit_column") << QByteArray();
    QTest::newRow("Column:TextField") << QByteArray("container_column") << QByteArray("textField_column") << QByteArray("textField_column");
    QTest::newRow("Column:TextArea") << QByteArray("container_column") << QByteArray("textArea_column") << QByteArray("textArea_column");
    QTest::newRow("Column:SpinBox") << QByteArray("container_column") << QByteArray("spinContent_column") << QByteArray("spinBox_column");

    QTest::newRow("Frame:TextInput") << QByteArray("container_frame") << QByteArray("textInput_frame") << QByteArray("container_frame");
    QTest::newRow("Frame:TextEdit") << QByteArray("container_frame") << QByteArray("textEdit_frame") << QByteArray("container_frame");
    QTest::newRow("Frame:TextField") << QByteArray("container_frame") << QByteArray("textField_frame") << QByteArray("textField_frame");
    QTest::newRow("Frame:TextArea") << QByteArray("container_frame") << QByteArray("textArea_frame") << QByteArray("textArea_frame");
    QTest::newRow("Frame:SpinBox") << QByteArray("container_frame") << QByteArray("spinContent_frame") << QByteArray("spinBox_frame");
}

void tst_applicationwindow::activeFocusControl()
{
    QFETCH(QByteArray, containerName);
    QFETCH(QByteArray, activeFocusItemName);
    QFETCH(QByteArray, activeFocusControlName);

    QQmlEngine engine;
    QQmlComponent component(&engine);
    component.loadUrl(testFileUrl("activeFocusControl.qml"));
    QScopedPointer<QObject> object(component.create());
    QVERIFY(!object.isNull());

    QQuickApplicationWindow* window = qobject_cast<QQuickApplicationWindow*>(object.data());
    QVERIFY(window);
    QVERIFY(!window->isVisible());
    QCOMPARE(window->width(), 400);
    QCOMPARE(window->height(), 400);

    window->show();
    QVERIFY(QTest::qWaitForWindowActive(window));

    QQuickItem *container = window->property(containerName).value<QQuickItem*>();
    QVERIFY(container);

    QQuickItem *activeFocusItem = window->property(activeFocusItemName).value<QQuickItem*>();
    QVERIFY(activeFocusItem);
    activeFocusItem->forceActiveFocus();
    QVERIFY(activeFocusItem->hasActiveFocus());
    QCOMPARE(window->activeFocusItem(), activeFocusItem);

    QQuickItem *activeFocusControl = window->property(activeFocusControlName).value<QQuickItem*>();
    if (activeFocusControlName.isEmpty()) {
        QVERIFY(!activeFocusControl);
    } else {
        QVERIFY(activeFocusControl);
        QVERIFY(activeFocusControl->hasActiveFocus());
    }
    QCOMPARE(window->activeFocusControl(), activeFocusControl);
}

void tst_applicationwindow::focusAfterPopupClosed()
{
    QQmlEngine engine;
    QQmlComponent component(&engine);
    component.loadUrl(testFileUrl("focusAfterPopupClosed.qml"));
    QScopedPointer<QQuickWindow> window(qobject_cast<QQuickWindow*>(component.create()));
    QVERIFY(window);

    window->show();
    window->requestActivate();
    QVERIFY(QTest::qWaitForWindowActive(window.data()));
    QVERIFY(QGuiApplication::focusWindow() == window.data());

    QQuickItem* contentItem = window->contentItem();
    QVERIFY(contentItem);
    QVERIFY(contentItem->hasActiveFocus());

    QQuickItem* focusScope = window->property("focusScope").value<QQuickItem*>();
    QVERIFY(focusScope);
    QVERIFY(focusScope->hasActiveFocus());

    QSignalSpy spy(window.data(), SIGNAL(focusScopeKeyPressed()));
    QTest::keyClick(window.data(), Qt::Key_Space);
    QCOMPARE(spy.count(), 1);

    // Open the menu.
    QQuickItem* toolButton = window->property("toolButton").value<QQuickItem*>();
    QVERIFY(toolButton);
    QTest::mouseClick(window.data(), Qt::LeftButton, Qt::NoModifier,
        toolButton->mapFromScene(QPointF(toolButton->width() / 2, toolButton->height() / 2)).toPoint());
    QVERIFY(!focusScope->hasActiveFocus());

    // The FocusScope shouldn't receive any key events while the menu is open.
    QTest::keyClick(window.data(), Qt::Key_Space);
    QCOMPARE(spy.count(), 1);

    // Close the menu. The FocusScope should regain focus.
    QTest::keyClick(window.data(), Qt::Key_Escape);
    QVERIFY(focusScope->hasActiveFocus());

    QTest::keyClick(window.data(), Qt::Key_Space);
    QCOMPARE(spy.count(), 2);
}

void tst_applicationwindow::layout()
{
    QQmlEngine engine;
    QQmlComponent component(&engine);
    component.loadUrl(testFileUrl("layout.qml"));
    QScopedPointer<QObject> object(component.create());
    QVERIFY(!object.isNull());

    QQuickApplicationWindow* window = qobject_cast<QQuickApplicationWindow*>(object.data());
    QVERIFY(window);
    QVERIFY(QTest::qWaitForWindowExposed(window));

    QQuickItem *content = window->contentItem();
    QVERIFY(content);
    QQuickItem *header = window->header();
    QVERIFY(header);
    QQuickItem *footer = window->footer();
    QVERIFY(footer);

    QCOMPARE(header->x(), 0.0);
    QCOMPARE(header->y(), -header->height());
    QCOMPARE(header->width(), qreal(window->width()));
    QVERIFY(header->height() > 0);

    QCOMPARE(footer->x(), 0.0);
    QCOMPARE(footer->y(), content->height());
    QCOMPARE(footer->width(), qreal(window->width()));
    QVERIFY(footer->height() > 0.0);

    QCOMPARE(content->x(), 0.0);
    QCOMPARE(content->y(), header->height());
    QCOMPARE(content->width(), qreal(window->width()));
    QCOMPARE(content->height(), window->height() - header->height() - footer->height());

    header->setVisible(false);
    QCOMPARE(content->x(), 0.0);
    QCOMPARE(content->y(), 0.0);
    QCOMPARE(content->width(), qreal(window->width()));
    QCOMPARE(content->height(), window->height() - footer->height());

    footer->setVisible(false);
    QCOMPARE(content->x(), 0.0);
    QCOMPARE(content->y(), 0.0);
    QCOMPARE(content->width(), qreal(window->width()));
    QCOMPARE(content->height(), qreal(window->height()));
}

QTEST_MAIN(tst_applicationwindow)

#include "tst_applicationwindow.moc"
