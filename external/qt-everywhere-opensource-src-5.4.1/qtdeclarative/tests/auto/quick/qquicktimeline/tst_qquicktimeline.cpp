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
