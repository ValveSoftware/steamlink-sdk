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
#include <QtQuickTemplates2/private/qquickcontrol_p.h>
#include <QtGui/private/qguiapplication_p.h>
#include <QtGui/qstylehints.h>
#include "../shared/util.h"
#include "../shared/visualtestutil.h"

using namespace QQuickVisualTestUtil;

class tst_focus : public QQmlDataTest
{
    Q_OBJECT

private slots:
    void initTestCase();

    void navigation_data();
    void navigation();

    void policy();

    void reason_data();
    void reason();

    void visualFocus();
};

void tst_focus::initTestCase()
{
    QQmlDataTest::initTestCase();
}

void tst_focus::navigation_data()
{
    QTest::addColumn<Qt::Key>("key");
    QTest::addColumn<QString>("testFile");
    QTest::addColumn<Qt::TabFocusBehavior>("behavior");
    QTest::addColumn<QStringList>("order");

    QTest::newRow("tab-all-controls") << Qt::Key_Tab << QString("activeFocusOnTab.qml") << Qt::TabFocusAllControls << (QStringList() << "button2" << "checkbox" << "checkbox1" << "checkbox2" << "radiobutton" << "radiobutton1" << "radiobutton2" << "rangeslider.first" << "rangeslider.second" << "slider" << "spinbox" << "switch" << "tabbutton1" << "tabbutton2" << "textfield" << "toolbutton" << "textarea" << "button1");
    QTest::newRow("backtab-all-controls") << Qt::Key_Backtab << QString("activeFocusOnTab.qml") << Qt::TabFocusAllControls << (QStringList() << "textarea" << "toolbutton" << "textfield" << "tabbutton2" << "tabbutton1" << "switch" << "spinbox" << "slider" << "rangeslider.second" << "rangeslider.first" << "radiobutton2" << "radiobutton1" << "radiobutton" << "checkbox2" << "checkbox1" << "checkbox" << "button2" << "button1");

    QTest::newRow("tab-text-controls") << Qt::Key_Tab << QString("activeFocusOnTab.qml") << Qt::TabFocusTextControls << (QStringList() << "spinbox" << "textfield" << "textarea");
    QTest::newRow("backtab-text-controls") << Qt::Key_Backtab << QString("activeFocusOnTab.qml") << Qt::TabFocusTextControls << (QStringList() << "textarea" << "textfield" << "spinbox");

    QTest::newRow("key-up") << Qt::Key_Up << QString("keyNavigation.qml") << Qt::TabFocusAllControls << (QStringList() << "textarea" << "toolbutton" << "textfield" << "tabbutton2" << "tabbutton1" << "switch" << "slider" << "rangeslider.first" << "radiobutton2" << "radiobutton1" << "radiobutton" << "checkbox2" << "checkbox1" << "checkbox" << "button2" << "button1");
    QTest::newRow("key-down") << Qt::Key_Down << QString("keyNavigation.qml") << Qt::TabFocusAllControls << (QStringList() << "button2" << "checkbox" << "checkbox1" << "checkbox2" << "radiobutton" << "radiobutton1" << "radiobutton2" << "rangeslider.first" << "slider" << "switch" << "tabbutton1" << "tabbutton2" << "textfield" << "toolbutton" << "textarea" << "button1");
    QTest::newRow("key-left") << Qt::Key_Left << QString("keyNavigation.qml") << Qt::TabFocusAllControls << (QStringList() << "toolbutton" << "tabbutton2" << "tabbutton1" << "switch" << "spinbox" << "radiobutton2" << "radiobutton1" << "radiobutton" << "checkbox2" << "checkbox1" << "checkbox" << "button2" << "button1");
    QTest::newRow("key-right") << Qt::Key_Right << QString("keyNavigation.qml") << Qt::TabFocusAllControls << (QStringList() << "button2" << "checkbox" << "checkbox1" << "checkbox2" << "radiobutton" << "radiobutton1" << "radiobutton2" << "spinbox" << "switch" << "tabbutton1" << "tabbutton2" << "toolbutton" << "button1");
}

