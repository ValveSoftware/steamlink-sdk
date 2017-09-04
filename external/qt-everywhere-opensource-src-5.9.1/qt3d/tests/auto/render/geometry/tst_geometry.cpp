/****************************************************************************
**
** Copyright (C) 2015 Klaralvdalens Datakonsult AB (KDAB).
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
#include <qbackendnodetester.h>
#include <Qt3DRender/private/geometry_p.h>
#include <Qt3DRender/qgeometry.h>
#include <Qt3DRender/qattribute.h>
#include <Qt3DCore/qpropertyupdatedchange.h>
#include <Qt3DCore/qpropertynodeaddedchange.h>
#include <Qt3DCore/qpropertynoderemovedchange.h>
#include "testrenderer.h"

class DummyAttribute : public Qt3DRender::QAttribute
{
    Q_OBJECT
public:
    DummyAttribute(Qt3DCore::QNode *parent = nullptr)
        : Qt3DRender::QAttribute(parent)
    {}
};

class tst_RenderGeometry : public Qt3DCore::QBackendNodeTester
{
    Q_OBJECT
private Q_SLOTS:

    void checkPeerPropertyMirroring()
    {
        // GIVEN
        Qt3DRender::Render::Geometry renderGeometry;

        Qt3DRender::QGeometry geometry;
        Qt3DRender::QAttribute attr1;
        Qt3DRender::QAttribute attr2;
        Qt3DRender::QAttribute attr4;
        Qt3DRender::QAttribute attr3;

        geometry.addAttribute(&attr1);
        geometry.addAttribute(&attr2);
        geometry.addAttribute(&attr3);
        geometry.addAttribute(&attr4);
        geometry.setBoundingVolumePositionAttribute(&attr1);

        // WHEN
        simulateInitialization(&geometry, &renderGeometry);

        // THEN
        QCOMPARE(renderGeometry.peerId(), geometry.id());
        QCOMPARE(renderGeometry.isDirty(), true);
        QCOMPARE(renderGeometry.attributes().count(), 4);
        QCOMPARE(renderGeometry.boundingPositionAttribute(), attr1.id());

        for (int i = 0; i < 4; ++i)
            QCOMPARE(geometry.attributes().at(i)->id(), renderGeometry.attributes().at(i));
    }

    void checkInitialAndCleanedUpState()
    {
        // GIVEN
        Qt3DRender::Render::Geometry renderGeometry;

        // THEN
        QCOMPARE(renderGeometry.isDirty(), false);
        QVERIFY(renderGeometry.attributes().isEmpty());
        QVERIFY(renderGeometry.peerId().isNull());
        QCOMPARE(renderGeometry.boundingPositionAttribute(), Qt3DCore::QNodeId());

        // GIVEN
        Qt3DRender::QGeometry geometry;
        Qt3DRender::QAttribute attr1;
        Qt3DRender::QAttribute attr2;
        Qt3DRender::QAttribute attr4;
        Qt3DRender::QAttribute attr3;
        geometry.setBoundingVolumePositionAttribute(&attr1);

        geometry.addAttribute(&attr1);
        geometry.addAttribute(&attr2);
        geometry.addAttribute(&attr3);
        geometry.addAttribute(&attr4);

        // WHEN
        simulateInitialization(&geometry, &renderGeometry);
        renderGeometry.cleanup();

        // THEN
        QCOMPARE(renderGeometry.isDirty(), false);
        QVERIFY(renderGeometry.attributes().isEmpty());
        QCOMPARE(renderGeometry.boundingPositionAttribute(), Qt3DCore::QNodeId());
    }

    void checkPropertyChanges()
    {
        // GIVEN
        TestRenderer renderer;
        Qt3DRender::Render::Geometry renderGeometry;
        renderGeometry.setRenderer(&renderer);

        DummyAttribute attribute;

        // WHEN
        const auto nodeAddedChange = Qt3DCore::QPropertyNodeAddedChangePtr::create(Qt3DCore::QNodeId(), &attribute);
        nodeAddedChange->setPropertyName("attribute");
        renderGeometry.sceneChangeEvent(nodeAddedChange);

        // THEN
        QCOMPARE(renderGeometry.attributes().count(), 1);
        QVERIFY(renderGeometry.isDirty());

        renderGeometry.unsetDirty();
        QVERIFY(!renderGeometry.isDirty());

        // WHEN
        const auto nodeRemovedChange = Qt3DCore::QPropertyNodeRemovedChangePtr::create(Qt3DCore::QNodeId(), &attribute);
        nodeRemovedChange->setPropertyName("attribute");
        renderGeometry.sceneChangeEvent(nodeRemovedChange);

        // THEN
        QCOMPARE(renderGeometry.attributes().count(), 0);
        QVERIFY(renderGeometry.isDirty());

        renderGeometry.unsetDirty();
        QVERIFY(!renderGeometry.isDirty());

        // WHEN
        const Qt3DCore::QNodeId boundingAttrId = Qt3DCore::QNodeId::createId();
        Qt3DCore::QPropertyUpdatedChangePtr updateChange(new Qt3DCore::QPropertyUpdatedChange(Qt3DCore::QNodeId()));
        updateChange->setValue(QVariant::fromValue(boundingAttrId));
        updateChange->setPropertyName("boundingVolumePositionAttribute");
        renderGeometry.sceneChangeEvent(updateChange);

        // THEN
        QCOMPARE(renderGeometry.boundingPositionAttribute(), boundingAttrId);
        QVERIFY(!renderGeometry.isDirty());
    }
};


QTEST_APPLESS_MAIN(tst_RenderGeometry)

#include "tst_geometry.moc"
