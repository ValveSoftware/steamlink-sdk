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
#include <QWinTaskbarButton>
#include <QWinTaskbarProgress>

class tst_QWinTaskbarProgress : public QObject
{
    Q_OBJECT

private slots:
    void testValue();
    void testRange();
    void testPause();
    void testVisibility();
    void testStop();
};

void tst_QWinTaskbarProgress::testValue()
{
    QWinTaskbarButton btn;
    QWinTaskbarProgress *progress = btn.progress();
    QVERIFY(progress);
    QCOMPARE(progress->value(), 0);
    QCOMPARE(progress->minimum(), 0);
    QCOMPARE(progress->maximum(), 100);

    QSignalSpy valueSpy(progress, SIGNAL(valueChanged(int)));
    QVERIFY(valueSpy.isValid());

    progress->setValue(25);
    QCOMPARE(progress->value(), 25);
    QCOMPARE(valueSpy.count(), 1);
    QCOMPARE(valueSpy.last().at(0).toInt(), 25);

    // under valid range -> no effect
    progress->setValue(-50);
    QCOMPARE(progress->value(), 25);
    QCOMPARE(valueSpy.count(), 1);

    progress->setValue(75);
    QCOMPARE(progress->value(), 75);
    QCOMPARE(valueSpy.count(), 2);
    QCOMPARE(valueSpy.last().at(0).toInt(), 75);

    // over valid range -> no effect
    progress->setValue(150);
    QCOMPARE(progress->value(), 75);
    QCOMPARE(valueSpy.count(), 2);
}

void tst_QWinTaskbarProgress::testRange()
{
    QWinTaskbarButton btn;
    QWinTaskbarProgress *progress = btn.progress();
    QVERIFY(progress);
    QCOMPARE(progress->value(), 0);
    QCOMPARE(progress->minimum(), 0);
    QCOMPARE(progress->maximum(), 100);

    QSignalSpy valueSpy(progress, SIGNAL(valueChanged(int)));
    QSignalSpy minimumSpy(progress, SIGNAL(minimumChanged(int)));
    QSignalSpy maximumSpy(progress, SIGNAL(maximumChanged(int)));
    QVERIFY(valueSpy.isValid());
    QVERIFY(minimumSpy.isValid());
    QVERIFY(maximumSpy.isValid());

    // value must remain intact
    progress->setValue(50);
    QCOMPARE(valueSpy.count(), 1);

    progress->setMinimum(25);
    QCOMPARE(progress->minimum(), 25);
    QCOMPARE(minimumSpy.count(), 1);
    QCOMPARE(minimumSpy.last().at(0).toInt(), 25);

    progress->setMaximum(75);
    QCOMPARE(progress->maximum(), 75);
    QCOMPARE(maximumSpy.count(), 1);
    QCOMPARE(maximumSpy.last().at(0).toInt(), 75);

    QCOMPARE(progress->value(), 50);
    QCOMPARE(valueSpy.count(), 1);

    // value under valid range -> reset
    progress->setMinimum(55);
    QCOMPARE(progress->value(), 55);
    QCOMPARE(progress->minimum(), 55);
    QCOMPARE(valueSpy.count(), 2);
    QCOMPARE(valueSpy.last().at(0).toInt(), 55);
    QCOMPARE(minimumSpy.count(), 2);
    QCOMPARE(minimumSpy.last().at(0).toInt(), 55);

    progress->setValue(70);
    QCOMPARE(valueSpy.count(), 3);

    // value over valid range -> reset
    progress->setMaximum(65);
    QCOMPARE(progress->value(), 55);
    QCOMPARE(progress->maximum(), 65);
    QCOMPARE(valueSpy.count(), 4);
    QCOMPARE(valueSpy.last().at(0).toInt(), 55);
    QCOMPARE(maximumSpy.count(), 2);
    QCOMPARE(maximumSpy.last().at(0).toInt(), 65);
}

void tst_QWinTaskbarProgress::testPause()
{
    QWinTaskbarButton btn;
    QWinTaskbarProgress *progress = btn.progress();
    QVERIFY(progress);
    QVERIFY(!progress->isPaused());

    QSignalSpy pausedSpy(progress, SIGNAL(pausedChanged(bool)));
    QVERIFY(pausedSpy.isValid());

    progress->setPaused(true);
    QVERIFY(progress->isPaused());
    QCOMPARE(pausedSpy.count(), 1);
    QCOMPARE(pausedSpy.last().at(0).toBool(), true);

    progress->setPaused(false);
    QVERIFY(!progress->isPaused());
    QCOMPARE(pausedSpy.count(), 2);
    QCOMPARE(pausedSpy.last().at(0).toBool(), false);

    progress->stop();
    progress->pause();
    QVERIFY(!progress->isPaused());
    QCOMPARE(pausedSpy.count(), 2);

    progress->resume();
    progress->pause();
    QVERIFY(progress->isPaused());
    QCOMPARE(pausedSpy.count(), 3);
    QCOMPARE(pausedSpy.last().at(0).toBool(), true);

    progress->resume();
    QVERIFY(!progress->isPaused());
    QCOMPARE(pausedSpy.count(), 4);
    QCOMPARE(pausedSpy.last().at(0).toBool(), false);
}

void tst_QWinTaskbarProgress::testVisibility()
{
    QWinTaskbarButton btn;
    QWinTaskbarProgress *progress = btn.progress();
    QVERIFY(progress);
    QVERIFY(!progress->isVisible());

    QSignalSpy visibleSpy(progress, SIGNAL(visibilityChanged(bool)));
    QVERIFY(visibleSpy.isValid());

    progress->setVisible(true);
    QVERIFY(progress->isVisible());
    QCOMPARE(visibleSpy.count(), 1);
    QCOMPARE(visibleSpy.last().at(0).toBool(), true);

    progress->setVisible(false);
    QVERIFY(!progress->isVisible());
    QCOMPARE(visibleSpy.count(), 2);
    QCOMPARE(visibleSpy.last().at(0).toBool(), false);
}

void tst_QWinTaskbarProgress::testStop()
{
    QWinTaskbarButton btn;
    QWinTaskbarProgress *progress = btn.progress();
    QVERIFY(progress);
    QVERIFY(!progress->isStopped());

    QSignalSpy stoppedSpy(progress, SIGNAL(stoppedChanged(bool)));
    QVERIFY(stoppedSpy.isValid());

    progress->pause();
    QVERIFY(progress->isPaused());
    QVERIFY(!progress->isStopped());
    progress->stop();
    QVERIFY(!progress->isPaused());
    QVERIFY(progress->isStopped());
    QCOMPARE(stoppedSpy.count(), 1);
    QCOMPARE(stoppedSpy.last().at(0).toBool(), true);

    progress->resume();
    QVERIFY(!progress->isStopped());
    QCOMPARE(stoppedSpy.count(), 2);
    QCOMPARE(stoppedSpy.last().at(0).toBool(), false);
}

QTEST_MAIN(tst_QWinTaskbarProgress)

#include "tst_qwintaskbarprogress.moc"
