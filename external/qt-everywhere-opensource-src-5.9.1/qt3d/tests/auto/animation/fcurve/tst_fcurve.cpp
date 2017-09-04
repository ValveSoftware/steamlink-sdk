/****************************************************************************
**
** Copyright (C) 2015 Klaralvdalens Datakonsult AB (KDAB).
** Contact: https://www.qt.io/licensing/
**
** This file is part of the Qt3D module of the Qt Toolkit.
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

#include <QtTest/QTest>
#include <private/fcurve_p.h>

using namespace Qt3DAnimation;
using namespace Qt3DAnimation::Animation;

class tst_FCurve : public QObject
{
    Q_OBJECT

private Q_SLOTS:
    void checkDefaultConstruction()
    {
        // WHEN
        FCurve fcurve;

        // THEN
        QCOMPARE(fcurve.keyframeCount(), 0);
        QCOMPARE(fcurve.startTime(), 0.0f);
        QCOMPARE(fcurve.endTime(), 0.0f);
    }

    void checkAddingKeyframes()
    {
        // GIVEN
        FCurve fcurve;

        // WHEN
        const Keyframe kf0{0.0f, {-5.0f, 0.0f}, {5.0f, 0.0f}, QKeyFrame::BezierInterpolation};
        fcurve.appendKeyframe(0.0f, kf0);

        // THEN
        QCOMPARE(fcurve.keyframeCount(), 1);
        QCOMPARE(fcurve.startTime(), 0.0f);
        QCOMPARE(fcurve.endTime(), 0.0f);

        // WHEN
        const Keyframe kf1{5.0f, {45.0f, 5.0f}, {55.0f, 5.0f}, QKeyFrame::BezierInterpolation};
        fcurve.appendKeyframe(50.0f, kf1);

        // THEN
        QCOMPARE(fcurve.keyframeCount(), 2);
        QCOMPARE(fcurve.startTime(), 0.0f);
        QCOMPARE(fcurve.endTime(), 50.0f);
        QCOMPARE(fcurve.keyframe(0), kf0);
        QCOMPARE(fcurve.keyframe(1), kf1);
    }

    void checkClearKeyframes()
    {
        // GIVEN
        FCurve fcurve;
        fcurve.appendKeyframe(0.0f, Keyframe{0.0f, {-5.0f, 0.0f}, {5.0f, 0.0f}, QKeyFrame::BezierInterpolation});
        fcurve.appendKeyframe(50.0f, Keyframe{5.0f, {45.0f, 5.0f}, {55.0f, 5.0f}, QKeyFrame::BezierInterpolation});

        // WHEN
        fcurve.clearKeyframes();

        // THEN
        QCOMPARE(fcurve.keyframeCount(), 0);
        QCOMPARE(fcurve.startTime(), 0.0f);
        QCOMPARE(fcurve.endTime(), 0.0f);
    }

    void checkEvaluateAtTime()
    {

    }
};

QTEST_APPLESS_MAIN(tst_FCurve)

#include "tst_fcurve.moc"
