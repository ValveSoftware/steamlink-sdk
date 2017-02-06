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

class tst_qquickscreen : public QQmlDataTest
{
    Q_OBJECT
private slots:
    void basicProperties();
    void screenOnStartup();
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
}

QTEST_MAIN(tst_qquickscreen)

#include "tst_qquickscreen.moc"
