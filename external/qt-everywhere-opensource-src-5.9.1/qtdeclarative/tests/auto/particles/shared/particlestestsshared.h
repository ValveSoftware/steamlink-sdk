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

#ifndef PARTICLES_TESTS_SHARED
#define PARTICLES_TESTS_SHARED
#include <QtQuick/QQuickView>
#include <QtTest>
#include <QAbstractAnimation>
const qreal EPSILON = 0.0001;

bool extremelyFuzzyCompare(qreal a, qreal b, qreal e)//For cases which can have larger variances
{
    return (a + e >= b) && (a - e <= b);
}

bool myFuzzyCompare(qreal a, qreal b)//For cases which might be near 0 so qFuzzyCompare fails
{
    return (a + EPSILON > b) && (a - EPSILON < b);
}

bool myFuzzyLEQ(qreal a, qreal b)
{
    return (a - EPSILON < b);
}

bool myFuzzyGEQ(qreal a, qreal b)
{
    return (a + EPSILON > b);
}

QQuickView* createView(const QUrl &filename, int additionalWait=0)
{
    QQuickView *view = new QQuickView(0);

    view->setSource(filename);
    if (view->status() != QQuickView::Ready)
        return 0;
    view->show();
    QTest::qWaitForWindowExposed(view);
    if (additionalWait)
        QTest::qWait(additionalWait);

    return view;
}

void ensureAnimTime(int requiredTime, QAbstractAnimation* anim)//With consistentTiming, who knows how long an animation really takes...
{
    while (anim->currentTime() < requiredTime)
        QTest::qWait(100);
}

#endif
