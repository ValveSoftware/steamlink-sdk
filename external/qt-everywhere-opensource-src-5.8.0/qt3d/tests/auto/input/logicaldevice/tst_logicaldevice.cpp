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
#include <Qt3DInput/qlogicaldevice.h>
#include <Qt3DInput/qaction.h>
#include <Qt3DInput/qaxis.h>
#include <Qt3DInput/private/qlogicaldevice_p.h>
#include <Qt3DInput/private/logicaldevice_p.h>
#include <Qt3DCore/qpropertyupdatedchange.h>
#include <Qt3DCore/qpropertynodeaddedchange.h>
#include <Qt3DCore/qpropertynoderemovedchange.h>
#include "qbackendnodetester.h"

class tst_LogicalDevice : public Qt3DCore::QBackendNodeTester
{
    Q_OBJECT

private Q_SLOTS:

    void checkInitialState()
    {
        // GIVEN
        Qt3DInput::Input::LogicalDevice backendLogicalDevice;

        // THEN
        QVERIFY(backendLogicalDevice.peerId().isNull());
        QCOMPARE(backendLogicalDevice.isEnabled(), false);
        QVERIFY(backendLogicalDevice.axes().empty());
        QVERIFY(backendLogicalDevice.actions().empty());
    }

    void checkCleanupState()
    {
        // GIVEN
        Qt3DInput::Input::LogicalDevice backendLogicalDevice;

        // WHEN
        backendLogicalDevice.setEnabled(true);

        // WHEN
        {
            Qt3DInput::QAxis newValue;
            const auto change = Qt3DCore::QPropertyNodeAddedChangePtr::create(Qt3DCore::QNodeId(), &newValue);
            change->setPropertyName("axis");
            backendLogicalDevice.sceneChangeEvent(change);
        }
        {
            Qt3DInput::QAction newValue;
            const auto change = Qt3DCore::QPropertyNodeAddedChangePtr::create(Qt3DCore::QNodeId(), &newValue);
            change->setPropertyName("action");
            backendLogicalDevice.sceneChangeEvent(change);
        }

        // THEN
        QCOMPARE(backendLogicalDevice.axes().size(), 1);
        QCOMPARE(backendLogicalDevice.actions().size(), 1);

        // WHEN
        backendLogicalDevice.cleanup();

        // THEN
        QCOMPARE(backendLogicalDevice.isEnabled(), false);
        QCOMPARE(backendLogicalDevice.axes().size(), 0);
        QCOMPARE(backendLogicalDevice.actions().size(), 0);
    }

    void checkInitializeFromPeer()
    {
        // GIVEN
        Qt3DInput::QLogicalDevice logicalDevice;

        Qt3DInput::QAction *action = new Qt3DInput::QAction(&logicalDevice);
        Qt3DInput::QAxis *axis = new Qt3DInput::QAxis(&logicalDevice);
        logicalDevice.addAction(action);
        logicalDevice.addAxis(axis);

        {
            // WHEN
            Qt3DInput::Input::LogicalDevice backendLogicalDevice;
            simulateInitialization(&logicalDevice, &backendLogicalDevice);

            // THEN
            QCOMPARE(backendLogicalDevice.isEnabled(), true);
            QCOMPARE(backendLogicalDevice.axes().size(), 1);
            QCOMPARE(backendLogicalDevice.axes().first(), axis->id());
            QCOMPARE(backendLogicalDevice.actions().size(), 1);
            QCOMPARE(backendLogicalDevice.actions().first(), action->id());
            QCOMPARE(backendLogicalDevice.peerId(), logicalDevice.id());
        }
        {
            // WHEN
            Qt3DInput::Input::LogicalDevice backendLogicalDevice;
            logicalDevice.setEnabled(false);
            simulateInitialization(&logicalDevice, &backendLogicalDevice);

            // THEN
            QCOMPARE(backendLogicalDevice.isEnabled(), false);
        }
    }

    void checkSceneChangeEvents()
    {
        // GIVEN
        Qt3DInput::Input::LogicalDevice backendLogicalDevice;

        {
            // WHEN
            const bool newValue = false;
            const auto change = Qt3DCore::QPropertyUpdatedChangePtr::create(Qt3DCore::QNodeId());
            change->setPropertyName("enabled");
            change->setValue(newValue);
            backendLogicalDevice.sceneChangeEvent(change);

            // THEN
            QCOMPARE(backendLogicalDevice.isEnabled(), newValue);
        }
        {
            // WHEN
            Qt3DInput::QAxis newValue;
            const auto change = Qt3DCore::QPropertyNodeAddedChangePtr::create(Qt3DCore::QNodeId(), &newValue);
            change->setPropertyName("axis");
            backendLogicalDevice.sceneChangeEvent(change);

            // THEN
            QCOMPARE(backendLogicalDevice.axes().size(), 1);
            QCOMPARE(backendLogicalDevice.axes().first(), newValue.id());

            // WHEN
            const auto change2 = Qt3DCore::QPropertyNodeRemovedChangePtr::create(Qt3DCore::QNodeId(), &newValue);
            change2->setPropertyName("axis");
            backendLogicalDevice.sceneChangeEvent(change2);

            // THEN
            QCOMPARE(backendLogicalDevice.axes().size(), 0);
        }
        {
            // WHEN
            Qt3DInput::QAction newValue;
            const auto change = Qt3DCore::QPropertyNodeAddedChangePtr::create(Qt3DCore::QNodeId(), &newValue);
            change->setPropertyName("action");
            backendLogicalDevice.sceneChangeEvent(change);

            // THEN
            QCOMPARE(backendLogicalDevice.actions().size(), 1);
            QCOMPARE(backendLogicalDevice.actions().first(), newValue.id());

            // WHEN
            const auto change2 = Qt3DCore::QPropertyNodeRemovedChangePtr::create(Qt3DCore::QNodeId(), &newValue);
            change2->setPropertyName("action");
            backendLogicalDevice.sceneChangeEvent(change2);

            // THEN
            QCOMPARE(backendLogicalDevice.actions().size(), 0);
        }
    }

};

QTEST_APPLESS_MAIN(tst_LogicalDevice)

#include "tst_logicaldevice.moc"
