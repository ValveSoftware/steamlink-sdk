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
#include <Qt3DInput/private/loadproxydevicejob_p.h>
#include <Qt3DInput/private/inputmanagers_p.h>
#include <Qt3DInput/private/inputhandler_p.h>
#include <Qt3DInput/private/physicaldeviceproxy_p.h>
#include <Qt3DInput/private/qinputdeviceintegration_p.h>
#include <Qt3DCore/qpropertyupdatedchange.h>
#include <Qt3DCore/private/qbackendnode_p.h>
#include "qbackendnodetester.h"
#include "testdeviceproxy.h"
#include "testpostmanarbiter.h"

class FakeInputDeviceIntegration : public Qt3DInput::QInputDeviceIntegration
{
public:
    explicit FakeInputDeviceIntegration(const QString &name)
        : Qt3DInput::QInputDeviceIntegration()
        , m_name(name)
    {}

    QVector<Qt3DCore::QAspectJobPtr> jobsToExecute(qint64) Q_DECL_OVERRIDE
    {
        return QVector<Qt3DCore::QAspectJobPtr>();
    }

    Qt3DInput::QAbstractPhysicalDevice *createPhysicalDevice(const QString &name) Q_DECL_OVERRIDE
    {
        if (name == m_name)
            return new TestPhysicalDevice();
        return nullptr;
    }

    Qt3DInput::QAbstractPhysicalDeviceBackendNode *physicalDevice(Qt3DCore::QNodeId) const Q_DECL_OVERRIDE
    {
        return nullptr;
    }

    QVector<Qt3DCore::QNodeId> physicalDevices() const Q_DECL_OVERRIDE
    {
        return QVector<Qt3DCore::QNodeId>();
    }

    QStringList deviceNames() const Q_DECL_OVERRIDE
    {
        return QStringList() << m_name;
    }

private:
    void onInitialize() Q_DECL_OVERRIDE {}
    QString m_name;
};

class tst_LoadProxyDeviceJob : public QObject
{
    Q_OBJECT

private Q_SLOTS:

    void checkInitialState()
    {
        // GIVEN
        Qt3DInput::Input::LoadProxyDeviceJob backendLoadProxyDeviceJob;

        // THEN
        QVERIFY(backendLoadProxyDeviceJob.inputHandler() == nullptr);
        QVERIFY(backendLoadProxyDeviceJob.proxies().empty());
    }

    void checkJobCreatesDeviceWhenValid()
    {
        // GIVEN
        Qt3DInput::Input::InputHandler inputHandler;
        Qt3DInput::Input::PhysicalDeviceProxyManager *manager = inputHandler.physicalDeviceProxyManager();

        FakeInputDeviceIntegration inputIntegration(QStringLiteral("TestProxy"));
        inputHandler.addInputDeviceIntegration(&inputIntegration);

        // THEN
        QCOMPARE(inputHandler.inputDeviceIntegrations().size(), 1);
        QCOMPARE(inputHandler.inputDeviceIntegrations().first(), &inputIntegration);

        // WHEN -> valid device name
        {
            // WHEN
            TestProxy proxy;
            TestArbiter arbiter;
            Qt3DInput::Input::PhysicalDeviceProxy *backendProxy = manager->getOrCreateResource(proxy.id());

            {
                backendProxy->setManager(manager);
                Qt3DCore::QBackendNodeTester backendNodeCreator;
                backendNodeCreator.simulateInitialization(&proxy, backendProxy);
                Qt3DCore::QBackendNodePrivate::get(backendProxy)->setArbiter(&arbiter);
            }

            // THEN
            QCOMPARE(manager->lookupResource(proxy.id()), backendProxy);
            QCOMPARE(backendProxy->deviceName(), QStringLiteral("TestProxy"));

            const QVector<Qt3DCore::QNodeId> pendingProxies = manager->takePendingProxiesToLoad();
            QCOMPARE(pendingProxies.size(), 1);
            QCOMPARE(pendingProxies.first(), backendProxy->peerId());

            // WHEN
            Qt3DInput::Input::LoadProxyDeviceJob job;
            job.setInputHandler(&inputHandler);
            job.setProxiesToLoad(std::move(pendingProxies));

            job.run();

            // THEN -> PhysicalDeviceWrapper::setDevice should have been called
            QCOMPARE(arbiter.events.count(), 1);
            Qt3DCore::QPropertyUpdatedChangePtr change = arbiter.events.first().staticCast<Qt3DCore::QPropertyUpdatedChange>();
            QCOMPARE(change->propertyName(), "device");
            QVERIFY(change->value().value<Qt3DInput::QAbstractPhysicalDevice *>() != nullptr);
            QCOMPARE(change->subjectId(), proxy.id());
        }

        // WHEN -> invalid name
        {
            // WHEN
            TestProxy proxy(QStringLiteral("NonExisting"));
            TestArbiter arbiter;
            Qt3DInput::Input::PhysicalDeviceProxy *backendProxy = manager->getOrCreateResource(proxy.id());

            {
                backendProxy->setManager(manager);
                Qt3DCore::QBackendNodeTester backendNodeCreator;
                backendNodeCreator.simulateInitialization(&proxy, backendProxy);
                Qt3DCore::QBackendNodePrivate::get(backendProxy)->setArbiter(&arbiter);
            }

            // THEN
            QCOMPARE(manager->lookupResource(proxy.id()), backendProxy);
            QCOMPARE(backendProxy->deviceName(), QStringLiteral("NonExisting"));

            const QVector<Qt3DCore::QNodeId> pendingProxies = manager->takePendingProxiesToLoad();
            QCOMPARE(pendingProxies.size(), 1);
            QCOMPARE(pendingProxies.first(), backendProxy->peerId());

            // WHEN
            Qt3DInput::Input::LoadProxyDeviceJob job;
            job.setInputHandler(&inputHandler);
            job.setProxiesToLoad(std::move(pendingProxies));

            job.run();

            // THEN -> PhysicalDeviceWrapper::setDevice should not have been called
            QCOMPARE(arbiter.events.count(), 0);
        }
    }

};

QTEST_MAIN(tst_LoadProxyDeviceJob)

#include "tst_loadproxydevicejob.moc"
