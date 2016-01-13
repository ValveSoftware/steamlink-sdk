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
#include <QDebug>
#include <QtQuick/QQuickItem>
#include <QtQuick/QQuickView>
#include <QtGui/QScreen>
#include "../../shared/util.h"

class tst_qquickscreen : public QQmlDataTest
{
    Q_OBJECT
private slots:
    void basicProperties();
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

QTEST_MAIN(tst_qquickscreen)

#include "tst_qquickscreen.moc"
