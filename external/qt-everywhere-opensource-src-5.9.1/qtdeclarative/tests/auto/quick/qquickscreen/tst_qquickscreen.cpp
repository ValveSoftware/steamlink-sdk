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
#include <QDebug>
#include <QQmlEngine>
#include <QtQuick/QQuickItem>
#include <QtQuick/QQuickView>
#include <QtGui/QScreen>
#include "../../shared/util.h"
#include <QtQuick/private/qquickscreen_p.h>
#include <QDebug>
class tst_qquickscreen : public QQmlDataTest
{
    Q_OBJECT
private slots:
    void basicProperties();
    void screenOnStartup();
    void fullScreenList();
};

void tst_qquickscreen::basicProperties()
{
    QQuickView view;
    view.setSource(testFileUrl("screen.qml"));
    view.show();
    QVERIFY(QTest::qWaitForWindowExposed(&view));

    QQuickItem* root = view.rootObject();
    QVERIFY(root);

    QScreen* screen = view.screen();
    QVERIFY(screen);

    QCOMPARE(screen->size().width(), root->property("w").toInt());
    QCOMPARE(screen->size().height(), root->property("h").toInt());
    QCOMPARE(int(screen->orientation()), root->property("curOrientation").toInt());
    QCOMPARE(int(screen->primaryOrientation()), root->property("priOrientation").toInt());
    QCOMPARE(int(screen->orientationUpdateMask()), root->property("updateMask").toInt());
    QCOMPARE(screen->devicePixelRatio(), root->property("devicePixelRatio").toReal());
    QVERIFY(screen->devicePixelRatio() >= 1.0);
    QCOMPARE(screen->geometry().x(), root->property("vx").toInt());
    QCOMPARE(screen->geometry().y(), root->property("vy").toInt());

    QVERIFY(root->property("screenCount").toInt() == QGuiApplication::screens().count());
}

void tst_qquickscreen::screenOnStartup()
{
    // We expect QQuickScreen to fall back to the primary screen
    QQmlEngine engine;
    QQmlComponent component(&engine, testFileUrl("screen.qml"));

    QScopedPointer<QQuickItem> root(qobject_cast<QQuickItem*>(component.create()));
    QVERIFY(root);

    QScreen* screen = QGuiApplication::primaryScreen();
    QVERIFY(screen);

    QCOMPARE(screen->size().width(), root->property("w").toInt());
    QCOMPARE(screen->size().height(), root->property("h").toInt());
    QCOMPARE(int(screen->orientation()), root->property("curOrientation").toInt());
    QCOMPARE(int(screen->primaryOrientation()), root->property("priOrientation").toInt());
    QCOMPARE(int(screen->orientationUpdateMask()), root->property("updateMask").toInt());
    QCOMPARE(screen->devicePixelRatio(), root->property("devicePixelRatio").toReal());
    QVERIFY(screen->devicePixelRatio() >= 1.0);
    QCOMPARE(screen->geometry().x(), root->property("vx").toInt());
    QCOMPARE(screen->geometry().y(), root->property("vy").toInt());
}

void tst_qquickscreen::fullScreenList()
{
    QQuickView view;
    view.setSource(testFileUrl("screen.qml"));
    view.show();
    QVERIFY(QTest::qWaitForWindowExposed(&view));

    QQuickItem* root = view.rootObject();
    QVERIFY(root);

    QJSValue screensArray = root->property("allScreens").value<QJSValue>();
    QVERIFY(screensArray.isArray());
    int length = screensArray.property("length").toInt();
    const QList<QScreen *> screenList = QGuiApplication::screens();
    QVERIFY(length == screenList.count());

    for (int i = 0; i < length; ++i) {
        QQuickScreenInfo *info = qobject_cast<QQuickScreenInfo *>(screensArray.property(i).toQObject());
        QVERIFY(info != nullptr);
        QCOMPARE(screenList[i]->name(), info->name());
        QCOMPARE(screenList[i]->size().width(), info->width());
        QCOMPARE(screenList[i]->size().height(), info->height());
        QCOMPARE(screenList[i]->availableVirtualGeometry().width(), info->desktopAvailableWidth());
        QCOMPARE(screenList[i]->availableVirtualGeometry().height(), info->desktopAvailableHeight());
        QCOMPARE(screenList[i]->devicePixelRatio(), info->devicePixelRatio());
        QCOMPARE(screenList[i]->geometry().x(), info->virtualX());
        QCOMPARE(screenList[i]->geometry().y(), info->virtualY());
    }
}

QTEST_MAIN(tst_qquickscreen)

#include "tst_qquickscreen.moc"
