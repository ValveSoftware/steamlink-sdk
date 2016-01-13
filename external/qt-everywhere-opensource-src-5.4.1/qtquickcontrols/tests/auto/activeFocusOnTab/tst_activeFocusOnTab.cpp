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
#include <QtTest/QSignalSpy>
#include <QtQml/qqmlengine.h>
#include <QtQml/qqmlcomponent.h>
#include <QtQml/qqmlcontext.h>
#include <QtQuick/qquickview.h>
#include <QtQuick/private/qquickitem_p.h>
#include <QtGui/private/qguiapplication_p.h>
#include <QtGui/qpa/qplatformtheme.h>
#include "../shared/util.h"
#include "../shared/visualtestutil.h"

using namespace QQuickVisualTestUtil;

class tst_activeFocusOnTab : public QQmlDataTest
{
    Q_OBJECT
public:
    tst_activeFocusOnTab();

private slots:
    void initTestCase();
    void cleanup();

    void activeFocusOnTab();
    void activeFocusOnTab2();
private:
    QQmlEngine engine;
    bool qt_tab_all_widgets() {
        if (const QPlatformTheme *theme = QGuiApplicationPrivate::platformTheme())
            return theme->themeHint(QPlatformTheme::TabAllWidgets).toBool();
        return true;
    }
};

tst_activeFocusOnTab::tst_activeFocusOnTab()
{
}

void tst_activeFocusOnTab::initTestCase()
{
    QQmlDataTest::initTestCase();
}

void tst_activeFocusOnTab::cleanup()
{
}

