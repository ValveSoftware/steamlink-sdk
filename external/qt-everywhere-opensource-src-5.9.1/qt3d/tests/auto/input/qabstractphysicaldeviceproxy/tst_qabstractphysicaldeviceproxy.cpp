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

#include <Qt3DCore/qpropertyupdatedchange.h>
#include <Qt3DCore/private/qnodecreatedchangegenerator_p.h>
#include <Qt3DCore/qnodecreatedchange.h>
#include "testdeviceproxy.h"

class tst_QAbstractPhysicalDeviceProxy : public QObject
{
    Q_OBJECT

private Q_SLOTS:

    void checkDefaultConstruction()
    {
        // GIVEN
        TestProxy abstractPhysicalDeviceProxy;

        // THEN
        QCOMPARE(abstractPhysicalDeviceProxy.deviceName(), QLatin1String("TestProxy"));
        QCOMPARE(abstractPhysicalDeviceProxy.status(), Qt3DInput::QAbstractPhysicalDeviceProxy::NotFound);
        QCOMPARE(abstractPhysicalDeviceProxy.axisCount(), 0);
        QCOMPARE(abstractPhysicalDeviceProxy.buttonCount(), 0);
        QCOMPARE(abstractPhysicalDeviceProxy.axisNames(), QStringList());
        QCOMPARE(abstractPhysicalDeviceProxy.buttonNames(), QStringList());
        QVERIFY(abstractPhysicalDeviceProxy.device() == nullptr);
    }


    void checkDeviceLoading()
    {
        // GIVEN
        TestProxy abstractPhysicalDeviceProxy;

        // WHEN
        TestPhysicalDevice *device = new TestPhysicalDevice();
        auto change = Qt3DCore::QPropertyUpdatedChangePtr::create(Qt3DCore::QNodeId());
        change->setPropertyName("device");
        change->setValue(QVariant::fromValue(device));

        abstractPhysicalDeviceProxy.simulateSceneChangeEvent(change);

        // THEN
        QCOMPARE(abstractPhysicalDeviceProxy.deviceName(), QLatin1String("TestProxy"));
        QCOMPARE(abstractPhysicalDeviceProxy.status(), Qt3DInput::QAbstractPhysicalDeviceProxy::Ready);
        QCOMPARE(abstractPhysicalDeviceProxy.axisCount(), device->axisCount());
        QCOMPARE(abstractPhysicalDeviceProxy.buttonCount(), device->buttonCount());
        QCOMPARE(abstractPhysicalDeviceProxy.axisNames(), device->axisNames());
        QCOMPARE(abstractPhysicalDeviceProxy.buttonNames(), device->buttonNames());
        QVERIFY(abstractPhysicalDeviceProxy.device() == device);
    }

    void checkDeviceBookkeeping()
    {
        // GIVEN
        TestProxy *abstractPhysicalDeviceProxy = new TestProxy();

        // WHEN
        TestPhysicalDevice *device = new TestPhysicalDevice();
        auto change = Qt3DCore::QPropertyUpdatedChangePtr::create(Qt3DCore::QNodeId());
        change->setPropertyName("device");
        change->setValue(QVariant::fromValue(device));

        abstractPhysicalDeviceProxy->simulateSceneChangeEvent(change);

        // THEN
        QVERIFY(abstractPhysicalDeviceProxy->device() == device);

        // WHEN
        delete device;

        // THEN -> should not crash
        QVERIFY(abstractPhysicalDeviceProxy->device() == nullptr);
    }

    void checkCreationData()
    {
        // GIVEN
        TestProxy abstractPhysicalDeviceProxy;


        // WHEN
        QVector<Qt3DCore::QNodeCreatedChangeBasePtr> creationChanges;

        {
            Qt3DCore::QNodeCreatedChangeGenerator creationChangeGenerator(&abstractPhysicalDeviceProxy);
            creationChanges = creationChangeGenerator.creationChanges();
        }

        // THEN
        {
            QCOMPARE(creationChanges.size(), 1);

            const auto creationChangeData = qSharedPointerCast<Qt3DCore::QNodeCreatedChange<Qt3DInput::QAbstractPhysicalDeviceProxyData>>(creationChanges.first());
            const Qt3DInput::QAbstractPhysicalDeviceProxyData cloneData = creationChangeData->data;

            QCOMPARE(abstractPhysicalDeviceProxy.deviceName(), cloneData.deviceName);
            QCOMPARE(abstractPhysicalDeviceProxy.id(), creationChangeData->subjectId());
            QCOMPARE(abstractPhysicalDeviceProxy.isEnabled(), true);
            QCOMPARE(abstractPhysicalDeviceProxy.isEnabled(), creationChangeData->isNodeEnabled());
            QCOMPARE(abstractPhysicalDeviceProxy.metaObject(), creationChangeData->metaObject());
        }

        // WHEN
        abstractPhysicalDeviceProxy.setEnabled(false);

        {
            Qt3DCore::QNodeCreatedChangeGenerator creationChangeGenerator(&abstractPhysicalDeviceProxy);
            creationChanges = creationChangeGenerator.creationChanges();
        }

        // THEN
        {
            QCOMPARE(creationChanges.size(), 1);

            const auto creationChangeData = qSharedPointerCast<Qt3DCore::QNodeCreatedChange<Qt3DInput::QAbstractPhysicalDeviceProxyData>>(creationChanges.first());
            const Qt3DInput::QAbstractPhysicalDeviceProxyData cloneData = creationChangeData->data;

            QCOMPARE(abstractPhysicalDeviceProxy.deviceName(), cloneData.deviceName);
            QCOMPARE(abstractPhysicalDeviceProxy.id(), creationChangeData->subjectId());
            QCOMPARE(abstractPhysicalDeviceProxy.isEnabled(), false);
            QCOMPARE(abstractPhysicalDeviceProxy.isEnabled(), creationChangeData->isNodeEnabled());
            QCOMPARE(abstractPhysicalDeviceProxy.metaObject(), creationChangeData->metaObject());
        }
    }

};

QTEST_MAIN(tst_QAbstractPhysicalDeviceProxy)

#include "tst_qabstractphysicaldeviceproxy.moc"
