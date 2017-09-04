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
#include <Qt3DCore/qentity.h>

#include <Qt3DRender/qrendersettings.h>
#include <Qt3DRender/qrendersurfaceselector.h>
#include <Qt3DRender/private/qrendersurfaceselector_p.h>
#include <QSignalSpy>
#include <Qt3DCore/qpropertyupdatedchange.h>
#include <Qt3DCore/private/qnodecreatedchangegenerator_p.h>
#include <Qt3DCore/qnodecreatedchange.h>
#include "testpostmanarbiter.h"

class tst_QRenderSurfaceSelector: public QObject
{
    Q_OBJECT

private Q_SLOTS:
    void shouldFindInstanceInEntityTree_data()
    {
        QTest::addColumn<QSharedPointer<Qt3DCore::QEntity>>("root");
        QTest::addColumn<Qt3DRender::QRenderSurfaceSelector*>("expected");

        auto root = QSharedPointer<Qt3DCore::QEntity>::create();
        auto settings = new Qt3DRender::QRenderSettings;
        root->addComponent(settings);
        auto selector = new Qt3DRender::QRenderSurfaceSelector;
        settings->setActiveFrameGraph(selector);
        QTest::newRow("simplest_tree") << root << selector;

        root = QSharedPointer<Qt3DCore::QEntity>::create();
        settings = new Qt3DRender::QRenderSettings;
        root->addComponent(settings);
        settings->setActiveFrameGraph(new Qt3DRender::QFrameGraphNode);
        selector = nullptr;
        QTest::newRow("no_selector") << root << selector;

        root = QSharedPointer<Qt3DCore::QEntity>::create();
        settings = new Qt3DRender::QRenderSettings;
        root->addComponent(settings);
        selector = nullptr;
        QTest::newRow("no_framegraph") << root << selector;

        root = QSharedPointer<Qt3DCore::QEntity>::create();
        selector = nullptr;
        QTest::newRow("no_rendersettings") << root << selector;

        root = QSharedPointer<Qt3DCore::QEntity>::create();
        auto entity = new Qt3DCore::QEntity(root.data());
        entity = new Qt3DCore::QEntity(entity);
        settings = new Qt3DRender::QRenderSettings;
        entity->addComponent(settings);
        selector = new Qt3DRender::QRenderSurfaceSelector;
        settings->setActiveFrameGraph(selector);
        QTest::newRow("in_subentity") << root << selector;

        root = QSharedPointer<Qt3DCore::QEntity>::create();
        entity = new Qt3DCore::QEntity(root.data());
        entity = new Qt3DCore::QEntity(entity);
        settings = new Qt3DRender::QRenderSettings;
        entity->addComponent(settings);
        auto node = new Qt3DRender::QFrameGraphNode;
        settings->setActiveFrameGraph(node);
        node = new Qt3DRender::QFrameGraphNode(node);
        selector = new Qt3DRender::QRenderSurfaceSelector(node);
        QTest::newRow("in_deeper_framegraph") << root << selector;
    }

    void shouldFindInstanceInEntityTree()
    {
        // GIVEN
        QFETCH(QSharedPointer<Qt3DCore::QEntity>, root);

        // WHEN
        auto selector = Qt3DRender::QRenderSurfaceSelectorPrivate::find(root.data());

        // THEN
        QFETCH(Qt3DRender::QRenderSurfaceSelector*, expected);
        QCOMPARE(selector, expected);
    }

    void checkDefaultConstruction()
    {
        // GIVEN
        Qt3DRender::QRenderSurfaceSelector renderSurfaceSelector;

        // THEN
        QVERIFY(renderSurfaceSelector.surface() == nullptr);
        QCOMPARE(renderSurfaceSelector.externalRenderTargetSize(), QSize());
        QCOMPARE(renderSurfaceSelector.surfacePixelRatio(), 1.0f);
    }