void tst_activeFocusOnTab::activeFocusOnTab()
{
    if (!qt_tab_all_widgets())
        QSKIP("This function doesn't support NOT iterating all.");

    QQuickView *window = new QQuickView(0);
    window->setBaseSize(QSize(800,600));

    window->setSource(testFileUrl("activeFocusOnTab.qml"));
    window->show();
    window->requestActivate();
    QVERIFY(QTest::qWaitForWindowActive(window));
    QVERIFY(QGuiApplication::focusWindow() == window);

    // original: button1
    QQuickItem *item = findItem<QQuickItem>(window->rootObject(), "button1");
    QVERIFY(item);
    QVERIFY(item->hasActiveFocus());

    // Tab: button1->button2
    QKeyEvent key(QEvent::KeyPress, Qt::Key_Tab, Qt::NoModifier, "", false, 1);
    QGuiApplication::sendEvent(window, &key);
    QVERIFY(key.isAccepted());

    item = findItem<QQuickItem>(window->rootObject(), "button2");
    QVERIFY(item);
    QVERIFY(item->hasActiveFocus());

    // Tab: button2->toolbutton
    key = QKeyEvent(QEvent::KeyPress, Qt::Key_Tab, Qt::NoModifier, "", false, 1);
    QGuiApplication::sendEvent(window, &key);
    QVERIFY(key.isAccepted());

    item = findItem<QQuickItem>(window->rootObject(), "toolbutton");
    QVERIFY(item);
    QVERIFY(item->hasActiveFocus());

    // Tab: toolbutton->combobox
    key = QKeyEvent(QEvent::KeyPress, Qt::Key_Tab, Qt::NoModifier, "", false, 1);
    QGuiApplication::sendEvent(window, &key);
    QVERIFY(key.isAccepted());

    item = findItem<QQuickItem>(window->rootObject(), "combobox");
    QVERIFY(item);
    QVERIFY(item->hasActiveFocus());

    // Tab: combobox->editable_combobox
    key = QKeyEvent(QEvent::KeyPress, Qt::Key_Tab, Qt::NoModifier, "", false, 1);
    QGuiApplication::sendEvent(window, &key);
    QVERIFY(key.isAccepted());

    item = findItem<QQuickItem>(window->rootObject(), "editable_combobox");
    QVERIFY(item);
    QVERIFY(item->hasActiveFocus());

    // Tab: editable_combobox->group1_checkbox
    key = QKeyEvent(QEvent::KeyPress, Qt::Key_Tab, Qt::NoModifier, "", false, 1);
    QGuiApplication::sendEvent(window, &key);
    QVERIFY(key.isAccepted());

    item = findItem<QQuickItem>(window->rootObject(), "group1");
    QVERIFY(item);
    QQuickItem *subitem = findItem<QQuickItem>(item, "group1_checkbox");
    QVERIFY(subitem);
    QVERIFY(subitem->hasActiveFocus());

    // Tab: group1->checkbox1
    key = QKeyEvent(QEvent::KeyPress, Qt::Key_Tab, Qt::NoModifier, "", false, 1);
    QGuiApplication::sendEvent(window, &key);
    QVERIFY(key.isAccepted());

    item = findItem<QQuickItem>(window->rootObject(), "checkbox1");
    QVERIFY(item);
    QVERIFY(item->hasActiveFocus());

    // Tab: checkbox1->checkbox2
    key = QKeyEvent(QEvent::KeyPress, Qt::Key_Tab, Qt::NoModifier, "", false, 1);
    QGuiApplication::sendEvent(window, &key);
    QVERIFY(key.isAccepted());

    item = findItem<QQuickItem>(window->rootObject(), "checkbox2");
    QVERIFY(item);
    QVERIFY(item->hasActiveFocus());

    // Tab: checkbox2->radiobutton1
    key = QKeyEvent(QEvent::KeyPress, Qt::Key_Tab, Qt::NoModifier, "", false, 1);
    QGuiApplication::sendEvent(window, &key);
    QVERIFY(key.isAccepted());

    item = findItem<QQuickItem>(window->rootObject(), "radiobutton1");
    QVERIFY(item);
    QVERIFY(item->hasActiveFocus());

    // Tab: radiobutton1->radiobutton2
    key = QKeyEvent(QEvent::KeyPress, Qt::Key_Tab, Qt::NoModifier, "", false, 1);
    QGuiApplication::sendEvent(window, &key);
    QVERIFY(key.isAccepted());

    item = findItem<QQuickItem>(window->rootObject(), "radiobutton2");
    QVERIFY(item);
    QVERIFY(item->hasActiveFocus());

    // Tab: radiobutton2->slider
    key = QKeyEvent(QEvent::KeyPress, Qt::Key_Tab, Qt::NoModifier, "", false, 1);
    QGuiApplication::sendEvent(window, &key);
    QVERIFY(key.isAccepted());

    item = findItem<QQuickItem>(window->rootObject(), "slider");
    QVERIFY(item);
    QVERIFY(item->hasActiveFocus());

    // Tab: slider->spinbox
    key = QKeyEvent(QEvent::KeyPress, Qt::Key_Tab, Qt::NoModifier, "", false, 1);
    QGuiApplication::sendEvent(window, &key);
    QVERIFY(key.isAccepted());

    item = findItem<QQuickItem>(window->rootObject(), "spinbox");
    QVERIFY(item);
    QVERIFY(item->hasActiveFocus());

    // Tab: spinbox->tab1_btn1
    key = QKeyEvent(QEvent::KeyPress, Qt::Key_Tab, Qt::NoModifier, "", false, 1);
    for (int i = 0; i < 2; ++i) {
        QGuiApplication::sendEvent(window, &key);
        QVERIFY(key.isAccepted());
    }

    item = findItem<QQuickItem>(window->rootObject(), "tab1_btn1");
    QVERIFY(item);
    QVERIFY(item->hasActiveFocus());

    // Tab: tab1_btn1->tab1_btn2
    key = QKeyEvent(QEvent::KeyPress, Qt::Key_Tab, Qt::NoModifier, "", false, 1);
    QGuiApplication::sendEvent(window, &key);
    QVERIFY(key.isAccepted());

    item = findItem<QQuickItem>(window->rootObject(), "tab1_btn2");
    QVERIFY(item);
    QVERIFY(item->hasActiveFocus());

    // Tab: tab1_btn2->textfield
    key = QKeyEvent(QEvent::KeyPress, Qt::Key_Tab, Qt::NoModifier, "", false, 1);
    QGuiApplication::sendEvent(window, &key);
    QVERIFY(key.isAccepted());

    item = findItem<QQuickItem>(window->rootObject(), "textfield");
    QVERIFY(item);
    QVERIFY(item->hasActiveFocus());

    // Tab: textfield->tableview
    key = QKeyEvent(QEvent::KeyPress, Qt::Key_Tab, Qt::NoModifier, "", false, 1);
    QGuiApplication::sendEvent(window, &key);
    QVERIFY(key.isAccepted());

    item = findItem<QQuickItem>(window->rootObject(), "tableview");
    QVERIFY(item);
    QVERIFY(item->hasActiveFocus());

    // Tab: tableview->textarea
    key = QKeyEvent(QEvent::KeyPress, Qt::Key_Tab, Qt::NoModifier, "", false, 1);
    QGuiApplication::sendEvent(window, &key);
    QVERIFY(key.isAccepted());

    item = findItem<QQuickItem>(window->rootObject(), "textarea");
    QVERIFY(item);
    QVERIFY(item->hasActiveFocus());

    // BackTab: textarea->tableview
    key = QKeyEvent(QEvent::KeyPress, Qt::Key_Tab, Qt::ShiftModifier, "", false, 1);
    QGuiApplication::sendEvent(window, &key);
    QVERIFY(key.isAccepted());

    item = findItem<QQuickItem>(window->rootObject(), "tableview");
    QVERIFY(item);
    QVERIFY(item->hasActiveFocus());

    // BackTab: tableview->textfield
    key = QKeyEvent(QEvent::KeyPress, Qt::Key_Tab, Qt::ShiftModifier, "", false, 1);
    QGuiApplication::sendEvent(window, &key);
    QVERIFY(key.isAccepted());

    item = findItem<QQuickItem>(window->rootObject(), "textfield");
    QVERIFY(item);
    QVERIFY(item->hasActiveFocus());

    // BackTab: textfield->tab1_btn2
    key = QKeyEvent(QEvent::KeyPress, Qt::Key_Tab, Qt::ShiftModifier, "", false, 1);
    QGuiApplication::sendEvent(window, &key);
    QVERIFY(key.isAccepted());

    item = findItem<QQuickItem>(window->rootObject(), "tab1_btn2");
    QVERIFY(item);
    QVERIFY(item->hasActiveFocus());

    // BackTab: tab1_btn2->tab1_btn1
    key = QKeyEvent(QEvent::KeyPress, Qt::Key_Tab, Qt::ShiftModifier, "", false, 1);
    QGuiApplication::sendEvent(window, &key);
    QVERIFY(key.isAccepted());

    item = findItem<QQuickItem>(window->rootObject(), "tab1_btn1");
    QVERIFY(item);
    QVERIFY(item->hasActiveFocus());

    // BackTab: tab1_btn1->tab1
    key = QKeyEvent(QEvent::KeyPress, Qt::Key_Tab, Qt::ShiftModifier, "", false, 1);
    QGuiApplication::sendEvent(window, &key);
    QVERIFY(key.isAccepted());
/*
    // Right: tab1->tab2
    key = QKeyEvent(QEvent::KeyPress, Qt::Key_Right, Qt::NoModifier, "", false, 1);
    QGuiApplication::sendEvent(window, &key);
    QVERIFY(key.isAccepted());

    // Tab: tab2->tab2_btn1
    key = QKeyEvent(QEvent::KeyPress, Qt::Key_Tab, Qt::NoModifier, "", false, 1);
    QGuiApplication::sendEvent(window, &key);
    QVERIFY(key.isAccepted());

    item = findItem<QQuickItem>(window->rootObject(), "tab2_btn1");
    QVERIFY(item);
    QVERIFY(item->hasActiveFocus());

    // Tab: tab2_btn1->tab2_btn2
    key = QKeyEvent(QEvent::KeyPress, Qt::Key_Tab, Qt::NoModifier, "", false, 1);
    QGuiApplication::sendEvent(window, &key);
    QVERIFY(key.isAccepted());

    item = findItem<QQuickItem>(window->rootObject(), "tab2_btn2");
    QVERIFY(item);
    QVERIFY(item->hasActiveFocus());

    // Tab: tab2_btn2->textfield
    key = QKeyEvent(QEvent::KeyPress, Qt::Key_Tab, Qt::NoModifier, "", false, 1);
    QGuiApplication::sendEvent(window, &key);
    QVERIFY(key.isAccepted());

    item = findItem<QQuickItem>(window->rootObject(), "textfield");
    QVERIFY(item);
    QVERIFY(item->hasActiveFocus());

    // BackTab: textfield->tab2_btn2
    key = QKeyEvent(QEvent::KeyPress, Qt::Key_Tab, Qt::ShiftModifier, "", false, 1);
    QGuiApplication::sendEvent(window, &key);
    QVERIFY(key.isAccepted());

    item = findItem<QQuickItem>(window->rootObject(), "tab2_btn2");
    QVERIFY(item);
    QVERIFY(item->hasActiveFocus());

    // BackTab: tab2_btn2->tab2_btn1
    key = QKeyEvent(QEvent::KeyPress, Qt::Key_Tab, Qt::ShiftModifier, "", false, 1);
    QGuiApplication::sendEvent(window, &key);
    QVERIFY(key.isAccepted());

    item = findItem<QQuickItem>(window->rootObject(), "tab2_btn1");
    QVERIFY(item);
    QVERIFY(item->hasActiveFocus());

    // BackTab: tab2_btn1->tab2
    key = QKeyEvent(QEvent::KeyPress, Qt::Key_Tab, Qt::ShiftModifier, "", false, 1);
    QGuiApplication::sendEvent(window, &key);
    QVERIFY(key.isAccepted());
*/
    // BackTab: tab2->spinbox
    key = QKeyEvent(QEvent::KeyPress, Qt::Key_Tab, Qt::ShiftModifier, "", false, 1);
    QGuiApplication::sendEvent(window, &key);
    QVERIFY(key.isAccepted());

    item = findItem<QQuickItem>(window->rootObject(), "spinbox");
    QVERIFY(item);
    QVERIFY(item->hasActiveFocus());

    // BackTab: spinbox->slider
    key = QKeyEvent(QEvent::KeyPress, Qt::Key_Tab, Qt::ShiftModifier, "", false, 1);
    QGuiApplication::sendEvent(window, &key);
    QVERIFY(key.isAccepted());

    item = findItem<QQuickItem>(window->rootObject(), "slider");
    QVERIFY(item);
    QVERIFY(item->hasActiveFocus());

    // BackTab: slider->radiobutton2
    key = QKeyEvent(QEvent::KeyPress, Qt::Key_Tab, Qt::ShiftModifier, "", false, 1);
    QGuiApplication::sendEvent(window, &key);
    QVERIFY(key.isAccepted());

    item = findItem<QQuickItem>(window->rootObject(), "radiobutton2");
    QVERIFY(item);
    QVERIFY(item->hasActiveFocus());

    // BackTab: radiobutton2->radiobutton1
    key = QKeyEvent(QEvent::KeyPress, Qt::Key_Tab, Qt::ShiftModifier, "", false, 1);
    QGuiApplication::sendEvent(window, &key);
    QVERIFY(key.isAccepted());

    item = findItem<QQuickItem>(window->rootObject(), "radiobutton1");
    QVERIFY(item);
    QVERIFY(item->hasActiveFocus());

    // BackTab: radiobutton1->checkbox2
    key = QKeyEvent(QEvent::KeyPress, Qt::Key_Tab, Qt::ShiftModifier, "", false, 1);
    QGuiApplication::sendEvent(window, &key);
    QVERIFY(key.isAccepted());

    item = findItem<QQuickItem>(window->rootObject(), "checkbox2");
    QVERIFY(item);
    QVERIFY(item->hasActiveFocus());

    // BackTab: checkbox2->checkbox1
    key = QKeyEvent(QEvent::KeyPress, Qt::Key_Tab, Qt::ShiftModifier, "", false, 1);
    QGuiApplication::sendEvent(window, &key);
    QVERIFY(key.isAccepted());

    item = findItem<QQuickItem>(window->rootObject(), "checkbox1");
    QVERIFY(item);
    QVERIFY(item->hasActiveFocus());

    // BackTab: checkbox1->group1_checkbox
    key = QKeyEvent(QEvent::KeyPress, Qt::Key_Tab, Qt::ShiftModifier, "", false, 1);
    QGuiApplication::sendEvent(window, &key);
    QVERIFY(key.isAccepted());

    item = findItem<QQuickItem>(window->rootObject(), "group1_checkbox");
    QVERIFY(item);
    QVERIFY(item->hasActiveFocus());

    // BackTab: group1_checkbox->editable_combobox
    key = QKeyEvent(QEvent::KeyPress, Qt::Key_Tab, Qt::ShiftModifier, "", false, 1);
    QGuiApplication::sendEvent(window, &key);
    QVERIFY(key.isAccepted());

    item = findItem<QQuickItem>(window->rootObject(), "editable_combobox");
    QVERIFY(item);
    QVERIFY(item->hasActiveFocus());

    // BackTab: editable_combobox->combobox
    key = QKeyEvent(QEvent::KeyPress, Qt::Key_Tab, Qt::ShiftModifier, "", false, 1);
    QGuiApplication::sendEvent(window, &key);
    QVERIFY(key.isAccepted());

    item = findItem<QQuickItem>(window->rootObject(), "combobox");
    QVERIFY(item);
    QVERIFY(item->hasActiveFocus());

    // BackTab: combobox->toolbutton
    key = QKeyEvent(QEvent::KeyPress, Qt::Key_Tab, Qt::ShiftModifier, "", false, 1);
    QGuiApplication::sendEvent(window, &key);
    QVERIFY(key.isAccepted());

    item = findItem<QQuickItem>(window->rootObject(), "toolbutton");
    QVERIFY(item);
    QVERIFY(item->hasActiveFocus());

    // BackTab: toolbutton->button2
    key = QKeyEvent(QEvent::KeyPress, Qt::Key_Tab, Qt::ShiftModifier, "", false, 1);
    QGuiApplication::sendEvent(window, &key);
    QVERIFY(key.isAccepted());

    item = findItem<QQuickItem>(window->rootObject(), "button2");
    QVERIFY(item);
    QVERIFY(item->hasActiveFocus());

    // BackTab: button2->button1
    key = QKeyEvent(QEvent::KeyPress, Qt::Key_Tab, Qt::ShiftModifier, "", false, 1);
    QGuiApplication::sendEvent(window, &key);
    QVERIFY(key.isAccepted());

    item = findItem<QQuickItem>(window->rootObject(), "button1");
    QVERIFY(item);
    QVERIFY(item->hasActiveFocus());

    // BackTab: button1->textarea
    key = QKeyEvent(QEvent::KeyPress, Qt::Key_Tab, Qt::ShiftModifier, "", false, 1);
    QGuiApplication::sendEvent(window, &key);
    QVERIFY(key.isAccepted());

    item = findItem<QQuickItem>(window->rootObject(), "textarea");
    QVERIFY(item);
    QVERIFY(item->hasActiveFocus());

    delete window;
}

