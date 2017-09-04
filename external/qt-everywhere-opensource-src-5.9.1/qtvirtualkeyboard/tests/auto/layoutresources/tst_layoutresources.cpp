/****************************************************************************
**
** Copyright (C) 2016 Pelagicore AB
** Contact: https://www.qt.io/licensing/
**
** This file is part of the Qt Virtual Keyboard module of the Qt Toolkit.
**
** $QT_BEGIN_LICENSE:GPL$
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
** General Public License version 3 or (at your option) any later version
** approved by the KDE Free Qt Foundation. The licenses are as published by
** the Free Software Foundation and appearing in the file LICENSE.GPL3
** included in the packaging of this file. Please review the following
** information to ensure the GNU General Public License requirements will
** be met: https://www.gnu.org/licenses/gpl-3.0.html.
**
** $QT_END_LICENSE$
**
****************************************************************************/

#include "layouttesthelper.h"

#include <QtTest/qtest.h>

static bool moduleEnv = qputenv("QT_IM_MODULE", QByteArray("qtvirtualkeyboard"));

class tst_layoutresources : public QObject
{
    Q_OBJECT

public:

private slots:
    void layouts();
};

void tst_layoutresources::layouts()
{
    QString layoutPath("qrc:/resource/layouts");

    qputenv("QT_VIRTUALKEYBOARD_LAYOUT_PATH", qPrintable(layoutPath));

    LayoutTestHelper layoutTestHelper;

    layoutTestHelper.window->show();
    QVERIFY(QTest::qWaitForWindowExposed(layoutTestHelper.window.data()));

    QObject *layout = layoutTestHelper.window->findChild<QObject*>("en_GB");
    QVERIFY(layout);

    QObject *settings = layoutTestHelper.window->property("settings").value<QObject*>();
    QVERIFY(settings);
    // availableLocales is based off of FolderListModel, which uses a QThread to
    // offload the work of getting the contents of a directory. Being
    // asynchronous, it can take a varying amount of time.
    QTRY_COMPARE(settings->property("availableLocales").toStringList(), QStringList() << "en_GB");
}

QTEST_MAIN(tst_layoutresources)

#include "tst_layoutresources.moc"