    void checkPropertyChanges()
    {
        // GIVEN
        Qt3DRender::QRenderSurfaceSelector renderSurfaceSelector;

        {
            // WHEN
            QSignalSpy spy(&renderSurfaceSelector, SIGNAL(surfaceChanged(QObject *)));
            QWindow newValue;
            renderSurfaceSelector.setSurface(&newValue);

            // THEN
            QVERIFY(spy.isValid());
            QCOMPARE(renderSurfaceSelector.surface(), &newValue);
            QCOMPARE(spy.count(), 1);

            // WHEN
            spy.clear();
            renderSurfaceSelector.setSurface(&newValue);

            // THEN
            QCOMPARE(renderSurfaceSelector.surface(), &newValue);
            QCOMPARE(spy.count(), 0);

            // Prevents crashes with temporary window being destroyed
            renderSurfaceSelector.setSurface(nullptr);
            QCoreApplication::processEvents();
        }
        {
            // WHEN
            QSignalSpy spy(&renderSurfaceSelector, SIGNAL(externalRenderTargetSizeChanged(QSize)));
            const QSize newValue(512, 512);
            renderSurfaceSelector.setExternalRenderTargetSize(newValue);

            // THEN
            QVERIFY(spy.isValid());
            QCOMPARE(renderSurfaceSelector.externalRenderTargetSize(), newValue);
            QCOMPARE(spy.count(), 1);

            // WHEN
            spy.clear();
            renderSurfaceSelector.setExternalRenderTargetSize(newValue);

            // THEN
            QCOMPARE(renderSurfaceSelector.externalRenderTargetSize(), newValue);
            QCOMPARE(spy.count(), 0);
        }
        {
            // WHEN
            QSignalSpy spy(&renderSurfaceSelector, SIGNAL(surfacePixelRatioChanged(float)));
            const float newValue = 15.0f;
            renderSurfaceSelector.setSurfacePixelRatio(newValue);

            // THEN
            QVERIFY(spy.isValid());
            QCOMPARE(renderSurfaceSelector.surfacePixelRatio(), newValue);
            QCOMPARE(spy.count(), 1);

            // WHEN
            spy.clear();
            renderSurfaceSelector.setSurfacePixelRatio(newValue);

            // THEN
            QCOMPARE(renderSurfaceSelector.surfacePixelRatio(), newValue);
            QCOMPARE(spy.count(), 0);
        }
    }

    void checkCreationData()
    {
        // GIVEN
        Qt3DRender::QRenderSurfaceSelector renderSurfaceSelector;
        QWindow newValue;

        renderSurfaceSelector.setSurface(&newValue);
        renderSurfaceSelector.setExternalRenderTargetSize(QSize(128, 128));
        renderSurfaceSelector.setSurfacePixelRatio(25.0f);

        // WHEN
        QVector<Qt3DCore::QNodeCreatedChangeBasePtr> creationChanges;

        {
            Qt3DCore::QNodeCreatedChangeGenerator creationChangeGenerator(&renderSurfaceSelector);
            creationChanges = creationChangeGenerator.creationChanges();
        }

        // THEN
        {
            QCOMPARE(creationChanges.size(), 1);

            const auto creationChangeData = qSharedPointerCast<Qt3DCore::QNodeCreatedChange<Qt3DRender::QRenderSurfaceSelectorData>>(creationChanges.first());
            const Qt3DRender::QRenderSurfaceSelectorData cloneData = creationChangeData->data;

            QCOMPARE(renderSurfaceSelector.surface(), cloneData.surface.data());
            QCOMPARE(renderSurfaceSelector.externalRenderTargetSize(), cloneData.externalRenderTargetSize);
            QCOMPARE(renderSurfaceSelector.surfacePixelRatio(), cloneData.surfacePixelRatio);
            QCOMPARE(renderSurfaceSelector.id(), creationChangeData->subjectId());
            QCOMPARE(renderSurfaceSelector.isEnabled(), true);
            QCOMPARE(renderSurfaceSelector.isEnabled(), creationChangeData->isNodeEnabled());
            QCOMPARE(renderSurfaceSelector.metaObject(), creationChangeData->metaObject());
        }

        // WHEN
        renderSurfaceSelector.setEnabled(false);

        {
            Qt3DCore::QNodeCreatedChangeGenerator creationChangeGenerator(&renderSurfaceSelector);
            creationChanges = creationChangeGenerator.creationChanges();
        }

        // THEN
        {
            QCOMPARE(creationChanges.size(), 1);

            const auto creationChangeData = qSharedPointerCast<Qt3DCore::QNodeCreatedChange<Qt3DRender::QRenderSurfaceSelectorData>>(creationChanges.first());
            const Qt3DRender::QRenderSurfaceSelectorData cloneData = creationChangeData->data;

            QCOMPARE(renderSurfaceSelector.surface(), cloneData.surface.data());
            QCOMPARE(renderSurfaceSelector.externalRenderTargetSize(), cloneData.externalRenderTargetSize);
            QCOMPARE(renderSurfaceSelector.surfacePixelRatio(), cloneData.surfacePixelRatio);
            QCOMPARE(renderSurfaceSelector.id(), creationChangeData->subjectId());
            QCOMPARE(renderSurfaceSelector.isEnabled(), false);
            QCOMPARE(renderSurfaceSelector.isEnabled(), creationChangeData->isNodeEnabled());
            QCOMPARE(renderSurfaceSelector.metaObject(), creationChangeData->metaObject());
        }

        // Prevents crashes with temporary window being destroyed
        renderSurfaceSelector.setSurface(nullptr);
    }

