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
#include <QtQml/qqmlcomponent.h>
#include <QtQml/qqmlengine.h>
#include <QtQuick/qquickitem.h>
#include <QtQuick/qquickview.h>
#include <QtGui/qinputmethod.h>
#include <qpa/qwindowsysteminterface.h>
#include <qpa/qplatformintegration.h>
#include <private/qguiapplication_p.h>

class tst_qquickapplication : public QObject
{
    Q_OBJECT
public:
    tst_qquickapplication();

private slots:
    void active();
    void state();
    void layoutDirection();
    void inputMethod();
    void cleanup();

private:
    QQmlEngine engine;
};

tst_qquickapplication::tst_qquickapplication()
{
}

void tst_qquickapplication::cleanup()
{
    if (QGuiApplicationPrivate::platformIntegration()->hasCapability(QPlatformIntegration::ApplicationState)) {
        QWindowSystemInterface::handleApplicationStateChanged(Qt::ApplicationInactive);
        QTest::waitForEvents();
    }
}

void tst_qquickapplication::active()
{
    QQmlComponent component(&engine);
    component.setData("import QtQuick 2.0; "
                      "Item { "
                      "    property bool active: Qt.application.active; "
                      "    property bool active2: false; "
                      "    Connections { "
                      "        target: Qt.application; "
                      "        onActiveChanged: active2 = Qt.application.active; "
                      "    } "
                      "}", QUrl::fromLocalFile(""));
    QQuickItem *item = qobject_cast<QQuickItem *>(component.create());
    QVERIFY(item);
    QQuickWindow window;
    item->setParentItem(window.contentItem());

    // not active
    QVERIFY(!item->property("active").toBool());
    QVERIFY(!item->property("active2").toBool());

    // active
    window.show();
    window.requestActivate();
    QTest::qWaitForWindowActive(&window);
    QVERIFY(QGuiApplication::focusWindow() == &window);
    QVERIFY(item->property("active").toBool());
    QVERIFY(item->property("active2").toBool());

    QWindowSystemInterface::handleWindowActivated(0);

#ifdef Q_OS_OSX
    // OS X has the concept of "reactivation"
    QTRY_VERIFY(QGuiApplication::focusWindow() != &window);
    QVERIFY(item->property("active").toBool());
    QVERIFY(item->property("active2").toBool());
#else
    // not active again
    QTRY_VERIFY(QGuiApplication::focusWindow() != &window);
    QVERIFY(!item->property("active").toBool());
    QVERIFY(!item->property("active2").toBool());
#endif
}