void tst_focus::navigation()
{
    QFETCH(Qt::Key, key);
    QFETCH(QString, testFile);
    QFETCH(Qt::TabFocusBehavior, behavior);
    QFETCH(QStringList, order);

    QGuiApplication::styleHints()->setTabFocusBehavior(behavior);

    QQuickView view;
    view.contentItem()->setObjectName("contentItem");

    view.setSource(testFileUrl(testFile));
    view.show();
    view.requestActivate();
    QVERIFY(QTest::qWaitForWindowActive(&view));
    QVERIFY(QGuiApplication::focusWindow() == &view);

    for (const QString &name : qAsConst(order)) {
        QKeyEvent event(QEvent::KeyPress, key, Qt::NoModifier);
        QGuiApplication::sendEvent(&view, &event);
        QVERIFY(event.isAccepted());

        QQuickItem *item = findItem<QQuickItem>(view.rootObject(), name);
        QVERIFY2(item, qPrintable(name));
        QVERIFY2(item->hasActiveFocus(), qPrintable(QString("expected: '%1', actual: '%2'").arg(name).arg(view.activeFocusItem() ? view.activeFocusItem()->objectName() : "null")));
    }

    QGuiApplication::styleHints()->setTabFocusBehavior(Qt::TabFocusBehavior(-1));
}

void tst_focus::policy()
{
    QQmlEngine engine;
    QQmlComponent component(&engine);
    component.setData("import QtQuick.Controls 2.1; ApplicationWindow { width: 100; height: 100; Control { anchors.fill: parent } }", QUrl());

    QScopedPointer<QQuickApplicationWindow> window(qobject_cast<QQuickApplicationWindow *>(component.create()));
    QVERIFY(window);

    QQuickControl *control = qobject_cast<QQuickControl *>(window->contentItem()->childItems().first());
    QVERIFY(control);

    QVERIFY(!control->hasActiveFocus());
    QVERIFY(!control->hasVisualFocus());

    window->show();
    window->requestActivate();
    QVERIFY(QTest::qWaitForWindowActive(window.data()));

    // Qt::TabFocus vs. QQuickItem::activeFocusOnTab
    control->setActiveFocusOnTab(true);
    QCOMPARE(control->focusPolicy(), Qt::TabFocus);
    control->setActiveFocusOnTab(false);
    QCOMPARE(control->focusPolicy(), Qt::NoFocus);

    control->setFocusPolicy(Qt::TabFocus);
    QCOMPARE(control->focusPolicy(), Qt::TabFocus);
    QCOMPARE(control->activeFocusOnTab(), true);

    // Qt::TabFocus
    QGuiApplication::styleHints()->setTabFocusBehavior(Qt::TabFocusAllControls);
    QTest::keyClick(window.data(), Qt::Key_Tab);
    QVERIFY(control->hasActiveFocus());
    QVERIFY(control->hasVisualFocus());
    QGuiApplication::styleHints()->setTabFocusBehavior(Qt::TabFocusBehavior(-1));

    // reset
    control->setFocus(false);
    QVERIFY(!control->hasActiveFocus());

    // Qt::ClickFocus
    control->setAcceptedMouseButtons(Qt::LeftButton);
    QTest::mouseClick(window.data(), Qt::LeftButton, Qt::NoModifier, QPoint(control->width() / 2, control->height() / 2));
    QVERIFY(!control->hasActiveFocus());
    QVERIFY(!control->hasVisualFocus());

    control->setFocusPolicy(Qt::ClickFocus);
    QCOMPARE(control->focusPolicy(), Qt::ClickFocus);
    QTest::mouseClick(window.data(), Qt::LeftButton, Qt::NoModifier, QPoint(control->width() / 2, control->height() / 2));
    QVERIFY(control->hasActiveFocus());
    QVERIFY(!control->hasVisualFocus());

    // reset
    control->setFocus(false);
    QVERIFY(!control->hasActiveFocus());

    // Qt::WheelFocus
    QWheelEvent wheelEvent(QPoint(control->width() / 2, control->height() / 2), 10, Qt::NoButton, Qt::NoModifier);
    QGuiApplication::sendEvent(control, &wheelEvent);
    QVERIFY(!control->hasActiveFocus());
    QVERIFY(!control->hasVisualFocus());

    control->setFocusPolicy(Qt::WheelFocus);
    QCOMPARE(control->focusPolicy(), Qt::WheelFocus);

    QGuiApplication::sendEvent(control, &wheelEvent);
    QVERIFY(control->hasActiveFocus());
    QVERIFY(!control->hasVisualFocus());
}

