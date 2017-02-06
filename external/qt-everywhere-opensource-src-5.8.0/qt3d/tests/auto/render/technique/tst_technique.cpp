/****************************************************************************
**
** Copyright (C) 2016 Paul Lemire <paul.lemire350@gmail.com>
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
#include <Qt3DRender/qtechnique.h>
#include <Qt3DRender/qfilterkey.h>
#include <Qt3DRender/qparameter.h>
#include <Qt3DRender/qrenderpass.h>
#include <Qt3DRender/private/qtechnique_p.h>
#include <Qt3DRender/private/technique_p.h>
#include <Qt3DRender/private/shaderparameterpack_p.h>
#include <Qt3DRender/private/nodemanagers_p.h>
#include <Qt3DRender/private/managers_p.h>
#include <Qt3DRender/private/filterkey_p.h>
#include <Qt3DCore/qpropertyupdatedchange.h>
#include <Qt3DCore/qpropertynodeaddedchange.h>
#include <Qt3DCore/qpropertynoderemovedchange.h>
#include "qbackendnodetester.h"
#include "testrenderer.h"

class tst_Technique : public Qt3DCore::QBackendNodeTester
{
    Q_OBJECT

private Q_SLOTS:

    void checkInitialState()
    {
        // GIVEN
        Qt3DRender::Render::Technique backendTechnique;

        // THEN
        QCOMPARE(backendTechnique.isEnabled(), false);
        QVERIFY(backendTechnique.peerId().isNull());

        QCOMPARE(backendTechnique.graphicsApiFilter()->m_minor, 0);
        QCOMPARE(backendTechnique.graphicsApiFilter()->m_major, 0);
        QCOMPARE(backendTechnique.graphicsApiFilter()->m_profile, Qt3DRender::QGraphicsApiFilter::NoProfile);
        QCOMPARE(backendTechnique.graphicsApiFilter()->m_vendor, QString());
        QCOMPARE(backendTechnique.graphicsApiFilter()->m_extensions, QStringList());

        QCOMPARE(backendTechnique.parameters().size(), 0);
        QCOMPARE(backendTechnique.filterKeys().size(), 0);
        QCOMPARE(backendTechnique.renderPasses().size(), 0);
        QCOMPARE(backendTechnique.isCompatibleWithRenderer(), false);
        QVERIFY(backendTechnique.nodeManager() == nullptr);
    }

    void checkCleanupState()
    {
        // GIVEN
        Qt3DRender::Render::Technique backendTechnique;

        // WHEN
        backendTechnique.setEnabled(true);
        backendTechnique.setCompatibleWithRenderer(true);

        {
            Qt3DRender::QTechnique technique;
            Qt3DRender::QRenderPass pass;
            Qt3DRender::QParameter parameter;
            Qt3DRender::QFilterKey filterKey;

            technique.addRenderPass(&pass);
            technique.addParameter(&parameter);
            technique.addFilterKey(&filterKey);

            simulateInitialization(&technique, &backendTechnique);
        }

        backendTechnique.cleanup();

        // THEN
        QCOMPARE(backendTechnique.isEnabled(), false);
        QCOMPARE(backendTechnique.parameters().size(), 0);
        QCOMPARE(backendTechnique.filterKeys().size(), 0);
        QCOMPARE(backendTechnique.renderPasses().size(), 0);
        QCOMPARE(backendTechnique.isCompatibleWithRenderer(), false);
    }

    void checkInitializeFromPeer()
    {
        // GIVEN
        Qt3DRender::QTechnique technique;
        Qt3DRender::QRenderPass pass;
        Qt3DRender::QParameter parameter;
        Qt3DRender::QFilterKey filterKey;

        technique.addRenderPass(&pass);
        technique.addParameter(&parameter);
        technique.addFilterKey(&filterKey);

        {
            // WHEN
            Qt3DRender::Render::Technique backendTechnique;
            simulateInitialization(&technique, &backendTechnique);

            // THEN
            QCOMPARE(backendTechnique.isEnabled(), true);
            QCOMPARE(backendTechnique.peerId(), technique.id());
            Qt3DRender::GraphicsApiFilterData apiFilterData = Qt3DRender::QGraphicsApiFilterPrivate::get(technique.graphicsApiFilter())->m_data;
            QCOMPARE(*backendTechnique.graphicsApiFilter(), apiFilterData);
            QCOMPARE(backendTechnique.parameters().size(), 1);
            QCOMPARE(backendTechnique.parameters().first(), parameter.id());
            QCOMPARE(backendTechnique.filterKeys().size(), 1);
            QCOMPARE(backendTechnique.filterKeys().first(), filterKey.id());
            QCOMPARE(backendTechnique.renderPasses().size(), 1);
            QCOMPARE(backendTechnique.renderPasses().first(), pass.id());
            QCOMPARE(backendTechnique.isCompatibleWithRenderer(), false);
        }
        {
            // WHEN
            Qt3DRender::Render::Technique backendTechnique;
            technique.setEnabled(false);
            simulateInitialization(&technique, &backendTechnique);

            // THEN
            QCOMPARE(backendTechnique.peerId(), technique.id());
            QCOMPARE(backendTechnique.isEnabled(), false);
        }
    }

    void checkSetCompatibleWithRenderer()
    {
        // GIVEN
        Qt3DRender::Render::Technique backendTechnique;

        // THEN
        QCOMPARE(backendTechnique.isCompatibleWithRenderer(), false);

        // WHEN
        backendTechnique.setCompatibleWithRenderer(true);

        // THEN
        QCOMPARE(backendTechnique.isCompatibleWithRenderer(), true);
    }

    void checkSceneChangeEvents()
    {
        // GIVEN
        Qt3DRender::Render::Technique backendTechnique;
        TestRenderer renderer;
        backendTechnique.setRenderer(&renderer);

        {
            // WHEN
            const bool newValue = false;
            const auto change = Qt3DCore::QPropertyUpdatedChangePtr::create(Qt3DCore::QNodeId());
            change->setPropertyName("enabled");
            change->setValue(newValue);
            backendTechnique.sceneChangeEvent(change);

            // THEN
            QCOMPARE(backendTechnique.isEnabled(), newValue);
        }
        {
            // WHEN
            backendTechnique.setCompatibleWithRenderer(true);
            Qt3DRender::GraphicsApiFilterData newValue;
            newValue.m_major = 4;
            newValue.m_minor = 5;
            newValue.m_vendor = QStringLiteral("ATI");

            const auto change = Qt3DCore::QPropertyUpdatedChangePtr::create(Qt3DCore::QNodeId());
            change->setPropertyName("graphicsApiFilterData");
            change->setValue(QVariant::fromValue(newValue));
            backendTechnique.sceneChangeEvent(change);

            // THEN
            QCOMPARE(*backendTechnique.graphicsApiFilter(), newValue);
            QCOMPARE(backendTechnique.isCompatibleWithRenderer(), false);
        }
        {
            Qt3DRender::QParameter parameter;

            {
                // WHEN
                const auto change = Qt3DCore::QPropertyNodeAddedChangePtr::create(Qt3DCore::QNodeId(), &parameter);
                change->setPropertyName("parameter");
                backendTechnique.sceneChangeEvent(change);

                // THEN
                QCOMPARE(backendTechnique.parameters().size(), 1);
                QCOMPARE(backendTechnique.parameters().first(), parameter.id());
            }
            {
                // WHEN
                const auto change = Qt3DCore::QPropertyNodeRemovedChangePtr::create(Qt3DCore::QNodeId(), &parameter);
                change->setPropertyName("parameter");
                backendTechnique.sceneChangeEvent(change);

                // THEN
                QCOMPARE(backendTechnique.parameters().size(), 0);
            }
        }
        {
            Qt3DRender::QFilterKey filterKey;

            {
                // WHEN
                const auto change = Qt3DCore::QPropertyNodeAddedChangePtr::create(Qt3DCore::QNodeId(), &filterKey);
                change->setPropertyName("filterKeys");
                backendTechnique.sceneChangeEvent(change);

                // THEN
                QCOMPARE(backendTechnique.filterKeys().size(), 1);
                QCOMPARE(backendTechnique.filterKeys().first(), filterKey.id());
            }
            {
                // WHEN
                const auto change = Qt3DCore::QPropertyNodeRemovedChangePtr::create(Qt3DCore::QNodeId(), &filterKey);
                change->setPropertyName("filterKeys");
                backendTechnique.sceneChangeEvent(change);

                // THEN
                QCOMPARE(backendTechnique.filterKeys().size(), 0);
            }
        }
        {
            Qt3DRender::QRenderPass pass;

            {
                // WHEN
                const auto change = Qt3DCore::QPropertyNodeAddedChangePtr::create(Qt3DCore::QNodeId(), &pass);
                change->setPropertyName("pass");
                backendTechnique.sceneChangeEvent(change);

                // THEN
                QCOMPARE(backendTechnique.renderPasses().size(), 1);
                QCOMPARE(backendTechnique.renderPasses().first(), pass.id());
            }
            {
                // WHEN
                const auto change = Qt3DCore::QPropertyNodeRemovedChangePtr::create(Qt3DCore::QNodeId(), &pass);
                change->setPropertyName("pass");
                backendTechnique.sceneChangeEvent(change);

                // THEN
                QCOMPARE(backendTechnique.renderPasses().size(), 0);
            }
        }
    }

    void checkIsCompatibleWithFilters()
    {
        // GIVEN
        Qt3DRender::Render::Technique backendTechnique;
        Qt3DRender::Render::NodeManagers nodeManagers;

        backendTechnique.setNodeManager(&nodeManagers);

        Qt3DRender::QFilterKey *filterKey1 = new Qt3DRender::QFilterKey();
        Qt3DRender::QFilterKey *filterKey2 = new Qt3DRender::QFilterKey();
        Qt3DRender::QFilterKey *filterKey3 = new Qt3DRender::QFilterKey();
        Qt3DRender::QFilterKey *filterKey4 = new Qt3DRender::QFilterKey();
        Qt3DRender::QFilterKey *filterKey5 = new Qt3DRender::QFilterKey();

        filterKey1->setName(QStringLiteral("displacement"));
        filterKey2->setName(QStringLiteral("diffRatio"));
        filterKey3->setName(QStringLiteral("oil"));
        filterKey4->setName(QStringLiteral("oil"));
        filterKey5->setName(QStringLiteral("heads"));

        filterKey1->setValue(QVariant(427.0f));
        filterKey2->setValue(QVariant(4.11f));
        filterKey3->setValue(QVariant(QStringLiteral("Valvoline-VR1")));
        filterKey4->setValue(QVariant(QStringLiteral("Mobil-1")));
        filterKey5->setName(QStringLiteral("AFR"));

        // Create backend nodes
        // WHEN
        Qt3DRender::Render::FilterKey *backendFilterKey1 = nodeManagers.filterKeyManager()->getOrCreateResource(filterKey1->id());
        Qt3DRender::Render::FilterKey *backendFilterKey2 = nodeManagers.filterKeyManager()->getOrCreateResource(filterKey2->id());
        Qt3DRender::Render::FilterKey *backendFilterKey3 = nodeManagers.filterKeyManager()->getOrCreateResource(filterKey3->id());
        Qt3DRender::Render::FilterKey *backendFilterKey4 = nodeManagers.filterKeyManager()->getOrCreateResource(filterKey4->id());
        Qt3DRender::Render::FilterKey *backendFilterKey5 = nodeManagers.filterKeyManager()->getOrCreateResource(filterKey5->id());

        simulateInitialization(filterKey1, backendFilterKey1);
        simulateInitialization(filterKey2, backendFilterKey2);
        simulateInitialization(filterKey3, backendFilterKey3);
        simulateInitialization(filterKey4, backendFilterKey4);
        simulateInitialization(filterKey5, backendFilterKey5);

        // THEN
        QCOMPARE(nodeManagers.filterKeyManager()->activeHandles().size(), 5);

        {
            // WHEN
            backendTechnique.appendFilterKey(filterKey1->id());
            backendTechnique.appendFilterKey(filterKey2->id());

            // THEN
            QCOMPARE(backendTechnique.filterKeys().size(), 2);

            // WHEN
            Qt3DCore::QNodeIdVector techniqueFilters;
            techniqueFilters.push_back(filterKey1->id());
            techniqueFilters.push_back(filterKey2->id());
            techniqueFilters.push_back(filterKey3->id());
            techniqueFilters.push_back(filterKey5->id());

            // THEN -> incompatible technique doesn't have enough filters
            QCOMPARE(backendTechnique.isCompatibleWithFilters(techniqueFilters), false);

            // WHEN
            backendTechnique.removeFilterKey(filterKey1->id());
            backendTechnique.removeFilterKey(filterKey2->id());

            // THEN
            QCOMPARE(backendTechnique.filterKeys().size(), 0);
        }

        {
            // WHEN
            backendTechnique.appendFilterKey(filterKey1->id());
            backendTechnique.appendFilterKey(filterKey2->id());
            backendTechnique.appendFilterKey(filterKey3->id());
            backendTechnique.appendFilterKey(filterKey5->id());

            // THEN
            QCOMPARE(backendTechnique.filterKeys().size(), 4);

            // WHEN
            Qt3DCore::QNodeIdVector techniqueFilters;
            techniqueFilters.push_back(filterKey1->id());
            techniqueFilters.push_back(filterKey2->id());
            techniqueFilters.push_back(filterKey3->id());
            techniqueFilters.push_back(filterKey5->id());

            // THEN -> compatible same number
            QCOMPARE(backendTechnique.isCompatibleWithFilters(techniqueFilters), true);

            // WHEN
            backendTechnique.removeFilterKey(filterKey1->id());
            backendTechnique.removeFilterKey(filterKey2->id());
            backendTechnique.removeFilterKey(filterKey3->id());
            backendTechnique.removeFilterKey(filterKey5->id());

            // THEN
            QCOMPARE(backendTechnique.filterKeys().size(), 0);
        }

        {
            // WHEN
            backendTechnique.appendFilterKey(filterKey1->id());
            backendTechnique.appendFilterKey(filterKey2->id());
            backendTechnique.appendFilterKey(filterKey3->id());
            backendTechnique.appendFilterKey(filterKey5->id());

            // THEN
            QCOMPARE(backendTechnique.filterKeys().size(), 4);

            // WHEN
            Qt3DCore::QNodeIdVector techniqueFilters;
            techniqueFilters.push_back(filterKey1->id());
            techniqueFilters.push_back(filterKey2->id());
            techniqueFilters.push_back(filterKey4->id());
            techniqueFilters.push_back(filterKey5->id());

            // THEN -> compatible same number, one not matching
            QCOMPARE(backendTechnique.isCompatibleWithFilters(techniqueFilters), false);

            // WHEN
            backendTechnique.removeFilterKey(filterKey1->id());
            backendTechnique.removeFilterKey(filterKey2->id());
            backendTechnique.removeFilterKey(filterKey3->id());
            backendTechnique.removeFilterKey(filterKey5->id());

            // THEN
            QCOMPARE(backendTechnique.filterKeys().size(), 0);
        }

        {
            // WHEN
            backendTechnique.appendFilterKey(filterKey1->id());
            backendTechnique.appendFilterKey(filterKey2->id());
            backendTechnique.appendFilterKey(filterKey3->id());
            backendTechnique.appendFilterKey(filterKey5->id());

            // THEN
            QCOMPARE(backendTechnique.filterKeys().size(), 4);

            // WHEN
            Qt3DCore::QNodeIdVector techniqueFilters;
            techniqueFilters.push_back(filterKey1->id());
            techniqueFilters.push_back(filterKey2->id());
            techniqueFilters.push_back(filterKey3->id());

            // THEN -> technique has more than necessary filters
            QCOMPARE(backendTechnique.isCompatibleWithFilters(techniqueFilters), true);

            // WHEN
            backendTechnique.removeFilterKey(filterKey1->id());
            backendTechnique.removeFilterKey(filterKey2->id());
            backendTechnique.removeFilterKey(filterKey3->id());
            backendTechnique.removeFilterKey(filterKey5->id());

            // THEN
            QCOMPARE(backendTechnique.filterKeys().size(), 0);
        }

        {
            // WHEN
            backendTechnique.appendFilterKey(filterKey1->id());
            backendTechnique.appendFilterKey(filterKey2->id());
            backendTechnique.appendFilterKey(filterKey3->id());
            backendTechnique.appendFilterKey(filterKey5->id());

            // THEN
            QCOMPARE(backendTechnique.filterKeys().size(), 4);

            // WHEN
            Qt3DCore::QNodeIdVector techniqueFilters;
            techniqueFilters.push_back(filterKey1->id());
            techniqueFilters.push_back(filterKey2->id());
            techniqueFilters.push_back(filterKey4->id());

            // THEN -> technique has more than necessary filters
            // but one is not matching
            QCOMPARE(backendTechnique.isCompatibleWithFilters(techniqueFilters), false);

            // WHEN
            backendTechnique.removeFilterKey(filterKey1->id());
            backendTechnique.removeFilterKey(filterKey2->id());
            backendTechnique.removeFilterKey(filterKey3->id());
            backendTechnique.removeFilterKey(filterKey5->id());

            // THEN
            QCOMPARE(backendTechnique.filterKeys().size(), 0);
        }
    }
};

QTEST_MAIN(tst_Technique)

#include "tst_technique.moc"
