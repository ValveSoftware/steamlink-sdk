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