void tst_activeFocusOnTab::activeFocusOnTab2()
{
    if (qt_tab_all_widgets())
        QSKIP("This function doesn't support iterating all.");

    QQuickView *window = new QQuickView(0);
    window->setBaseSize(QSize(800,600));

    window->setSource(testFileUrl("activeFocusOnTab.qml"));
    window->show();
    window->requestActivate();
    QVERIFY(QTest::qWaitForWindowActive(window));
    QVERIFY(QGuiApplication::focusWindow() == window);

    // original: editable_combobox
    QQuickItem *item = findItem<QQuickItem>(window->rootObject(), "editable_combobox");
    QVERIFY(item);
    item->forceActiveFocus();
    QVERIFY(item->hasActiveFocus());

    // Tab: editable_combobox->spinbox
    QKeyEvent key(QEvent::KeyPress, Qt::Key_Tab, Qt::NoModifier, "", false, 1);
    QGuiApplication::sendEvent(window, &key);
    QVERIFY(key.isAccepted());

    item = findItem<QQuickItem>(window->rootObject(), "spinbox");
    QVERIFY(item);
    QVERIFY(item->hasActiveFocus());

    // Tab: spinbox->textfield
    key = QKeyEvent(QEvent::KeyPress, Qt::Key_Tab, Qt::NoModifier, "", false, 1);
    QGuiApplication::sendEvent(window, &key);
    QVERIFY(key.isAccepted());

    item = findItem<QQuickItem>(window->rootObject(), "textfield");
    QVERIFY(item);
    QVERIFY(item->hasActiveFocus());

    // Tab: textfield->tableview
    key = QKeyEvent(QEvent::KeyPress, Qt::Key_Tab, Qt::NoModifier, "", false, 1);
    QGuiApplication::sendEvent(window, &key);
    QVERIFY(key.isAccepted());

    item = findItem<QQuickItem>(window->rootObject(), "tableview");
    QVERIFY(item);
    QVERIFY(item->hasActiveFocus());

    // Tab: tableview->textarea
    key = QKeyEvent(QEvent::KeyPress, Qt::Key_Tab, Qt::NoModifier, "", false, 1);
    QGuiApplication::sendEvent(window, &key);
    QVERIFY(key.isAccepted());

    item = findItem<QQuickItem>(window->rootObject(), "textarea");
    QVERIFY(item);
    QVERIFY(item->hasActiveFocus());

    // BackTab: textarea->tableview
    key = QKeyEvent(QEvent::KeyPress, Qt::Key_Tab, Qt::ShiftModifier, "", false, 1);
    QGuiApplication::sendEvent(window, &key);
    QVERIFY(key.isAccepted());

    item = findItem<QQuickItem>(window->rootObject(), "tableview");
    QVERIFY(item);
    QVERIFY(item->hasActiveFocus());

    // BackTab: tableview->textfield
    key = QKeyEvent(QEvent::KeyPress, Qt::Key_Tab, Qt::ShiftModifier, "", false, 1);
    QGuiApplication::sendEvent(window, &key);
    QVERIFY(key.isAccepted());

    item = findItem<QQuickItem>(window->rootObject(), "textfield");
    QVERIFY(item);
    QVERIFY(item->hasActiveFocus());

    // BackTab: textfield->spinbox
    key = QKeyEvent(QEvent::KeyPress, Qt::Key_Tab, Qt::ShiftModifier, "", false, 1);
    QGuiApplication::sendEvent(window, &key);
    QVERIFY(key.isAccepted());

    item = findItem<QQuickItem>(window->rootObject(), "spinbox");
    QVERIFY(item);
    QVERIFY(item->hasActiveFocus());

    // BackTab: spinbox->editable_combobox
    key = QKeyEvent(QEvent::KeyPress, Qt::Key_Tab, Qt::ShiftModifier, "", false, 1);
    QGuiApplication::sendEvent(window, &key);
    QVERIFY(key.isAccepted());

    item = findItem<QQuickItem>(window->rootObject(), "editable_combobox");
    QVERIFY(item);
    QVERIFY(item->hasActiveFocus());

    // BackTab: editable_combobox->textarea
    key = QKeyEvent(QEvent::KeyPress, Qt::Key_Tab, Qt::ShiftModifier, "", false, 1);
    QGuiApplication::sendEvent(window, &key);
    QVERIFY(key.isAccepted());

    item = findItem<QQuickItem>(window->rootObject(), "textarea");
    QVERIFY(item);
    QVERIFY(item->hasActiveFocus());

    // BackTab: textarea->tableview
    key = QKeyEvent(QEvent::KeyPress, Qt::Key_Tab, Qt::ShiftModifier, "", false, 1);
    QGuiApplication::sendEvent(window, &key);
    QVERIFY(key.isAccepted());

    item = findItem<QQuickItem>(window->rootObject(), "tableview");
    QVERIFY(item);
    QVERIFY(item->hasActiveFocus());

    delete window;
}

QTEST_MAIN(tst_activeFocusOnTab)

#include "tst_activeFocusOnTab.moc"
