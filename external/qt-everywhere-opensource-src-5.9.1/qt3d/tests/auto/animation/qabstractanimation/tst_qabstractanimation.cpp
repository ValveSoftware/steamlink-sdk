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
#include <Qt3DAnimation/qabstractanimation.h>
#include <Qt3DCore/qnode.h>
#include <qobject.h>
#include <qsignalspy.h>

#include <private/qabstractanimation_p.h>

class TestAnimation : public Qt3DAnimation::QAbstractAnimation
{
public:
    explicit TestAnimation(Qt3DCore::QNode *parent = nullptr)
        : Qt3DAnimation::QAbstractAnimation(
              *new Qt3DAnimation::QAbstractAnimationPrivate(
                  Qt3DAnimation::QAbstractAnimation::KeyframeAnimation), parent)
    {

    }
};

class tst_QAbstractAnimation : public QObject
{
    Q_OBJECT

private Q_SLOTS:

    void checkDefaultConstruction()
    {
        // GIVEN
        TestAnimation abstractAnimation;

        // THEN
        QCOMPARE(abstractAnimation.animationName(), QString());
        QCOMPARE(abstractAnimation.animationType(),
                 Qt3DAnimation::QAbstractAnimation::KeyframeAnimation);
        QCOMPARE(abstractAnimation.position(), 0.0f);
        QCOMPARE(abstractAnimation.duration(), 0.0f);
    }

    void checkPropertyChanges()
    {
        // GIVEN
        TestAnimation abstractAnimation;

        {
            // WHEN
            QSignalSpy spy(&abstractAnimation, SIGNAL(animationNameChanged(QString)));
            const QString newValue = QString("test");
            abstractAnimation.setAnimationName(newValue);

            // THEN
            QCOMPARE(abstractAnimation.animationName(), newValue);
            QCOMPARE(spy.count(), 1);

            // WHEN
            spy.clear();
            abstractAnimation.setAnimationName(newValue);

            // THEN
            QCOMPARE(abstractAnimation.animationName(), newValue);
            QCOMPARE(spy.count(), 0);

        }
        {
            // WHEN
            QSignalSpy spy(&abstractAnimation, SIGNAL(positionChanged(float)));
            const float newValue = 1.0f;
            abstractAnimation.setPosition(newValue);

            // THEN
            QCOMPARE(abstractAnimation.position(), newValue);
            QCOMPARE(spy.count(), 1);

            // WHEN
            spy.clear();
            abstractAnimation.setPosition(newValue);

            // THEN
            QCOMPARE(abstractAnimation.position(), newValue);
            QCOMPARE(spy.count(), 0);

        }
    }

};

QTEST_APPLESS_MAIN(tst_QAbstractAnimation)

#include "tst_qabstractanimation.moc"
