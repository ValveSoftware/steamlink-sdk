/****************************************************************************
**
** Copyright (C) 2016 Klaralvdalens Datakonsult AB (KDAB).
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
#include <Qt3DExtras/qfirstpersoncameracontroller.h>
#include <Qt3DRender/qcamera.h>
#include <QObject>
#include <QSignalSpy>

class tst_QFirstPersonCameraController : public QObject
{
    Q_OBJECT

private Q_SLOTS:

    void checkDefaultConstruction()
    {
        // GIVEN
        Qt3DExtras::QFirstPersonCameraController firstPersonCameraController;

        // THEN
        QVERIFY(firstPersonCameraController.camera() == nullptr);
        QCOMPARE(firstPersonCameraController.linearSpeed(), 10.0f);
        QCOMPARE(firstPersonCameraController.lookSpeed(), 180.0f);
        QCOMPARE(firstPersonCameraController.acceleration(), -1.0f);
        QCOMPARE(firstPersonCameraController.deceleration(), -1.0f);
    }

    void checkPropertyChanges()
    {
        // GIVEN
        Qt3DExtras::QFirstPersonCameraController firstPersonCameraController;

        {
            // WHEN
            QSignalSpy spy(&firstPersonCameraController, SIGNAL(cameraChanged()));
            Qt3DRender::QCamera *newValue = new Qt3DRender::QCamera(&firstPersonCameraController);
            firstPersonCameraController.setCamera(newValue);

            // THEN
            QCOMPARE(firstPersonCameraController.camera(), newValue);
            QCOMPARE(spy.count(), 1);

            // WHEN
            spy.clear();
            firstPersonCameraController.setCamera(newValue);

            // THEN
            QCOMPARE(firstPersonCameraController.camera(), newValue);
            QCOMPARE(spy.count(), 0);

            // WHEN
            spy.clear();
            // Check node bookeeping
            delete newValue;

            // THEN
            QCOMPARE(spy.count(), 1);
            QVERIFY(firstPersonCameraController.camera() == nullptr);
        }
        {
            // WHEN
            QSignalSpy spy(&firstPersonCameraController, SIGNAL(linearSpeedChanged()));
            const float newValue = 300.0f;
            firstPersonCameraController.setLinearSpeed(newValue);

            // THEN
            QCOMPARE(firstPersonCameraController.linearSpeed(), newValue);
            QCOMPARE(spy.count(), 1);

            // WHEN
            spy.clear();
            firstPersonCameraController.setLinearSpeed(newValue);

            // THEN
            QCOMPARE(firstPersonCameraController.linearSpeed(), newValue);
            QCOMPARE(spy.count(), 0);

        }
        {
            // WHEN
            QSignalSpy spy(&firstPersonCameraController, SIGNAL(lookSpeedChanged()));
            const float newValue = 0.001f;
            firstPersonCameraController.setLookSpeed(newValue);

            // THEN
            QCOMPARE(firstPersonCameraController.lookSpeed(), newValue);
            QCOMPARE(spy.count(), 1);

            // WHEN
            spy.clear();
            firstPersonCameraController.setLookSpeed(newValue);

            // THEN
            QCOMPARE(firstPersonCameraController.lookSpeed(), newValue);
            QCOMPARE(spy.count(), 0);

        }
        {
            // WHEN
            QSignalSpy spy(&firstPersonCameraController, SIGNAL(accelerationChanged(float)));
            const float newValue = 0.001f;
            firstPersonCameraController.setAcceleration(newValue);

            // THEN
            QCOMPARE(firstPersonCameraController.acceleration(), newValue);
            QCOMPARE(spy.count(), 1);

            // WHEN
            spy.clear();
            firstPersonCameraController.setAcceleration(newValue);

            // THEN
            QCOMPARE(firstPersonCameraController.acceleration(), newValue);
            QCOMPARE(spy.count(), 0);

        }
        {
            // WHEN
            QSignalSpy spy(&firstPersonCameraController, SIGNAL(decelerationChanged(float)));
            const float newValue = 0.001f;
            firstPersonCameraController.setDeceleration(newValue);

            // THEN
            QCOMPARE(firstPersonCameraController.deceleration(), newValue);
            QCOMPARE(spy.count(), 1);

            // WHEN
            spy.clear();
            firstPersonCameraController.setDeceleration(newValue);

            // THEN
            QCOMPARE(firstPersonCameraController.deceleration(), newValue);
            QCOMPARE(spy.count(), 0);

        }
    }

};

QTEST_APPLESS_MAIN(tst_QFirstPersonCameraController)

#include "tst_qfirstpersoncameracontroller.moc"
