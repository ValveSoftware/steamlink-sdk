/****************************************************************************
**
** Copyright (C) 2017 The Qt Company Ltd.
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
#include <Qt3DAnimation/qvertexblendanimation.h>
#include <qobject.h>
#include <qsignalspy.h>

class tst_QVertexBlendAnimation : public QObject
{
    Q_OBJECT

private Q_SLOTS:

    void checkDefaultConstruction()
    {
        // GIVEN
        Qt3DAnimation::QVertexBlendAnimation vertexBlendAnimation;

        // THEN
        QCOMPARE(vertexBlendAnimation.interpolator(), 0.0f);
        QCOMPARE(vertexBlendAnimation.target(), nullptr);
        QCOMPARE(vertexBlendAnimation.targetName(), QString());
    }

    void checkPropertyChanges()
    {
        // GIVEN
        Qt3DAnimation::QVertexBlendAnimation vertexBlendAnimation;

        {
            // WHEN
            QScopedPointer<Qt3DRender::QGeometryRenderer> gm(new Qt3DRender::QGeometryRenderer);
            QSignalSpy spy(&vertexBlendAnimation,
                           SIGNAL(targetChanged(Qt3DRender::QGeometryRenderer *)));
            Qt3DRender::QGeometryRenderer *newValue = gm.data();
            vertexBlendAnimation.setTarget(newValue);

            // THEN
            QCOMPARE(vertexBlendAnimation.target(), newValue);
            QCOMPARE(spy.count(), 1);

            // WHEN
            spy.clear();
            vertexBlendAnimation.setTarget(newValue);

            // THEN
            QCOMPARE(vertexBlendAnimation.target(), newValue);
            QCOMPARE(spy.count(), 0);

        }
        {
            // WHEN
            QSignalSpy spy(&vertexBlendAnimation, SIGNAL(targetNameChanged(QString)));
            const QString newValue = QString("target");
            vertexBlendAnimation.setTargetName(newValue);

            // THEN
            QCOMPARE(vertexBlendAnimation.targetName(), newValue);
            QCOMPARE(spy.count(), 1);

            // WHEN
            spy.clear();
            vertexBlendAnimation.setTargetName(newValue);

            // THEN
            QCOMPARE(vertexBlendAnimation.targetName(), newValue);
            QCOMPARE(spy.count(), 0);

        }
    }

};

QTEST_APPLESS_MAIN(tst_QVertexBlendAnimation)

#include "tst_qvertexblendanimation.moc"
