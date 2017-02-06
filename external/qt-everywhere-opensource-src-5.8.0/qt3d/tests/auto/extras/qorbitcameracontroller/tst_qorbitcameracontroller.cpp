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
#include <Qt3DExtras/qorbitcameracontroller.h>
#include <Qt3DRender/qcamera.h>
#include <QObject>
#include <QSignalSpy>

class tst_QOrbitCameraController : public QObject
{
    Q_OBJECT

private Q_SLOTS:

    void checkDefaultConstruction()
    {
        // GIVEN
        Qt3DExtras::QOrbitCameraController orbitCameraController;

        // THEN
        QVERIFY(orbitCameraController.camera() == nullptr);
        QCOMPARE(orbitCameraController.linearSpeed(), 10.0f);
        QCOMPARE(orbitCameraController.lookSpeed(), 180.0f);
        QCOMPARE(orbitCameraController.zoomInLimit(), 2.0f);
    }

    void checkPropertyChanges()
    {
        // GIVEN
        Qt3DExtras::QOrbitCameraController orbitCameraController;

        {
            // WHEN
            QSignalSpy spy(&orbitCameraController, SIGNAL(cameraChanged()));
            Qt3DRender::QCamera *newValue = new Qt3DRender::QCamera(&orbitCameraController);
            orbitCameraController.setCamera(newValue);

            // THEN
            QCOMPARE(orbitCameraController.camera(), newValue);
            QCOMPARE(spy.count(), 1);

            // WHEN
            spy.clear();
            orbitCameraController.setCamera(newValue);

            // THEN
            QCOMPARE(orbitCameraController.camera(), newValue);
            QCOMPARE(spy.count(), 0);

            // WHEN
            spy.clear();
            // Check node bookeeping
            delete newValue;

            // THEN
            QCOMPARE(spy.count(), 1);
            QVERIFY(orbitCameraController.camera() == nullptr);
        }
        {
            // WHEN
            QSignalSpy spy(&orbitCameraController, SIGNAL(linearSpeedChanged()));
            const float newValue = 0.0f;
            orbitCameraController.setLinearSpeed(newValue);

            // THEN
            QCOMPARE(orbitCameraController.linearSpeed(), newValue);
            QCOMPARE(spy.count(), 1);

            // WHEN
            spy.clear();
            orbitCameraController.setLinearSpeed(newValue);

            // THEN
            QCOMPARE(orbitCameraController.linearSpeed(), newValue);
            QCOMPARE(spy.count(), 0);

        }
        {
            // WHEN
            QSignalSpy spy(&orbitCameraController, SIGNAL(lookSpeedChanged()));
            const float newValue = 100.0f;
            orbitCameraController.setLookSpeed(newValue);

            // THEN
            QCOMPARE(orbitCameraController.lookSpeed(), newValue);
            QCOMPARE(spy.count(), 1);

            // WHEN
            spy.clear();
            orbitCameraController.setLookSpeed(newValue);

            // THEN
            QCOMPARE(orbitCameraController.lookSpeed(), newValue);
            QCOMPARE(spy.count(), 0);

        }
        {
            // WHEN
            QSignalSpy spy(&orbitCameraController, SIGNAL(zoomInLimitChanged()));
            const float newValue = 1.0f;
            orbitCameraController.setZoomInLimit(newValue);

            // THEN
            QCOMPARE(orbitCameraController.zoomInLimit(), newValue);
            QCOMPARE(spy.count(), 1);

            // WHEN
            spy.clear();
            orbitCameraController.setZoomInLimit(newValue);

            // THEN
            QCOMPARE(orbitCameraController.zoomInLimit(), newValue);
            QCOMPARE(spy.count(), 0);

        }
    }

};

QTEST_APPLESS_MAIN(tst_QOrbitCameraController)

#include "tst_qorbitcameracontroller.moc"
