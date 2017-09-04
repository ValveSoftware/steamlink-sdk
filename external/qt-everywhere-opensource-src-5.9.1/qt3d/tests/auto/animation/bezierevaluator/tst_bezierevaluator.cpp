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
#include <Qt3DAnimation/private/bezierevaluator_p.h>
#include <Qt3DAnimation/private/keyframe_p.h>
#include <QtCore/qvector.h>

#include <cmath>

Q_DECLARE_METATYPE(Qt3DAnimation::Animation::Keyframe)

using namespace Qt3DAnimation;
using namespace Qt3DAnimation::Animation;

class tst_BezierEvaluator : public QObject
{
    Q_OBJECT

private Q_SLOTS:
    void checkFindCubicRoots_data()
    {
        // Test data verified on Wolfram Alpha with snippets such as:
        // Plot[x^3-5x^2+x+3,{x,-3,6}]
        // Solve[x^3-5x^2+x+3,x]
        // If you need more, try these at https://www.wolframalpha.com/

        QTest::addColumn<float>("a");
        QTest::addColumn<float>("b");
        QTest::addColumn<float>("c");
        QTest::addColumn<float>("d");
        QTest::addColumn<int>("rootCount");
        QTest::addColumn<QVector<float>>("roots");

        float a = 1.0f;
        float b = 0.0f;
        float c = 0.0f;
        float d = 0.0f;
        int rootCount = 1;
        QVector<float> roots = { 0.0f };
        QTest::newRow("a=1, b=0, c=0, d=0") << a << b << c << d << rootCount << roots;
        roots.clear();

        a = 1.0f;
        b = -1.0f;
        c = 1.0f;
        d = -1.0f;
        rootCount = 1;
        roots.resize(1);
        roots[0] = 1.0f;
        QTest::newRow("a=1, b=-1, c=1, d=-1") << a << b << c << d << rootCount << roots;
        roots.clear();

        a = 1.0f;
        b = -2.0f;
        c = 1.0f;
        d = -1.0f;
        rootCount = 1;
        roots.resize(1);
        roots[0] = 1.7548776f;
        QTest::newRow("a=1, b=-2, c=1, d=-1") << a << b << c << d << rootCount << roots;
        roots.clear();

        a = 1.0f;
        b = -5.0f;
        c = 1.0f;
        d = 3.0f;
        rootCount = 3;
        roots.resize(3);
        roots[0] = 2.0f + std::sqrt(7.0f);
        roots[1] = 1.0f;
        roots[2] = 2.0f - std::sqrt(7.0f);
        QTest::newRow("a=1, b=-5, c=1, d=3") << a << b << c << d << rootCount << roots;
        roots.clear();
    }

    void checkFindCubicRoots()
    {
        QFETCH(float, a);
        QFETCH(float, b);
        QFETCH(float, c);
        QFETCH(float, d);
        QFETCH(int, rootCount);
        QFETCH(QVector<float>, roots);

        float coeffs[4];
        coeffs[0] = d;
        coeffs[1] = c;
        coeffs[2] = b;
        coeffs[3] = a;

        float results[3];
        const int foundRootCount = BezierEvaluator::findCubicRoots(coeffs, results);

        QCOMPARE(foundRootCount, rootCount);
        for (int i = 0; i < rootCount; ++i)
            QCOMPARE(results[i], roots[i]);
    }

