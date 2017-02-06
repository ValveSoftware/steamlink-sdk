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
#include <QtQml/qqmlcomponent.h>
#include <QtQml/qqmlengine.h>
#include <QtQuick/qquickitem.h>
#include <QtQuick/qquickview.h>
#include <QtGui/qinputmethod.h>
#include <QtGui/qstylehints.h>
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
    void font();
    void inputMethod();
    void styleHints();
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
    QCOMPARE(QGuiApplication::focusWindow(), &window);
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

    // If the platform plugin has the ApplicationState capability, state changes originate from it
    // as a result of a system event. We therefore have to simulate these events here.
    if (QGuiApplicationPrivate::platformIntegration()->hasCapability(QPlatformIntegration::ApplicationState)) {

        // Flush pending events, in case the platform have already queued real application state events
        QWindowSystemInterface::flushWindowSystemEvents();

        QWindowSystemInterface::handleApplicationStateChanged(Qt::ApplicationActive);
        QWindowSystemInterface::flushWindowSystemEvents();
        QCOMPARE(Qt::ApplicationState(item->property("state").toInt()), Qt::ApplicationActive);
        QCOMPARE(Qt::ApplicationState(item->property("state2").toInt()), Qt::ApplicationActive);

        QWindowSystemInterface::handleApplicationStateChanged(Qt::ApplicationInactive);
        QWindowSystemInterface::flushWindowSystemEvents();
        QCOMPARE(Qt::ApplicationState(item->property("state").toInt()), Qt::ApplicationInactive);
        QCOMPARE(Qt::ApplicationState(item->property("state2").toInt()), Qt::ApplicationInactive);

        QWindowSystemInterface::handleApplicationStateChanged(Qt::ApplicationSuspended);
        QWindowSystemInterface::flushWindowSystemEvents();
        QCOMPARE(Qt::ApplicationState(item->property("state").toInt()), Qt::ApplicationSuspended);
        QCOMPARE(Qt::ApplicationState(item->property("state2").toInt()), Qt::ApplicationSuspended);

        QWindowSystemInterface::handleApplicationStateChanged(Qt::ApplicationHidden);
        QWindowSystemInterface::flushWindowSystemEvents();
        QCOMPARE(Qt::ApplicationState(item->property("state").toInt()), Qt::ApplicationHidden);
        QCOMPARE(Qt::ApplicationState(item->property("state2").toInt()), Qt::ApplicationHidden);

    } else {
        // Otherwise, the application can only be in two states, Active and Inactive. These are
        // triggered by window activation.
        window.show();
        window.requestActivate();
        QTest::qWaitForWindowActive(&window);
        QCOMPARE(QGuiApplication::focusWindow(), &window);
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

void tst_qquickapplication::font()
{
    QQmlComponent component(&engine);
    component.setData("import QtQuick 2.0; Item { property font defaultFont: Qt.application.font }", QUrl::fromLocalFile(""));
    QQuickItem *item = qobject_cast<QQuickItem *>(component.create());
    QVERIFY(item);
    QQuickView view;
    item->setParentItem(view.rootObject());

    QVariant defaultFontProperty = item->property("defaultFont");
    QVERIFY(defaultFontProperty.isValid());
    QCOMPARE(defaultFontProperty.type(), QVariant::Font);
    QCOMPARE(defaultFontProperty.value<QFont>(), qApp->font());
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

void tst_qquickapplication::styleHints()
{
    // technically not in QQuickApplication, but testing anyway here
    QQmlComponent component(&engine);
    component.setData("import QtQuick 2.0; Item { property variant styleHints: Qt.styleHints }", QUrl::fromLocalFile(""));
    QQuickItem *item = qobject_cast<QQuickItem *>(component.create());
    QVERIFY(item);
    QQuickView view;
    item->setParentItem(view.rootObject());

    QCOMPARE(qvariant_cast<QObject*>(item->property("styleHints")), qApp->styleHints());
}

QTEST_MAIN(tst_qquickapplication)

#include "tst_qquickapplication.moc"
