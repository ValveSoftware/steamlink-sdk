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
#include <Qt3DCore/private/qnode_p.h>
#include <Qt3DCore/private/qscene_p.h>
#include <Qt3DCore/private/qnodecreatedchangegenerator_p.h>

#include <Qt3DRender/qclearbuffers.h>
#include <Qt3DRender/private/qclearbuffers_p.h>

#include "testpostmanarbiter.h"

class tst_QClearBuffers: public QObject
{
    Q_OBJECT

private Q_SLOTS:

    void checkCloning_data()
    {
        QTest::addColumn<Qt3DRender::QClearBuffers *>("clearBuffers");
        QTest::addColumn<Qt3DRender::QClearBuffers::BufferType>("bufferType");

        Qt3DRender::QClearBuffers *defaultConstructed = new Qt3DRender::QClearBuffers();
        QTest::newRow("defaultConstructed") << defaultConstructed << Qt3DRender::QClearBuffers::None;

        Qt3DRender::QClearBuffers *allBuffers = new Qt3DRender::QClearBuffers();
        allBuffers->setBuffers(Qt3DRender::QClearBuffers::AllBuffers);
        QTest::newRow("allBuffers") << allBuffers << Qt3DRender::QClearBuffers::AllBuffers;

        Qt3DRender::QClearBuffers *depthBuffer = new Qt3DRender::QClearBuffers();
        depthBuffer->setBuffers(Qt3DRender::QClearBuffers::DepthBuffer);
        QTest::newRow("depthBuffer") << depthBuffer << Qt3DRender::QClearBuffers::DepthBuffer;

        Qt3DRender::QClearBuffers *colorDepthBuffer = new Qt3DRender::QClearBuffers();
        colorDepthBuffer->setBuffers(Qt3DRender::QClearBuffers::ColorDepthBuffer);
        QTest::newRow("colorDepthBuffer") << colorDepthBuffer << Qt3DRender::QClearBuffers::ColorDepthBuffer;
    }

    void checkCloning()
    {
        // GIVEN
        QFETCH(Qt3DRender::QClearBuffers *, clearBuffers);
        QFETCH(Qt3DRender::QClearBuffers::BufferType, bufferType);

        // THEN
        QCOMPARE(clearBuffers->buffers(), bufferType);

        // WHEN
        Qt3DCore::QNodeCreatedChangeGenerator creationChangeGenerator(clearBuffers);
        QVector<Qt3DCore::QNodeCreatedChangeBasePtr> creationChanges = creationChangeGenerator.creationChanges();

        // THEN
        QCOMPARE(creationChanges.size(), 1);

        const Qt3DCore::QNodeCreatedChangePtr<Qt3DRender::QClearBuffersData> creationChangeData =
                qSharedPointerCast<Qt3DCore::QNodeCreatedChange<Qt3DRender::QClearBuffersData>>(creationChanges.first());
        const Qt3DRender::QClearBuffersData &cloneData = creationChangeData->data;

        // THEN
        QCOMPARE(clearBuffers->id(), creationChangeData->subjectId());
        QCOMPARE(clearBuffers->isEnabled(), creationChangeData->isNodeEnabled());
        QCOMPARE(clearBuffers->metaObject(), creationChangeData->metaObject());
        QCOMPARE(clearBuffers->buffers(), cloneData.buffersType);
        QCOMPARE(clearBuffers->clearColor(), cloneData.clearColor);
        QCOMPARE(clearBuffers->clearDepthValue(), cloneData.clearDepthValue);
        QCOMPARE(clearBuffers->clearStencilValue(), cloneData.clearStencilValue);

        delete clearBuffers;
    }

    void checkPropertyUpdates()
    {
        // GIVEN
        TestArbiter arbiter;
        QScopedPointer<Qt3DRender::QClearBuffers> clearBuffer(new Qt3DRender::QClearBuffers());
        arbiter.setArbiterOnNode(clearBuffer.data());

        // WHEN
        clearBuffer->setBuffers(Qt3DRender::QClearBuffers::AllBuffers);
        QCoreApplication::processEvents();

        // THEN
        QCOMPARE(arbiter.events.size(), 1);
        Qt3DCore::QPropertyUpdatedChangePtr change = arbiter.events.first().staticCast<Qt3DCore::QPropertyUpdatedChange>();
        QCOMPARE(change->propertyName(), "buffers");
        QCOMPARE(change->subjectId(), clearBuffer->id());
        QCOMPARE(change->value().value<Qt3DRender::QClearBuffers::BufferType>(), Qt3DRender::QClearBuffers::AllBuffers);
        QCOMPARE(change->type(), Qt3DCore::PropertyUpdated);

        arbiter.events.clear();

        // WHEN
        clearBuffer->setBuffers(Qt3DRender::QClearBuffers::AllBuffers);
        QCoreApplication::processEvents();

        // THEN
        QCOMPARE(arbiter.events.size(), 0);

        // WHEN
        clearBuffer->setBuffers(Qt3DRender::QClearBuffers::ColorDepthBuffer);
        QCoreApplication::processEvents();

        // THEN
        QCOMPARE(arbiter.events.size(), 1);
        change = arbiter.events.first().staticCast<Qt3DCore::QPropertyUpdatedChange>();
        QCOMPARE(change->propertyName(), "buffers");
        QCOMPARE(change->subjectId(), clearBuffer->id());
        QCOMPARE(change->value().value<Qt3DRender::QClearBuffers::BufferType>(), Qt3DRender::QClearBuffers::ColorDepthBuffer);
        QCOMPARE(change->type(), Qt3DCore::PropertyUpdated);

        arbiter.events.clear();
    }
};

QTEST_MAIN(tst_QClearBuffers)

#include "tst_qclearbuffers.moc"
