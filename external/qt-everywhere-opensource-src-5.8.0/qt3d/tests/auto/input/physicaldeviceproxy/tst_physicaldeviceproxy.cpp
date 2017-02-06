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
#include <Qt3DInput/private/qabstractphysicaldeviceproxy_p.h>
#include <Qt3DInput/private/qabstractphysicaldeviceproxy_p_p.h>
#include <Qt3DInput/private/physicaldeviceproxy_p.h>
#include <Qt3DInput/private/inputmanagers_p.h>
#include <Qt3DCore/qpropertyupdatedchange.h>
#include <Qt3DCore/private/qbackendnode_p.h>
#include "qbackendnodetester.h"
#include "testdeviceproxy.h"
#include "testpostmanarbiter.h"

class tst_PhysicalDeviceProxy : public Qt3DCore::QBackendNodeTester
{
    Q_OBJECT

private Q_SLOTS:

    void checkInitialState()
    {
        // GIVEN
        Qt3DInput::Input::PhysicalDeviceProxy backendPhysicalDeviceProxy;

        // THEN
        QCOMPARE(backendPhysicalDeviceProxy.isEnabled(), false);
        QVERIFY(backendPhysicalDeviceProxy.peerId().isNull());
        QCOMPARE(backendPhysicalDeviceProxy.deviceName(), QString());
        QVERIFY(backendPhysicalDeviceProxy.manager() == nullptr);
        QVERIFY(backendPhysicalDeviceProxy.physicalDeviceId().isNull());
    }

    void checkInitializeFromPeer()
    {
        // GIVEN
        TestProxy PhysicalDeviceProxy;
        Qt3DInput::Input::PhysicalDeviceProxyManager manager;

        {
            // WHEN
            Qt3DInput::Input::PhysicalDeviceProxy backendPhysicalDeviceProxy;
            backendPhysicalDeviceProxy.setManager(&manager);
            simulateInitialization(&PhysicalDeviceProxy, &backendPhysicalDeviceProxy);

            // THEN
            QCOMPARE(backendPhysicalDeviceProxy.isEnabled(), true);
            QCOMPARE(backendPhysicalDeviceProxy.peerId(), PhysicalDeviceProxy.id());
            QCOMPARE(backendPhysicalDeviceProxy.deviceName(), QStringLiteral("TestProxy"));
            QVERIFY(backendPhysicalDeviceProxy.manager() == &manager);
            QVERIFY(backendPhysicalDeviceProxy.physicalDeviceId().isNull());
        }
        {
            // WHEN
            Qt3DInput::Input::PhysicalDeviceProxy backendPhysicalDeviceProxy;
            backendPhysicalDeviceProxy.setManager(&manager);
            PhysicalDeviceProxy.setEnabled(false);
            simulateInitialization(&PhysicalDeviceProxy, &backendPhysicalDeviceProxy);

            // THEN
            QCOMPARE(backendPhysicalDeviceProxy.peerId(), PhysicalDeviceProxy.id());
            QCOMPARE(backendPhysicalDeviceProxy.isEnabled(), false);
        }
    }

    void checkLoadingRequested()
    {
        // GIVEN
        Qt3DInput::Input::PhysicalDeviceProxyManager manager;
        Qt3DInput::Input::PhysicalDeviceProxy backendPhysicalDeviceProxy;
        TestProxy deviceProxy;

        // WHEN
        backendPhysicalDeviceProxy.setManager(&manager);
        simulateInitialization(&deviceProxy, &backendPhysicalDeviceProxy);

        // THEN
        QCOMPARE(backendPhysicalDeviceProxy.deviceName(), QStringLiteral("TestProxy"));
        const QVector<Qt3DCore::QNodeId> pendingWrappers = manager.takePendingProxiesToLoad();
        QCOMPARE(pendingWrappers.size(), 1);
        QCOMPARE(pendingWrappers.first(), deviceProxy.id());
    }

    void checkDeviceLoadedNotification()
    {
        // GIVEN
        Qt3DInput::Input::PhysicalDeviceProxy backendPhysicalDeviceProxy;
        TestPhysicalDevice physicalDevice;
        TestArbiter arbiter;

        // WHEN
        Qt3DCore::QBackendNodePrivate::get(&backendPhysicalDeviceProxy)->setArbiter(&arbiter);

        backendPhysicalDeviceProxy.setDevice(&physicalDevice);

        // THEN
        QCOMPARE(arbiter.events.count(), 1);
        Qt3DCore::QPropertyUpdatedChangePtr change = arbiter.events.first().staticCast<Qt3DCore::QPropertyUpdatedChange>();
        QCOMPARE(change->propertyName(), "device");
        QCOMPARE(change->value().value<Qt3DInput::QAbstractPhysicalDevice *>(), &physicalDevice);
        QCOMPARE(change->subjectId(), backendPhysicalDeviceProxy.peerId());
        QCOMPARE(backendPhysicalDeviceProxy.physicalDeviceId(), physicalDevice.id());
    }

    void checkCleanupState()
    {
        // GIVEN
        Qt3DInput::Input::PhysicalDeviceProxy backendPhysicalDeviceProxy;
        Qt3DInput::Input::PhysicalDeviceProxyManager manager;
        TestProxy deviceProxy;

        // WHEN
        backendPhysicalDeviceProxy.setManager(&manager);
        simulateInitialization(&deviceProxy, &backendPhysicalDeviceProxy);

        backendPhysicalDeviceProxy.cleanup();

        // THEN
        QCOMPARE(backendPhysicalDeviceProxy.isEnabled(), false);
        QCOMPARE(backendPhysicalDeviceProxy.deviceName(), QString());
        QVERIFY(backendPhysicalDeviceProxy.manager() == nullptr);
        QVERIFY(backendPhysicalDeviceProxy.physicalDeviceId().isNull());
    }

};

QTEST_MAIN(tst_PhysicalDeviceProxy)

#include "tst_physicaldeviceproxy.moc"
