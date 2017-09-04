/****************************************************************************
**
** Copyright (C) 2016 The Qt Company Ltd.
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

#include <QtTest/qtest.h>
#include <Qt3DAnimation/qkeyframeanimation.h>
#include <qobject.h>
#include <qsignalspy.h>

class tst_QKeyframeAnimation : public QObject
{
    Q_OBJECT

private Q_SLOTS:

    void initTestCase()
    {
        qRegisterMetaType<Qt3DAnimation::QKeyframeAnimation::RepeatMode>(
                    "QKeyframeAnimation::RepeatMode");
    }

    void checkDefaultConstruction()
    {
        // GIVEN
        Qt3DAnimation::QKeyframeAnimation keyframeAnimation;

        // THEN
        QCOMPARE(keyframeAnimation.target(), nullptr);
        QCOMPARE(keyframeAnimation.easing(), QEasingCurve());
        QCOMPARE(keyframeAnimation.targetName(), QString());
        QCOMPARE(keyframeAnimation.startMode(), Qt3DAnimation::QKeyframeAnimation::Constant);
        QCOMPARE(keyframeAnimation.endMode(), Qt3DAnimation::QKeyframeAnimation::Constant);
    }

    void checkPropertyChanges()
    {
        // GIVEN
        Qt3DAnimation::QKeyframeAnimation keyframeAnimation;

        {
            // WHEN
            QScopedPointer<Qt3DCore::QTransform> transform(new Qt3DCore::QTransform);
            QSignalSpy spy(&keyframeAnimation, SIGNAL(targetChanged(Qt3DCore::QTransform *)));
            Qt3DCore::QTransform * newValue = transform.data();
            keyframeAnimation.setTarget(newValue);

            // THEN
            QCOMPARE(keyframeAnimation.target(), newValue);
            QCOMPARE(spy.count(), 1);

            // WHEN
            spy.clear();
            keyframeAnimation.setTarget(newValue);

            // THEN
            QCOMPARE(keyframeAnimation.target(), newValue);
            QCOMPARE(spy.count(), 0);

        }
        {
            // WHEN
            QSignalSpy spy(&keyframeAnimation, SIGNAL(easingChanged(const QEasingCurve &)));
            const QEasingCurve newValue = QEasingCurve(QEasingCurve::CosineCurve);
            keyframeAnimation.setEasing(newValue);

            // THEN
            QCOMPARE(keyframeAnimation.easing(), newValue);
            QCOMPARE(spy.count(), 1);

            // WHEN
            spy.clear();
            keyframeAnimation.setEasing(newValue);

            // THEN
            QCOMPARE(keyframeAnimation.easing(), newValue);
            QCOMPARE(spy.count(), 0);

        }
        {
            // WHEN
            QSignalSpy spy(&keyframeAnimation, SIGNAL(targetNameChanged(QString)));
            const QString newValue = QString("target");
            keyframeAnimation.setTargetName(newValue);

            // THEN
            QCOMPARE(keyframeAnimation.targetName(), newValue);
            QCOMPARE(spy.count(), 1);

            // WHEN
            spy.clear();
            keyframeAnimation.setTargetName(newValue);

            // THEN
            QCOMPARE(keyframeAnimation.targetName(), newValue);
            QCOMPARE(spy.count(), 0);

        }
        {
            // WHEN
            QSignalSpy spy(&keyframeAnimation,
                           SIGNAL(startModeChanged(QKeyframeAnimation::RepeatMode)));
            const Qt3DAnimation::QKeyframeAnimation::RepeatMode newValue
                    = Qt3DAnimation::QKeyframeAnimation::Repeat;
            keyframeAnimation.setStartMode(newValue);

            // THEN
            QCOMPARE(keyframeAnimation.startMode(), newValue);
            QCOMPARE(spy.count(), 1);

            // WHEN
            spy.clear();
            keyframeAnimation.setStartMode(newValue);

            // THEN
            QCOMPARE(keyframeAnimation.startMode(), newValue);
            QCOMPARE(spy.count(), 0);

        }
        {
            // WHEN
            QSignalSpy spy(&keyframeAnimation,
                           SIGNAL(endModeChanged(QKeyframeAnimation::RepeatMode)));
            const Qt3DAnimation::QKeyframeAnimation::RepeatMode newValue
                    = Qt3DAnimation::QKeyframeAnimation::Repeat;
            keyframeAnimation.setEndMode(newValue);

            // THEN
            QCOMPARE(keyframeAnimation.endMode(), newValue);
            QCOMPARE(spy.count(), 1);

            // WHEN
            spy.clear();
            keyframeAnimation.setEndMode(newValue);

            // THEN
            QCOMPARE(keyframeAnimation.endMode(), newValue);
            QCOMPARE(spy.count(), 0);

        }
    }

    void testAnimating()
    {
        // GIVEN
        Qt3DAnimation::QKeyframeAnimation keyframeAnimation;
        Qt3DCore::QTransform targetTransform;
        keyframeAnimation.setTarget(&targetTransform);

        const float positions[3] = {0.0f, 1.0f, 2.0f};

        Qt3DCore::QTransform keyframes[3];
        keyframes[0].setScale(1.0f);
        keyframes[1].setScale(2.0f);
        keyframes[2].setScale(3.0f);

        keyframes[0].setTranslation(QVector3D(0.0f, 0.0f, 0.0f));
        keyframes[1].setTranslation(QVector3D(1.0f, 1.0f, 0.0f));
        keyframes[2].setTranslation(QVector3D(2.0f, 0.0f, 2.0f));

        keyframes[0].setRotationX(0.0f);
        keyframes[1].setRotationY(45.0f);
        keyframes[2].setRotationZ(90.0f);

        QVector<float> framePositions;
        framePositions.push_back(positions[0]);
        framePositions.push_back(positions[1]);
        framePositions.push_back(positions[2]);

        QVector<Qt3DCore::QTransform*> frames;
        frames.push_back(&keyframes[0]);
        frames.push_back(&keyframes[1]);
        frames.push_back(&keyframes[2]);

        keyframeAnimation.setFramePositions(framePositions);
        keyframeAnimation.setKeyframes(frames);

        {
            // WHEN
            keyframeAnimation.setPosition(0.0f);

            // THEN
            QVERIFY(qFuzzyCompare(targetTransform.scale3D(), QVector3D(1.0f, 1.0f, 1.0f)));
            QVERIFY(qFuzzyCompare(targetTransform.translation(), QVector3D(0.0f, 0.0f, 0.0f)));
            QVERIFY(qFuzzyCompare(targetTransform.rotation(),
                                  QQuaternion::fromEulerAngles(QVector3D(0.0f, 0.0f, 0.0f))));

            // WHEN
            keyframeAnimation.setPosition(1.0f);

            // THEN
            QVERIFY(qFuzzyCompare(targetTransform.scale3D(), QVector3D(2.0f, 2.0f, 2.0f)));
            QVERIFY(qFuzzyCompare(targetTransform.translation(), QVector3D(1.0f, 1.0f, 0.0f)));
            QVERIFY(qFuzzyCompare(targetTransform.rotation(),
                                  QQuaternion::fromEulerAngles(QVector3D(0.0f, 45.0f, 0.0f))));

            // WHEN
            keyframeAnimation.setPosition(2.0f);

            // THEN
            QVERIFY(qFuzzyCompare(targetTransform.scale3D(), QVector3D(3.0f, 3.0f, 3.0f)));
            QVERIFY(qFuzzyCompare(targetTransform.translation(), QVector3D(2.0f, 0.0f, 2.0f)));
            QVERIFY(qFuzzyCompare(targetTransform.rotation(),
                                  QQuaternion::fromEulerAngles(QVector3D(0.0f, 0.0f, 90.0f))));

            // WHEN
            keyframeAnimation.setPosition(3.0f);

            // THEN
            QVERIFY(qFuzzyCompare(targetTransform.scale3D(), QVector3D(3.0f, 3.0f, 3.0f)));
            QVERIFY(qFuzzyCompare(targetTransform.translation(), QVector3D(2.0f, 0.0f, 2.0f)));
            QVERIFY(qFuzzyCompare(targetTransform.rotation(),
                                  QQuaternion::fromEulerAngles(QVector3D(0.0f, 0.0f, 90.0f))));

            // WHEN
            keyframeAnimation.setStartMode(Qt3DAnimation::QKeyframeAnimation::None);
            keyframeAnimation.setEndMode(Qt3DAnimation::QKeyframeAnimation::None);
            keyframeAnimation.setPosition(-1.0f);

            // THEN
            QVERIFY(qFuzzyCompare(targetTransform.scale3D(), QVector3D(3.0f, 3.0f, 3.0f)));
            QVERIFY(qFuzzyCompare(targetTransform.translation(), QVector3D(2.0f, 0.0f, 2.0f)));
            QVERIFY(qFuzzyCompare(targetTransform.rotation(),
                                  QQuaternion::fromEulerAngles(QVector3D(0.0f, 0.0f, 90.0f))));

            // WHEN
            keyframeAnimation.setPosition(5.0f);

            // THEN
            QVERIFY(qFuzzyCompare(targetTransform.scale3D(), QVector3D(3.0f, 3.0f, 3.0f)));
            QVERIFY(qFuzzyCompare(targetTransform.translation(), QVector3D(2.0f, 0.0f, 2.0f)));
            QVERIFY(qFuzzyCompare(targetTransform.rotation(),
                                  QQuaternion::fromEulerAngles(QVector3D(0.0f, 0.0f, 90.0f))));

            // WHEN
            keyframeAnimation.setStartMode(Qt3DAnimation::QKeyframeAnimation::Repeat);
            keyframeAnimation.setEndMode(Qt3DAnimation::QKeyframeAnimation::Repeat);
            keyframeAnimation.setPosition(-1.0f);

            // THEN
            QVERIFY(qFuzzyCompare(targetTransform.scale3D(), QVector3D(2.0f, 2.0f, 2.0f)));
            QVERIFY(qFuzzyCompare(targetTransform.translation(), QVector3D(1.0f, 1.0f, 0.0f)));
            QVERIFY(qFuzzyCompare(targetTransform.rotation(),
                                  QQuaternion::fromEulerAngles(QVector3D(0.0f, 45.0f, 0.0f))));

            // WHEN
            keyframeAnimation.setStartMode(Qt3DAnimation::QKeyframeAnimation::Repeat);
            keyframeAnimation.setEndMode(Qt3DAnimation::QKeyframeAnimation::Repeat);
            keyframeAnimation.setPosition(4.0f);

            // THEN
            QVERIFY(qFuzzyCompare(targetTransform.scale3D(), QVector3D(1.0f, 1.0f, 1.0f)));
            QVERIFY(qFuzzyCompare(targetTransform.translation(), QVector3D(0.0f, 0.0f, 0.0f)));
            QVERIFY(qFuzzyCompare(targetTransform.rotation(),
                                  QQuaternion::fromEulerAngles(QVector3D(0.0f, 0.0f, 0.0f))));
        }
    }
};

QTEST_APPLESS_MAIN(tst_QKeyframeAnimation)

#include "tst_qkeyframeanimation.moc"