void tst_focus::reason_data()
{
    QTest::addColumn<QString>("name");

    QTest::newRow("Control") << "Control";
    QTest::newRow("TextField") << "TextField";
    QTest::newRow("TextArea") << "TextArea";
    QTest::newRow("SpinBox") << "SpinBox";
    QTest::newRow("ComboBox") << "ComboBox";
}

void tst_focus::reason()
{
    QFETCH(QString, name);

    QQmlEngine engine;
    QQmlComponent component(&engine);
    component.setData(QString("import QtQuick.Controls 2.1; ApplicationWindow { width: 100; height: 100; %1 { anchors.fill: parent } }").arg(name).toUtf8(), QUrl());

    QScopedPointer<QQuickApplicationWindow> window(qobject_cast<QQuickApplicationWindow *>(component.create()));
    QVERIFY(window.data());

    QQuickItem *control = window->contentItem()->childItems().first();
    QVERIFY(control);

    window->show();
    window->requestActivate();
    QVERIFY(QTest::qWaitForWindowActive(window.data()));

    QCOMPARE(control->property("focusReason").toInt(), int(Qt::OtherFocusReason));
    control->forceActiveFocus(Qt::MouseFocusReason);
    QVERIFY(control->hasActiveFocus());
    QCOMPARE(control->property("focusReason").toInt(), int(Qt::MouseFocusReason));

    QEXPECT_FAIL("TextArea", "TODO: TextArea::visualFocus?", Continue);
    QEXPECT_FAIL("TextField", "TODO: TextField::visualFocus?", Continue);
    QCOMPARE(control->property("visualFocus"), QVariant(false));

    window->contentItem()->setFocus(false, Qt::TabFocusReason);
    QVERIFY(!control->hasActiveFocus());
    QCOMPARE(control->property("focusReason").toInt(), int(Qt::TabFocusReason));

    QEXPECT_FAIL("TextArea", "", Continue);
    QEXPECT_FAIL("TextField", "", Continue);
    QCOMPARE(control->property("visualFocus"), QVariant(false));

    control->forceActiveFocus(Qt::TabFocusReason);
    QVERIFY(control->hasActiveFocus());
    QCOMPARE(control->property("focusReason").toInt(), int(Qt::TabFocusReason));

    QEXPECT_FAIL("TextArea", "", Continue);
    QEXPECT_FAIL("TextField", "", Continue);
    QCOMPARE(control->property("visualFocus"), QVariant(true));
}

void tst_focus::visualFocus()
{
    QQuickView view;
    view.setSource(testFileUrl("visualFocus.qml"));
    view.show();
    view.requestActivate();
    QVERIFY(QTest::qWaitForWindowActive(&view));

    QQuickItem *column = view.rootObject();
    QVERIFY(column);
    QCOMPARE(column->childItems().count(), 2);

    QQuickControl *button = qobject_cast<QQuickControl *>(column->childItems().first());
    QVERIFY(button);

    QQuickItem *textfield = column->childItems().last();
    QVERIFY(textfield);

    button->forceActiveFocus(Qt::TabFocusReason);
    QVERIFY(button->hasActiveFocus());
    QVERIFY(button->hasVisualFocus());
    QVERIFY(button->property("showFocus").toBool());

    QTest::mouseClick(&view, Qt::LeftButton, Qt::NoModifier, QPoint(textfield->x() + textfield->width() / 2, textfield->y() + textfield->height() / 2));
    QVERIFY(!button->hasActiveFocus());
    QVERIFY(!button->hasVisualFocus());
    QVERIFY(!button->property("showFocus").toBool());
}

QTEST_MAIN(tst_focus)

#include "tst_focus.moc"
