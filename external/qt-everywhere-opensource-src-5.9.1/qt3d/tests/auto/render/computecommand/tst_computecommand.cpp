/****************************************************************************
**
** Copyright (C) 2016  Klaralvdalens Datakonsult AB (KDAB).
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
#include <Qt3DRender/qcomputecommand.h>
#include <Qt3DRender/private/qcomputecommand_p.h>
#include <Qt3DRender/private/computecommand_p.h>
#include <Qt3DCore/qpropertyupdatedchange.h>
#include "qbackendnodetester.h"
#include "testrenderer.h"

class tst_ComputeCommand : public Qt3DCore::QBackendNodeTester
{
    Q_OBJECT

private Q_SLOTS:

    void checkInitialState()
    {
        // GIVEN
        Qt3DRender::Render::ComputeCommand backendComputeCommand;

        // THEN
        QCOMPARE(backendComputeCommand.isEnabled(), false);
        QVERIFY(backendComputeCommand.peerId().isNull());
        QCOMPARE(backendComputeCommand.x(), 1);
        QCOMPARE(backendComputeCommand.y(), 1);
        QCOMPARE(backendComputeCommand.z(), 1);
    }

    void checkCleanupState()
    {
        // GIVEN
        Qt3DRender::Render::ComputeCommand backendComputeCommand;

        // WHEN
        backendComputeCommand.setEnabled(true);

        backendComputeCommand.cleanup();

        // THEN
        QCOMPARE(backendComputeCommand.isEnabled(), false);
    }

    void checkInitializeFromPeer()
    {
        // GIVEN
        Qt3DRender::QComputeCommand computeCommand;
        computeCommand.setWorkGroupX(256);
        computeCommand.setWorkGroupY(512);
        computeCommand.setWorkGroupZ(128);

        {
            // WHEN
            Qt3DRender::Render::ComputeCommand backendComputeCommand;
            simulateInitialization(&computeCommand, &backendComputeCommand);

            // THEN
            QCOMPARE(backendComputeCommand.isEnabled(), true);
            QCOMPARE(backendComputeCommand.peerId(), computeCommand.id());
            QCOMPARE(backendComputeCommand.x(), computeCommand.workGroupX());
            QCOMPARE(backendComputeCommand.y(), computeCommand.workGroupY());
            QCOMPARE(backendComputeCommand.z(), computeCommand.workGroupZ());
        }
        {
            // WHEN
            Qt3DRender::Render::ComputeCommand backendComputeCommand;
            computeCommand.setEnabled(false);
            simulateInitialization(&computeCommand, &backendComputeCommand);

            // THEN
            QCOMPARE(backendComputeCommand.peerId(), computeCommand.id());
            QCOMPARE(backendComputeCommand.isEnabled(), false);
        }
    }

    void checkSceneChangeEvents()
    {
        // GIVEN
        Qt3DRender::Render::ComputeCommand backendComputeCommand;
        TestRenderer renderer;
        backendComputeCommand.setRenderer(&renderer);

        {
            // WHEN
            const bool newValue = false;
            const auto change = Qt3DCore::QPropertyUpdatedChangePtr::create(Qt3DCore::QNodeId());
            change->setPropertyName("enabled");
            change->setValue(newValue);
            backendComputeCommand.sceneChangeEvent(change);

            // THEN
            QCOMPARE(backendComputeCommand.isEnabled(), newValue);
        }
        {
            // WHEN
            const int newValue = 128;
            const auto change = Qt3DCore::QPropertyUpdatedChangePtr::create(Qt3DCore::QNodeId());
            change->setPropertyName("workGroupX");
            change->setValue(newValue);
            backendComputeCommand.sceneChangeEvent(change);

            // THEN
            QCOMPARE(backendComputeCommand.x(), newValue);
        }
        {
            // WHEN
            const int newValue = 64;
            const auto change = Qt3DCore::QPropertyUpdatedChangePtr::create(Qt3DCore::QNodeId());
            change->setPropertyName("workGroupY");
            change->setValue(newValue);
            backendComputeCommand.sceneChangeEvent(change);

            // THEN
            QCOMPARE(backendComputeCommand.y(), newValue);
        }
        {
            // WHEN
            const int newValue = 32;
            const auto change = Qt3DCore::QPropertyUpdatedChangePtr::create(Qt3DCore::QNodeId());
            change->setPropertyName("workGroupZ");
            change->setValue(newValue);
            backendComputeCommand.sceneChangeEvent(change);

            // THEN
            QCOMPARE(backendComputeCommand.z(), newValue);
        }
    }

};

QTEST_MAIN(tst_ComputeCommand)

#include "tst_computecommand.moc"