void tst_qquickapplication::state()
{
    QQmlComponent component(&engine);
    component.setData("import QtQuick 2.0; "
                      "Item { "
                      "    property int state: Qt.application.state; "
                      "    property int state2: Qt.ApplicationInactive; "
                      "    Connections { "
                      "        target: Qt.application; "
                      "        onStateChanged: state2 = Qt.application.state; "
                      "    } "
                      "    Component.onCompleted: state2 = Qt.application.state; "
                      "}", QUrl::fromLocalFile(""));
    QQuickItem *item = qobject_cast<QQuickItem *>(component.create());
    QVERIFY(item);
    QQuickWindow window;
    item->setParentItem(window.contentItem());

    // initial state should be ApplicationInactive
    QCOMPARE(Qt::ApplicationState(item->property("state").toInt()), Qt::ApplicationInactive);
    QCOMPARE(Qt::ApplicationState(item->property("state2").toInt()), Qt::ApplicationInactive);

    // If the platform plugin has the ApplicationState capability, state changes originate from it
    // as a result of a system event. We therefore have to simulate these events here.
    if (QGuiApplicationPrivate::platformIntegration()->hasCapability(QPlatformIntegration::ApplicationState)) {

        QWindowSystemInterface::handleApplicationStateChanged(Qt::ApplicationActive);
        QTest::waitForEvents();
        QCOMPARE(Qt::ApplicationState(item->property("state").toInt()), Qt::ApplicationActive);
        QCOMPARE(Qt::ApplicationState(item->property("state2").toInt()), Qt::ApplicationActive);

        QWindowSystemInterface::handleApplicationStateChanged(Qt::ApplicationInactive);
        QTest::waitForEvents();
        QCOMPARE(Qt::ApplicationState(item->property("state").toInt()), Qt::ApplicationInactive);
        QCOMPARE(Qt::ApplicationState(item->property("state2").toInt()), Qt::ApplicationInactive);

        QWindowSystemInterface::handleApplicationStateChanged(Qt::ApplicationSuspended);
        QTest::waitForEvents();
        QCOMPARE(Qt::ApplicationState(item->property("state").toInt()), Qt::ApplicationSuspended);
        QCOMPARE(Qt::ApplicationState(item->property("state2").toInt()), Qt::ApplicationSuspended);

        QWindowSystemInterface::handleApplicationStateChanged(Qt::ApplicationHidden);
        QTest::waitForEvents();
        QCOMPARE(Qt::ApplicationState(item->property("state").toInt()), Qt::ApplicationHidden);
        QCOMPARE(Qt::ApplicationState(item->property("state2").toInt()), Qt::ApplicationHidden);

    } else {
        // Otherwise, the application can only be in two states, Active and Inactive. These are
        // triggered by window activation.
        window.show();
        window.requestActivate();
        QTest::qWaitForWindowActive(&window);
        QVERIFY(QGuiApplication::focusWindow() == &window);
        QCOMPARE(Qt::ApplicationState(item->property("state").toInt()), Qt::ApplicationActive);
        QCOMPARE(Qt::ApplicationState(item->property("state2").toInt()), Qt::ApplicationActive);

        // not active again
        QWindowSystemInterface::handleWindowActivated(0);
        QTRY_VERIFY(QGuiApplication::focusWindow() != &window);
        QCOMPARE(Qt::ApplicationState(item->property("state").toInt()), Qt::ApplicationInactive);
        QCOMPARE(Qt::ApplicationState(item->property("state2").toInt()), Qt::ApplicationInactive);
    }
}

void tst_qquickapplication::layoutDirection()
{

    QQmlComponent component(&engine);
    component.setData("import QtQuick 2.0; Item { property bool layoutDirection: Qt.application.layoutDirection }", QUrl::fromLocalFile(""));
    QQuickItem *item = qobject_cast<QQuickItem *>(component.create());
    QVERIFY(item);
    QQuickView view;
    item->setParentItem(view.rootObject());

    // not mirrored
    QCOMPARE(Qt::LayoutDirection(item->property("layoutDirection").toInt()), Qt::LeftToRight);

    // mirrored
    QGuiApplication::setLayoutDirection(Qt::RightToLeft);
    QCOMPARE(Qt::LayoutDirection(item->property("layoutDirection").toInt()), Qt::RightToLeft);

    // not mirrored again
    QGuiApplication::setLayoutDirection(Qt::LeftToRight);
    QCOMPARE(Qt::LayoutDirection(item->property("layoutDirection").toInt()), Qt::LeftToRight);
}

void tst_qquickapplication::inputMethod()
{
    // technically not in QQuickApplication, but testing anyway here
    QQmlComponent component(&engine);
    component.setData("import QtQuick 2.0; Item { property variant inputMethod: Qt.inputMethod }", QUrl::fromLocalFile(""));
    QQuickItem *item = qobject_cast<QQuickItem *>(component.create());
    QVERIFY(item);
    QQuickView view;
    item->setParentItem(view.rootObject());

    // check that the inputMethod property maches with application's input method
    QCOMPARE(qvariant_cast<QObject*>(item->property("inputMethod")), qApp->inputMethod());
}


QTEST_MAIN(tst_qquickapplication)

#include "tst_qquickapplication.moc"
