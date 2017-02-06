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
#include <Qt3DRender/qrendertarget.h>
#include <Qt3DRender/qrendertargetoutput.h>
#include <Qt3DRender/private/qrendertarget_p.h>
#include <Qt3DRender/private/rendertarget_p.h>
#include <Qt3DCore/qpropertyupdatedchange.h>
#include <Qt3DCore/qpropertynodeaddedchange.h>
#include <Qt3DCore/qpropertynoderemovedchange.h>
#include "qbackendnodetester.h"
#include "testrenderer.h"

class tst_RenderTarget : public Qt3DCore::QBackendNodeTester
{
    Q_OBJECT

private Q_SLOTS:

    void checkInitialState()
    {
        // GIVEN
        Qt3DRender::Render::RenderTarget backendRenderTarget;

        // THEN
        QCOMPARE(backendRenderTarget.isEnabled(), false);
        QVERIFY(backendRenderTarget.peerId().isNull());
        QVERIFY(backendRenderTarget.renderOutputs().empty());
    }

    void checkCleanupState()
    {
        // GIVEN
        Qt3DRender::Render::RenderTarget backendRenderTarget;

        // WHEN
        backendRenderTarget.setEnabled(true);
        backendRenderTarget.appendRenderOutput(Qt3DCore::QNodeId::createId());
        backendRenderTarget.appendRenderOutput(Qt3DCore::QNodeId::createId());

        backendRenderTarget.cleanup();

        // THEN
        QCOMPARE(backendRenderTarget.isEnabled(), false);
        QCOMPARE(backendRenderTarget.renderOutputs().size(), 0);
    }

    void checkInitializeFromPeer()
    {
        // GIVEN
        Qt3DRender::QRenderTarget renderTarget;
        Qt3DRender::QRenderTargetOutput renderTargetOuput;
        renderTarget.addOutput(&renderTargetOuput);

        {
            // WHEN
            Qt3DRender::Render::RenderTarget backendRenderTarget;
            simulateInitialization(&renderTarget, &backendRenderTarget);

            // THEN
            QCOMPARE(backendRenderTarget.isEnabled(), true);
            QCOMPARE(backendRenderTarget.peerId(), renderTarget.id());
            QCOMPARE(backendRenderTarget.renderOutputs(), QVector<Qt3DCore::QNodeId>() << renderTargetOuput.id());
        }
        {
            // WHEN
            Qt3DRender::Render::RenderTarget backendRenderTarget;
            renderTarget.setEnabled(false);
            simulateInitialization(&renderTarget, &backendRenderTarget);

            // THEN
            QCOMPARE(backendRenderTarget.peerId(), renderTarget.id());
            QCOMPARE(backendRenderTarget.isEnabled(), false);
        }
    }

    void checkAddNoDuplicateOutput()
    {
        // GIVEN
        Qt3DRender::Render::RenderTarget backendRenderTarget;

        // THEN
        QCOMPARE(backendRenderTarget.renderOutputs().size(), 0);

        // WHEN
        Qt3DCore::QNodeId renderTargetOutputId = Qt3DCore::QNodeId::createId();
        backendRenderTarget.appendRenderOutput(renderTargetOutputId);

        // THEN
        QCOMPARE(backendRenderTarget.renderOutputs().size(), 1);

        // WHEN
        backendRenderTarget.appendRenderOutput(renderTargetOutputId);

        // THEN
        QCOMPARE(backendRenderTarget.renderOutputs().size(), 1);
    }

    void checkAddRemoveOutput()
    {
        // GIVEN
        Qt3DRender::Render::RenderTarget backendRenderTarget;

        // THEN
        QCOMPARE(backendRenderTarget.renderOutputs().size(), 0);

        // WHEN
        Qt3DCore::QNodeId renderTargetOutputId1 = Qt3DCore::QNodeId::createId();
        Qt3DCore::QNodeId renderTargetOutputId2 = Qt3DCore::QNodeId::createId();
        backendRenderTarget.appendRenderOutput(renderTargetOutputId1);
        backendRenderTarget.appendRenderOutput(renderTargetOutputId2);

        // THEN
        QCOMPARE(backendRenderTarget.renderOutputs().size(), 2);

        // WHEN
        backendRenderTarget.removeRenderOutput(Qt3DCore::QNodeId());

        // THEN
        QCOMPARE(backendRenderTarget.renderOutputs().size(), 2);

        // WHEN
        backendRenderTarget.removeRenderOutput(renderTargetOutputId1);

        // THEN
        QCOMPARE(backendRenderTarget.renderOutputs().size(), 1);

        // WHEN
        backendRenderTarget.removeRenderOutput(renderTargetOutputId1);

        // THEN
        QCOMPARE(backendRenderTarget.renderOutputs().size(), 1);

        // WHEN
        backendRenderTarget.removeRenderOutput(renderTargetOutputId2);

        // THEN
        QCOMPARE(backendRenderTarget.renderOutputs().size(), 0);
    }

    void checkSceneChangeEvents()
    {
        // GIVEN
        Qt3DRender::Render::RenderTarget backendRenderTarget;
        TestRenderer renderer;
        backendRenderTarget.setRenderer(&renderer);

        {
            // WHEN
            const bool newValue = false;
            const auto change = Qt3DCore::QPropertyUpdatedChangePtr::create(Qt3DCore::QNodeId());
            change->setPropertyName("enabled");
            change->setValue(newValue);
            backendRenderTarget.sceneChangeEvent(change);

            // THEN
            QCOMPARE(backendRenderTarget.isEnabled(), newValue);
        }
        {
            // WHEN
            Qt3DRender::QRenderTargetOutput targetOutput;
            const auto addChange = Qt3DCore::QPropertyNodeAddedChangePtr::create(Qt3DCore::QNodeId(), &targetOutput);
            addChange->setPropertyName("output");
            backendRenderTarget.sceneChangeEvent(addChange);

            // THEN
            QCOMPARE(backendRenderTarget.renderOutputs().size(), 1);
            QCOMPARE(backendRenderTarget.renderOutputs().first(), targetOutput.id());

            // WHEN
            const auto removeChange = Qt3DCore::QPropertyNodeRemovedChangePtr::create(Qt3DCore::QNodeId(), &targetOutput);
            removeChange->setPropertyName("output");
            backendRenderTarget.sceneChangeEvent(removeChange);

            // THEN
            QCOMPARE(backendRenderTarget.renderOutputs().size(), 0);
        }
    }

};

QTEST_MAIN(tst_RenderTarget)

#include "tst_rendertarget.moc"
