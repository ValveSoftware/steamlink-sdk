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
#include <QWinTaskbarButton>
#include <QWinTaskbarProgress>

class tst_QWinTaskbarButton : public QObject
{
    Q_OBJECT

private slots:
    void testWindow();
    void testOverlayIcon();
    void testOverlayAccessibleDescription();
    void testProgress();
};

void tst_QWinTaskbarButton::testWindow()
{
    QWindow window;

    QWinTaskbarButton btn1;
    QVERIFY(!btn1.window());
    btn1.setWindow(&window);
    QCOMPARE(btn1.window(), &window);

    QWinTaskbarButton *btn2 = new QWinTaskbarButton(&window);
    QCOMPARE(btn2->window(), &window);
    btn2->setWindow(0);
    QVERIFY(!btn2->window());
}

void tst_QWinTaskbarButton::testOverlayIcon()
{
    QWinTaskbarButton btn;
    QVERIFY(btn.overlayIcon().isNull());

    QIcon icon;
    QPixmap pixmap(64, 64);
    pixmap.fill(Qt::transparent);
    icon.addPixmap(pixmap);

    btn.setOverlayIcon(icon);
    QCOMPARE(btn.overlayIcon(), icon);

    btn.clearOverlayIcon();
    QVERIFY(btn.overlayIcon().isNull());
}

void tst_QWinTaskbarButton::testOverlayAccessibleDescription()
{
    QWinTaskbarButton btn;
    QVERIFY(btn.overlayAccessibleDescription().isNull());

    btn.setOverlayAccessibleDescription(QStringLiteral("Qt"));
    QCOMPARE(btn.overlayAccessibleDescription(), QStringLiteral("Qt"));

    btn.setOverlayAccessibleDescription(QString());
    QVERIFY(btn.overlayAccessibleDescription().isNull());
}

void tst_QWinTaskbarButton::testProgress()
{
    QWinTaskbarButton btn;
    QVERIFY(btn.progress());
    QVERIFY(btn.progress()->objectName().isEmpty());

    btn.progress()->setObjectName(QStringLiteral("DEAD"));
    delete btn.progress();

    QVERIFY(btn.progress());
    QVERIFY(btn.progress()->objectName().isEmpty());
}

QTEST_MAIN(tst_QWinTaskbarButton)

#include "tst_qwintaskbarbutton.moc"
