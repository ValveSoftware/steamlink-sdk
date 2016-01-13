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

#include <QtTest/QtTest>
#include <QWinThumbnailToolBar>
#include <QWinThumbnailToolButton>

class tst_QWinThumbnailToolBar : public QObject
{
    Q_OBJECT

private slots:
    void testWindow();
    void testButtons();
};

void tst_QWinThumbnailToolBar::testWindow()
{
    QWindow window;

    QWinThumbnailToolBar tbar1;
    QVERIFY(!tbar1.window());
    tbar1.setWindow(&window);
    QCOMPARE(tbar1.window(), &window);

    QWinThumbnailToolBar *tbar2 = new QWinThumbnailToolBar(&window);
    QCOMPARE(tbar2->window(), &window);
    tbar2->setWindow(0);
    QVERIFY(!tbar2->window());
}

void tst_QWinThumbnailToolBar::testButtons()
{
    QWinThumbnailToolBar tbar;
    QCOMPARE(tbar.count(), 0);
    QVERIFY(tbar.buttons().isEmpty());

    tbar.addButton(0);
    QCOMPARE(tbar.count(), 0);
    QVERIFY(tbar.buttons().isEmpty());

    QWinThumbnailToolButton *btn1 = new QWinThumbnailToolButton;
    QWinThumbnailToolButton *btn2 = new QWinThumbnailToolButton;

    tbar.addButton(btn1);
    QCOMPARE(tbar.count(), 1);
    QCOMPARE(tbar.buttons().count(), 1);
    QCOMPARE(tbar.buttons().at(0), btn1);

    tbar.addButton(btn2);
    QCOMPARE(tbar.count(), 2);
    QCOMPARE(tbar.buttons().count(), 2);
    QCOMPARE(tbar.buttons().at(0), btn1);
    QCOMPARE(tbar.buttons().at(1), btn2);

    tbar.clear();
    QCOMPARE(tbar.count(), 0);
    QVERIFY(tbar.buttons().isEmpty());

    QList<QWinThumbnailToolButton *> buttons;
    for (int i = 0; i < 3; ++i)
        buttons << new QWinThumbnailToolButton;

    tbar.setButtons(buttons);
    QCOMPARE(tbar.count(), buttons.count());
    QCOMPARE(tbar.buttons().count(), buttons.count());
    for (int i = 0; i < buttons.count(); ++i)
        QCOMPARE(tbar.buttons().at(i), buttons.at(i));

    tbar.removeButton(buttons.at(1));
    QCOMPARE(tbar.count(), 2);
    QCOMPARE(tbar.buttons().count(), 2);
    QCOMPARE(tbar.buttons().at(0), buttons.at(0));
    QCOMPARE(tbar.buttons().at(1), buttons.at(2));

    tbar.removeButton(buttons.at(2));
    QCOMPARE(tbar.count(), 1);
    QCOMPARE(tbar.buttons().count(), 1);
    QCOMPARE(tbar.buttons().at(0), buttons.at(0));

    tbar.removeButton(buttons.at(0));
    QCOMPARE(tbar.count(), 0);
    QVERIFY(tbar.buttons().isEmpty());
}

QTEST_MAIN(tst_QWinThumbnailToolBar)

#include "tst_qwinthumbnailtoolbar.moc"
