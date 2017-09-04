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
#include <private/qquicktimeline_p_p.h>
#include <limits>

class tst_QQuickTimeLine : public QObject
{
    Q_OBJECT

private slots:
    void overflow();
};

void tst_QQuickTimeLine::overflow()
{
    // Test ensures that time value used in QQuickTimeLine::accel methods is always positive.
    // On platforms where casting qreal value infinity to int yields a positive value this is
    // always the case and the test would fail. Strictly speaking, the cast is undefined behavior.
    if (static_cast<int>(qInf()) > 0)
        QSKIP("Test is not applicable on this platform");

    QQuickTimeLine timeline;
    QQuickTimeLineValue value;

    // overflow -> negative time -> assertion failure (QTBUG-35046)
    QCOMPARE(timeline.accel(value, std::numeric_limits<qreal>::max(), 0.0001f), -1);
    QCOMPARE(timeline.accel(value, std::numeric_limits<qreal>::max(), 0.0001f, 0.0001f), -1);
    QCOMPARE(timeline.accelDistance(value, 0.0001f, std::numeric_limits<qreal>::max()), -1);
}

QTEST_MAIN(tst_QQuickTimeLine)

#include "tst_qquicktimeline.moc"
