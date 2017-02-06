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

#include <Qt3DRender/qviewport.h>
#include <Qt3DRender/private/qviewport_p.h>

#include "testpostmanarbiter.h"

class tst_QViewport: public QObject
{
    Q_OBJECT

private Q_SLOTS:

    void checkCloning_data()
    {
        QTest::addColumn<Qt3DRender::QViewport *>("viewport");
        QTest::addColumn<QRectF>("normalizedRect");

        Qt3DRender::QViewport *defaultConstructed = new Qt3DRender::QViewport();
        QTest::newRow("defaultConstructed") << defaultConstructed << QRectF(0.0f, 0.0f, 1.0f, 1.0f);

        Qt3DRender::QViewport *smallGreenViewport = new Qt3DRender::QViewport();
        smallGreenViewport->setNormalizedRect(QRectF(0.2f, 0.2f, 0.6f, 0.6f));
        QTest::newRow("smallGreenViewport") << smallGreenViewport << QRectF(0.2f, 0.2f, 0.6f, 0.6f);

    }

    void checkCloning()
    {
        // GIVEN
        QFETCH(Qt3DRender::QViewport *, viewport);
        QFETCH(QRectF, normalizedRect);

        // THEN
        QCOMPARE(viewport->normalizedRect(), normalizedRect);

        // WHEN
        Qt3DCore::QNodeCreatedChangeGenerator creationChangeGenerator(viewport);
        QVector<Qt3DCore::QNodeCreatedChangeBasePtr> creationChanges = creationChangeGenerator.creationChanges();

        // THEN
        QCOMPARE(creationChanges.size(), 1);

        const Qt3DCore::QNodeCreatedChangePtr<Qt3DRender::QViewportData> creationChangeData =
                qSharedPointerCast<Qt3DCore::QNodeCreatedChange<Qt3DRender::QViewportData>>(creationChanges.first());
        const Qt3DRender::QViewportData &cloneData = creationChangeData->data;

        QCOMPARE(viewport->id(), creationChangeData->subjectId());
        QCOMPARE(viewport->isEnabled(), creationChangeData->isNodeEnabled());
        QCOMPARE(viewport->metaObject(), creationChangeData->metaObject());
        QCOMPARE(viewport->normalizedRect(), cloneData.normalizedRect);

        delete viewport;
    }

    void checkPropertyUpdates()
    {
        // GIVEN
        TestArbiter arbiter;
        QScopedPointer<Qt3DRender::QViewport> viewport(new Qt3DRender::QViewport());
        arbiter.setArbiterOnNode(viewport.data());

        // WHEN
        viewport->setNormalizedRect(QRectF(0.5f, 0.5f, 1.0f, 1.0f));
        QCoreApplication::processEvents();

        // THEN
        QCOMPARE(arbiter.events.size(), 1);
        Qt3DCore::QPropertyUpdatedChangePtr change = arbiter.events.first().staticCast<Qt3DCore::QPropertyUpdatedChange>();
        QCOMPARE(change->propertyName(), "normalizedRect");
        QCOMPARE(change->subjectId(), viewport->id());
        QCOMPARE(change->value().value<QRectF>(), QRectF(0.5f, 0.5f, 1.0f, 1.0f));
        QCOMPARE(change->type(), Qt3DCore::PropertyUpdated);

        arbiter.events.clear();

        // WHEN
        viewport->setNormalizedRect(QRectF(0.5f, 0.5f, 1.0f, 1.0f));
        QCoreApplication::processEvents();

        // THEN
        QCOMPARE(arbiter.events.size(), 0);

        // WHEN
        viewport->setNormalizedRect(QRectF(0.0f, 0.0f, 1.0f, 1.0f));
        QCoreApplication::processEvents();

        // THEN
        QCOMPARE(arbiter.events.size(), 1);
        change = arbiter.events.first().staticCast<Qt3DCore::QPropertyUpdatedChange>();
        QCOMPARE(change->propertyName(), "normalizedRect");
        QCOMPARE(change->subjectId(), viewport->id());
        QCOMPARE(change->value().value<QRectF>(), QRectF(0.0f, 0.0f, 1.0f, 1.0f));
        QCOMPARE(change->type(), Qt3DCore::PropertyUpdated);

    }
};

QTEST_MAIN(tst_QViewport)

#include "tst_qviewport.moc"