    void checkSurfaceUpdate()
    {
        // GIVEN
        TestArbiter arbiter;
        Qt3DRender::QRenderSurfaceSelector renderSurfaceSelector;
        arbiter.setArbiterOnNode(&renderSurfaceSelector);
        QWindow newWindow;

        {
            // WHEN
            renderSurfaceSelector.setSurface(&newWindow);
            QCoreApplication::processEvents();

            // THEN
            QCOMPARE(arbiter.events.size(), 1);
            auto change = arbiter.events.first().staticCast<Qt3DCore::QPropertyUpdatedChange>();
            QCOMPARE(change->propertyName(), "surface");
            QCOMPARE(change->value().value<QObject *>(), renderSurfaceSelector.surface());
            QCOMPARE(change->type(), Qt3DCore::PropertyUpdated);

            arbiter.events.clear();
        }

        {
            // WHEN
            renderSurfaceSelector.setSurface(&newWindow);
            QCoreApplication::processEvents();

            // THEN
            QCOMPARE(arbiter.events.size(), 0);
        }

        // Prevents crashes with temporary window being destroyed
        renderSurfaceSelector.setSurface(nullptr);
    }

    void checkExternalRenderTargetSizeUpdate()
    {
        // GIVEN
        TestArbiter arbiter;
        Qt3DRender::QRenderSurfaceSelector renderSurfaceSelector;
        arbiter.setArbiterOnNode(&renderSurfaceSelector);

        {
            // WHEN
            renderSurfaceSelector.setExternalRenderTargetSize(QSize(454, 454));
            QCoreApplication::processEvents();

            // THEN
            QCOMPARE(arbiter.events.size(), 1);
            auto change = arbiter.events.first().staticCast<Qt3DCore::QPropertyUpdatedChange>();
            QCOMPARE(change->propertyName(), "externalRenderTargetSize");
            QCOMPARE(change->value().value<QSize>(), renderSurfaceSelector.externalRenderTargetSize());
            QCOMPARE(change->type(), Qt3DCore::PropertyUpdated);

            arbiter.events.clear();
        }

        {
            // WHEN
            renderSurfaceSelector.setExternalRenderTargetSize(QSize(454, 454));
            QCoreApplication::processEvents();

            // THEN
            QCOMPARE(arbiter.events.size(), 0);
        }

    }

    void checkSurfacePixelRatioUpdate()
    {
        // GIVEN
        TestArbiter arbiter;
        Qt3DRender::QRenderSurfaceSelector renderSurfaceSelector;
        arbiter.setArbiterOnNode(&renderSurfaceSelector);

        {
            // WHEN
            renderSurfaceSelector.setSurfacePixelRatio(99.0f);
            QCoreApplication::processEvents();

            // THEN
            QCOMPARE(arbiter.events.size(), 1);
            auto change = arbiter.events.first().staticCast<Qt3DCore::QPropertyUpdatedChange>();
            QCOMPARE(change->propertyName(), "surfacePixelRatio");
            QCOMPARE(change->value().value<float>(), renderSurfaceSelector.surfacePixelRatio());
            QCOMPARE(change->type(), Qt3DCore::PropertyUpdated);

            arbiter.events.clear();
        }

        {
            // WHEN
            renderSurfaceSelector.setSurfacePixelRatio(99.0f);
            QCoreApplication::processEvents();

            // THEN
            QCOMPARE(arbiter.events.size(), 0);
        }

    }

};

QTEST_MAIN(tst_QRenderSurfaceSelector)

#include "tst_qrendersurfaceselector.moc"