    void checkParameterForTime_data()
    {
        QTest::addColumn<float>("t0");
        QTest::addColumn<Keyframe>("kf0");
        QTest::addColumn<float>("t1");
        QTest::addColumn<Keyframe>("kf1");
        QTest::addColumn<QVector<float>>("times");
        QTest::addColumn<QVector<float>>("bezierParamters");

        float t0 = 0.0f;
        Keyframe kf0{0.0f, {-5.0f, 0.0f}, {5.0f, 0.0f}, QKeyFrame::BezierInterpolation};
        float t1 = 50.0f;
        Keyframe kf1{5.0f, {45.0f, 5.0f}, {55.0f, 5.0f}, QKeyFrame::BezierInterpolation};
        const int count = 21;
        QVector<float> times = (QVector<float>()
            << 0.0f
            << 1.00375f
            << 2.48f
            << 4.37625f
            << 6.64f
            << 9.21875f
            << 12.06f
            << 15.11125f
            << 18.32f
            << 21.63375f
            << 25.0f
            << 28.36625f
            << 31.68f
            << 34.88875f
            << 37.94f
            << 40.78125f
            << 43.36f
            << 45.62375f
            << 47.52f
            << 48.99625f
            << 50.0f);

        QVector<float> bezierParameters;
        float deltaU = 1.0f / float(count - 1);
        for (int i = 0; i < count; ++i)
            bezierParameters.push_back(float(i) * deltaU);

        QTest::newRow("t=0 to t=50, default easing") << t0 << kf0
                                                     << t1 << kf1
                                                     << times << bezierParameters;
    }

    void checkParameterForTime()
    {
        // GIVEN
        QFETCH(float, t0);
        QFETCH(Keyframe, kf0);
        QFETCH(float, t1);
        QFETCH(Keyframe, kf1);
        QFETCH(QVector<float>, times);
        QFETCH(QVector<float>, bezierParamters);

        // WHEN
        BezierEvaluator bezier(t0, kf0, t1, kf1);

        // THEN
        for (int i = 0; i < times.size(); ++i) {
            const float time = times[i];
            const float u = bezier.parameterForTime(time);
            QCOMPARE(u, bezierParamters[i]);
        }
    }

    void checkValueForTime_data()
    {
        QTest::addColumn<float>("t0");
        QTest::addColumn<Keyframe>("kf0");
        QTest::addColumn<float>("t1");
        QTest::addColumn<Keyframe>("kf1");
        QTest::addColumn<QVector<float>>("times");
        QTest::addColumn<QVector<float>>("values");

        float t0 = 0.0f;
        Keyframe kf0{0.0f, {-5.0f, 0.0f}, {5.0f, 0.0f}, QKeyFrame::BezierInterpolation};
        float t1 = 50.0f;
        Keyframe kf1{5.0f, {45.0f, 5.0f}, {55.0f, 5.0f}, QKeyFrame::BezierInterpolation};
        QVector<float> times = (QVector<float>()
            << 0.0f
            << 1.00375f
            << 2.48f
            << 4.37625f
            << 6.64f
            << 9.21875f
            << 12.06f
            << 15.11125f
            << 18.32f
            << 21.63375f
            << 25.0f
            << 28.36625f
            << 31.68f
            << 34.88875f
            << 37.94f
            << 40.78125f
            << 43.36f
            << 45.62375f
            << 47.52f
            << 48.99625f
            << 50.0f);

        QVector<float> values = (QVector<float>()
            << 0.0f
            << 0.03625f
            << 0.14f
            << 0.30375f
            << 0.52f
            << 0.78125f
            << 1.08f
            << 1.40875f
            << 1.76f
            << 2.12625f
            << 2.5f
            << 2.87375f
            << 3.24f
            << 3.59125f
            << 3.92f
            << 4.21875f
            << 4.48f
            << 4.69625f
            << 4.86f
            << 4.96375f
            << 5.0f);

        QTest::newRow("t=0, value=0 to t=50, value=5, default easing") << t0 << kf0
                                                     << t1 << kf1
                                                     << times << values;
    }

    void checkValueForTime()
    {
        // GIVEN
        QFETCH(float, t0);
        QFETCH(Keyframe, kf0);
        QFETCH(float, t1);
        QFETCH(Keyframe, kf1);
        QFETCH(QVector<float>, times);
        QFETCH(QVector<float>, values);

        // WHEN
        BezierEvaluator bezier(t0, kf0, t1, kf1);

        // THEN
        for (int i = 0; i < times.size(); ++i) {
            const float time = times[i];
            const float value = bezier.valueForTime(time);
            QCOMPARE(value, values[i]);
        }
    }
};

QTEST_APPLESS_MAIN(tst_BezierEvaluator)

#include "tst_bezierevaluator.moc"
