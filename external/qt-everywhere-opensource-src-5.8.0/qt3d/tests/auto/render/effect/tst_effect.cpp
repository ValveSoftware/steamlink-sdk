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
#include <Qt3DRender/qeffect.h>
#include <Qt3DRender/qtechnique.h>
#include <Qt3DRender/qparameter.h>
#include <Qt3DRender/private/qeffect_p.h>
#include <Qt3DRender/private/effect_p.h>
#include <Qt3DRender/private/shaderparameterpack_p.h>
#include <Qt3DCore/qpropertyupdatedchange.h>
#include <Qt3DCore/qpropertynodeaddedchange.h>
#include <Qt3DCore/qpropertynoderemovedchange.h>
#include "qbackendnodetester.h"
#include "testrenderer.h"


class tst_Effect : public Qt3DCore::QBackendNodeTester
{
    Q_OBJECT

private Q_SLOTS:

    void checkInitialState()
    {
        // GIVEN
        Qt3DRender::Render::Effect backendEffect;

        // THEN
        QCOMPARE(backendEffect.isEnabled(), false);
        QVERIFY(backendEffect.peerId().isNull());
        QVERIFY(backendEffect.techniques().empty());
    }

    void checkCleanupState()
    {
        // GIVEN
        Qt3DRender::Render::Effect backendEffect;

        // WHEN
        backendEffect.setEnabled(true);

        {
            Qt3DRender::QEffect effect;
            Qt3DRender::QTechnique technique;
            Qt3DRender::QParameter parameter;
            effect.addTechnique(&technique);
            effect.addParameter(&parameter);
            simulateInitialization(&effect, &backendEffect);
        }

        backendEffect.cleanup();

        // THEN
        QCOMPARE(backendEffect.isEnabled(), false);
        QCOMPARE(backendEffect.techniques().size(), 0);
    }

    void checkInitializeFromPeer()
    {
        // GIVEN
        Qt3DRender::QEffect effect;
        Qt3DRender::QTechnique technique;
        Qt3DRender::QParameter parameter;

        effect.addTechnique(&technique);
        effect.addParameter(&parameter);

        {
            // WHEN
            Qt3DRender::Render::Effect backendEffect;
            simulateInitialization(&effect, &backendEffect);

            // THEN
            QCOMPARE(backendEffect.isEnabled(), true);
            QCOMPARE(backendEffect.peerId(), effect.id());
            QCOMPARE(backendEffect.techniques().size(), 1);
            QCOMPARE(backendEffect.techniques().first(), technique.id());
            QCOMPARE(backendEffect.parameters().size(), 1);
            QCOMPARE(backendEffect.parameters().first(), parameter.id());
        }
        {
            // WHEN
            Qt3DRender::Render::Effect backendEffect;
            effect.setEnabled(false);
            simulateInitialization(&effect, &backendEffect);

            // THEN
            QCOMPARE(backendEffect.peerId(), effect.id());
            QCOMPARE(backendEffect.isEnabled(), false);
        }
    }

    void checkSceneChangeEvents()
    {
        // GIVEN
        Qt3DRender::Render::Effect backendEffect;
        TestRenderer renderer;
        backendEffect.setRenderer(&renderer);

        {
            // WHEN
            const bool newValue = false;
            const auto change = Qt3DCore::QPropertyUpdatedChangePtr::create(Qt3DCore::QNodeId());
            change->setPropertyName("enabled");
            change->setValue(newValue);
            backendEffect.sceneChangeEvent(change);

            // THEN
            QCOMPARE(backendEffect.isEnabled(), newValue);
        }
        {
            Qt3DRender::QTechnique technique;
            {
                // WHEN
                const auto change = Qt3DCore::QPropertyNodeAddedChangePtr::create(Qt3DCore::QNodeId(), &technique);
                change->setPropertyName("technique");
                backendEffect.sceneChangeEvent(change);

                // THEN
                QCOMPARE(backendEffect.techniques().size(), 1);
                QCOMPARE(backendEffect.techniques().first(), technique.id());
            }
            {
                // WHEN
                const auto change = Qt3DCore::QPropertyNodeRemovedChangePtr::create(Qt3DCore::QNodeId(), &technique);
                change->setPropertyName("technique");
                backendEffect.sceneChangeEvent(change);

                // THEN
                QCOMPARE(backendEffect.techniques().size(), 0);
            }
        }
        {
            Qt3DRender::QParameter parameter;

            {
                // WHEN
                const auto change = Qt3DCore::QPropertyNodeAddedChangePtr::create(Qt3DCore::QNodeId(), &parameter);
                change->setPropertyName("parameter");
                backendEffect.sceneChangeEvent(change);

                // THEN
                QCOMPARE(backendEffect.parameters().size(), 1);
                QCOMPARE(backendEffect.parameters().first(), parameter.id());
            }
            {
                // WHEN
                const auto change = Qt3DCore::QPropertyNodeRemovedChangePtr::create(Qt3DCore::QNodeId(), &parameter);
                change->setPropertyName("parameter");
                backendEffect.sceneChangeEvent(change);

                // THEN
                QCOMPARE(backendEffect.parameters().size(), 0);
            }
        }
    }

};

QTEST_MAIN(tst_Effect)

#include "tst_effect.moc"
