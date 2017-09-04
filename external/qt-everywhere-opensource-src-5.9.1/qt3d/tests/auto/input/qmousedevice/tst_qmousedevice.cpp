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
#include <Qt3DInput/qmousedevice.h>
#include <Qt3DInput/qmouseevent.h>
#include <Qt3DInput/private/qmousedevice_p.h>
#include <QObject>
#include <QSignalSpy>
#include <Qt3DCore/qpropertyupdatedchange.h>
#include <Qt3DCore/private/qnodecreatedchangegenerator_p.h>
#include <Qt3DCore/qnodecreatedchange.h>
#include "testpostmanarbiter.h"

class tst_QMouseDevice : public QObject
{
    Q_OBJECT

private Q_SLOTS:

    void checkDefaultConstruction()
    {
        // GIVEN
        Qt3DInput::QMouseDevice mouseDevice;

        // THEN
        QCOMPARE(mouseDevice.sensitivity(), 0.1f);
        QCOMPARE(mouseDevice.axisCount(), 4);
        QCOMPARE(mouseDevice.buttonCount(), 3);
        QCOMPARE(mouseDevice.axisNames(), QStringList()
                 << QStringLiteral("X")
                 << QStringLiteral("Y")
                 << QStringLiteral("WheelX")
                 << QStringLiteral("WheelY"));
        QCOMPARE(mouseDevice.buttonNames(), QStringList()
                 << QStringLiteral("Left")
                 << QStringLiteral("Right")
                 << QStringLiteral("Center"));

        QVERIFY(mouseDevice.axisIdentifier(QStringLiteral("X")) == Qt3DInput::QMouseDevice::X);
        QVERIFY(mouseDevice.axisIdentifier(QStringLiteral("Y")) == Qt3DInput::QMouseDevice::Y);
        QVERIFY(mouseDevice.axisIdentifier(QStringLiteral("WheelX")) == Qt3DInput::QMouseDevice::WheelX);
        QVERIFY(mouseDevice.axisIdentifier(QStringLiteral("WheelY")) == Qt3DInput::QMouseDevice::WheelY);

        QVERIFY(mouseDevice.buttonIdentifier(QStringLiteral("Left")) == Qt3DInput::QMouseEvent::LeftButton);
        QVERIFY(mouseDevice.buttonIdentifier(QStringLiteral("Right")) == Qt3DInput::QMouseEvent::RightButton);
        QVERIFY(mouseDevice.buttonIdentifier(QStringLiteral("Center")) == Qt3DInput::QMouseEvent::MiddleButton);
    }

    void checkPropertyChanges()
    {
        // GIVEN
        Qt3DInput::QMouseDevice mouseDevice;

        {
            // WHEN
            QSignalSpy spy(&mouseDevice, SIGNAL(sensitivityChanged(float)));
            const float newValue = 0.5f;
            mouseDevice.setSensitivity(newValue);

            // THEN
            QVERIFY(spy.isValid());
            QCOMPARE(mouseDevice.sensitivity(), newValue);
            QCOMPARE(spy.count(), 1);

            // WHEN
            spy.clear();
            mouseDevice.setSensitivity(newValue);

            // THEN
            QCOMPARE(mouseDevice.sensitivity(), newValue);
            QCOMPARE(spy.count(), 0);
        }
    }

    void checkCreationData()
    {
        // GIVEN
        Qt3DInput::QMouseDevice mouseDevice;

        mouseDevice.setSensitivity(0.8f);

        // WHEN
        QVector<Qt3DCore::QNodeCreatedChangeBasePtr> creationChanges;

        {
            Qt3DCore::QNodeCreatedChangeGenerator creationChangeGenerator(&mouseDevice);
            creationChanges = creationChangeGenerator.creationChanges();
        }

        // THEN
        {
            QCOMPARE(creationChanges.size(), 1);

            const auto creationChangeData = qSharedPointerCast<Qt3DCore::QNodeCreatedChange<Qt3DInput::QMouseDeviceData>>(creationChanges.first());
            const Qt3DInput::QMouseDeviceData cloneData = creationChangeData->data;

            QCOMPARE(mouseDevice.sensitivity(), cloneData.sensitivity);
            QCOMPARE(mouseDevice.id(), creationChangeData->subjectId());
            QCOMPARE(mouseDevice.isEnabled(), true);
            QCOMPARE(mouseDevice.isEnabled(), creationChangeData->isNodeEnabled());
            QCOMPARE(mouseDevice.metaObject(), creationChangeData->metaObject());
        }

        // WHEN
        mouseDevice.setEnabled(false);

        {
            Qt3DCore::QNodeCreatedChangeGenerator creationChangeGenerator(&mouseDevice);
            creationChanges = creationChangeGenerator.creationChanges();
        }

        // THEN
        {
            QCOMPARE(creationChanges.size(), 1);

            const auto creationChangeData = qSharedPointerCast<Qt3DCore::QNodeCreatedChange<Qt3DInput::QMouseDeviceData>>(creationChanges.first());
            const Qt3DInput::QMouseDeviceData cloneData = creationChangeData->data;

            QCOMPARE(mouseDevice.sensitivity(), cloneData.sensitivity);
            QCOMPARE(mouseDevice.id(), creationChangeData->subjectId());
            QCOMPARE(mouseDevice.isEnabled(), false);
            QCOMPARE(mouseDevice.isEnabled(), creationChangeData->isNodeEnabled());
            QCOMPARE(mouseDevice.metaObject(), creationChangeData->metaObject());
        }
    }

    void checkSensitivityUpdate()
    {
        // GIVEN
        TestArbiter arbiter;
        Qt3DInput::QMouseDevice mouseDevice;
        arbiter.setArbiterOnNode(&mouseDevice);

        {
            // WHEN
            mouseDevice.setSensitivity(0.7f);
            QCoreApplication::processEvents();

            // THEN
            QCOMPARE(arbiter.events.size(), 1);
            auto change = arbiter.events.first().staticCast<Qt3DCore::QPropertyUpdatedChange>();
            QCOMPARE(change->propertyName(), "sensitivity");
            QCOMPARE(change->value().value<float>(), mouseDevice.sensitivity());
            QCOMPARE(change->type(), Qt3DCore::PropertyUpdated);

            arbiter.events.clear();
        }

        {
            // WHEN
            mouseDevice.setSensitivity(0.7f);
            QCoreApplication::processEvents();

            // THEN
            QCOMPARE(arbiter.events.size(), 0);
        }

    }

};

QTEST_MAIN(tst_QMouseDevice)

#include "tst_qmousedevice.moc"
