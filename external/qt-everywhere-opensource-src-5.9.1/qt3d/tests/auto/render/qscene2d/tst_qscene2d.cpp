/****************************************************************************
**
** Copyright (C) 2017 The Qt Company Ltd.
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
#include <Qt3DQuickScene2D/qscene2d.h>
#include <Qt3DRender/qrendertargetoutput.h>
#include <private/qscene2d_p.h>
#include <QObject>
#include <QSignalSpy>
#include <Qt3DCore/qpropertyupdatedchange.h>
#include <Qt3DCore/private/qnodecreatedchangegenerator_p.h>
#include <Qt3DCore/qnodecreatedchange.h>
#include "testpostmanarbiter.h"

using namespace Qt3DRender::Quick;

class tst_QScene2D : public QObject
{
    Q_OBJECT

private Q_SLOTS:

    void initTestCase()
    {
        qRegisterMetaType<Qt3DRender::Quick::QScene2D::RenderPolicy>(
                    "QScene2D::RenderPolicy");
    }

    void checkDefaultConstruction()
    {
        // GIVEN
        Qt3DRender::Quick::QScene2D scene2d;

        // THEN
        QCOMPARE(scene2d.output(), nullptr);
        QCOMPARE(scene2d.renderPolicy(), QScene2D::Continuous);
        QCOMPARE(scene2d.item(), nullptr);
        QCOMPARE(scene2d.isMouseEnabled(), true);
    }

    void checkPropertyChanges()
    {
        // GIVEN
        Qt3DRender::Quick::QScene2D scene2d;
        QScopedPointer<Qt3DRender::QRenderTargetOutput> output(new Qt3DRender::QRenderTargetOutput());
        QScopedPointer<QQuickItem> item(new QQuickItem());

        {
            // WHEN
            QSignalSpy spy(&scene2d, SIGNAL(outputChanged(Qt3DRender::QRenderTargetOutput*)));
            Qt3DRender::QRenderTargetOutput *newValue = output.data();
            scene2d.setOutput(newValue);

            // THEN
            QVERIFY(spy.isValid());
            QCOMPARE(scene2d.output(), newValue);
            QCOMPARE(spy.count(), 1);

            // WHEN
            spy.clear();
            scene2d.setOutput(newValue);

            // THEN
            QCOMPARE(scene2d.output(), newValue);
            QCOMPARE(spy.count(), 0);
        }
        {
            // WHEN
            QSignalSpy spy(&scene2d, SIGNAL(renderPolicyChanged(QScene2D::RenderPolicy)));
            const QScene2D::RenderPolicy newValue = QScene2D::SingleShot;
            scene2d.setRenderPolicy(newValue);

            // THEN
            QVERIFY(spy.isValid());
            QCOMPARE(scene2d.renderPolicy(), newValue);
            QCOMPARE(spy.count(), 1);

            // WHEN
            spy.clear();
            scene2d.setRenderPolicy(newValue);

            // THEN
            QCOMPARE(scene2d.renderPolicy(), newValue);
            QCOMPARE(spy.count(), 0);
        }
        {
            // WHEN
            QSignalSpy spy(&scene2d, SIGNAL(itemChanged(QQuickItem*)));
            QQuickItem *newValue = item.data();
            scene2d.setItem(newValue);

            // THEN
            QVERIFY(spy.isValid());
            QCOMPARE(scene2d.item(), newValue);
            QCOMPARE(spy.count(), 1);

            // WHEN
            spy.clear();
            scene2d.setItem(newValue);

            // THEN
            QCOMPARE(scene2d.item(), newValue);
            QCOMPARE(spy.count(), 0);
        }

        {
            // WHEN
            QSignalSpy spy(&scene2d, SIGNAL(mouseEnabledChanged(bool)));
            bool newValue = false;
            scene2d.setMouseEnabled(newValue);

            // THEN
            QVERIFY(spy.isValid());
            QCOMPARE(scene2d.isMouseEnabled(), newValue);
            QCOMPARE(spy.count(), 1);

            // WHEN
            spy.clear();
            scene2d.setMouseEnabled(newValue);

            // THEN
            QCOMPARE(scene2d.isMouseEnabled(), newValue);
            QCOMPARE(spy.count(), 0);
        }
    }

    void checkCreationData()
    {
        // GIVEN
        Qt3DRender::Quick::QScene2D scene2d;
        QScopedPointer<Qt3DRender::QRenderTargetOutput> output(new Qt3DRender::QRenderTargetOutput());

        scene2d.setOutput(output.data());
        scene2d.setRenderPolicy(QScene2D::SingleShot);

        // WHEN
        QVector<Qt3DCore::QNodeCreatedChangeBasePtr> creationChanges;

        {
            Qt3DCore::QNodeCreatedChangeGenerator creationChangeGenerator(&scene2d);
            creationChanges = creationChangeGenerator.creationChanges();
        }

        // THEN
        {
            QCOMPARE(creationChanges.size(), 1);

            const auto creationChangeData = qSharedPointerCast<Qt3DCore::QNodeCreatedChange<
                    Qt3DRender::Quick::QScene2DData>>(creationChanges.first());
            const Qt3DRender::Quick::QScene2DData cloneData = creationChangeData->data;

            QCOMPARE(scene2d.output()->id(), cloneData.output);
            QCOMPARE(scene2d.renderPolicy(), cloneData.renderPolicy);
            QCOMPARE(scene2d.id(), creationChangeData->subjectId());
            QCOMPARE(scene2d.isEnabled(), true);
            QCOMPARE(scene2d.isEnabled(), creationChangeData->isNodeEnabled());
            QCOMPARE(scene2d.metaObject(), creationChangeData->metaObject());
            QCOMPARE(scene2d.isMouseEnabled(), cloneData.mouseEnabled);
        }

        // WHEN
        scene2d.setEnabled(false);

        {
            Qt3DCore::QNodeCreatedChangeGenerator creationChangeGenerator(&scene2d);
            creationChanges = creationChangeGenerator.creationChanges();
        }

        // THEN
        {
            QCOMPARE(creationChanges.size(), 1);

            const auto creationChangeData = qSharedPointerCast<Qt3DCore::QNodeCreatedChange<
                    Qt3DRender::Quick::QScene2DData>>(creationChanges.first());
            const Qt3DRender::Quick::QScene2DData cloneData = creationChangeData->data;

            QCOMPARE(scene2d.output()->id(), cloneData.output);
            QCOMPARE(scene2d.renderPolicy(), cloneData.renderPolicy);
            QCOMPARE(scene2d.id(), creationChangeData->subjectId());
            QCOMPARE(scene2d.isEnabled(), false);
            QCOMPARE(scene2d.isEnabled(), creationChangeData->isNodeEnabled());
            QCOMPARE(scene2d.metaObject(), creationChangeData->metaObject());
        }
    }

    void checkOutputUpdate()
    {
        // GIVEN
        TestArbiter arbiter;
        Qt3DRender::Quick::QScene2D scene2d;
        arbiter.setArbiterOnNode(&scene2d);
        QScopedPointer<Qt3DRender::QRenderTargetOutput> output(new Qt3DRender::QRenderTargetOutput());

        {
            // WHEN
            scene2d.setOutput(output.data());
            QCoreApplication::processEvents();

            // THEN
            QCOMPARE(arbiter.events.size(), 1);
            auto change = arbiter.events.first().staticCast<Qt3DCore::QPropertyUpdatedChange>();
            QCOMPARE(change->propertyName(), "output");
            QCOMPARE(change->value().value<Qt3DCore::QNodeId>(), scene2d.output()->id());
            QCOMPARE(change->type(), Qt3DCore::PropertyUpdated);

            arbiter.events.clear();
        }

        {
            // WHEN
            scene2d.setOutput(output.data());
            QCoreApplication::processEvents();

            // THEN
            QCOMPARE(arbiter.events.size(), 0);
        }

    }

    void checkRenderPolicyUpdate()
    {
        // GIVEN
        TestArbiter arbiter;
        Qt3DRender::Quick::QScene2D scene2d;
        arbiter.setArbiterOnNode(&scene2d);

        {
            // WHEN
            scene2d.setRenderPolicy(QScene2D::SingleShot);
            QCoreApplication::processEvents();

            // THEN
            QCOMPARE(arbiter.events.size(), 1);
            auto change = arbiter.events.first().staticCast<Qt3DCore::QPropertyUpdatedChange>();
            QCOMPARE(change->propertyName(), "renderPolicy");
            QCOMPARE(change->value().value<Qt3DRender::Quick::QScene2D::RenderPolicy>(),
                     scene2d.renderPolicy());
            QCOMPARE(change->type(), Qt3DCore::PropertyUpdated);

            arbiter.events.clear();
        }

        {
            // WHEN
            scene2d.setRenderPolicy(QScene2D::SingleShot);
            QCoreApplication::processEvents();

            // THEN
            QCOMPARE(arbiter.events.size(), 0);
        }

    }

    void checkMouseEnabledUpdate()
    {
        // GIVEN
        TestArbiter arbiter;
        Qt3DRender::Quick::QScene2D scene2d;
        arbiter.setArbiterOnNode(&scene2d);

        {
            // WHEN
            scene2d.setMouseEnabled(false);
            QCoreApplication::processEvents();

            // THEN
            QCOMPARE(arbiter.events.size(), 1);
            auto change = arbiter.events.first().staticCast<Qt3DCore::QPropertyUpdatedChange>();
            QCOMPARE(change->propertyName(), "mouseEnabled");
            QCOMPARE(change->value().toBool(), scene2d.isMouseEnabled());
            QCOMPARE(change->type(), Qt3DCore::PropertyUpdated);

            arbiter.events.clear();
        }

        {
            // WHEN
            scene2d.setMouseEnabled(false);
            QCoreApplication::processEvents();

            // THEN
            QCOMPARE(arbiter.events.size(), 0);
        }

    }

};

QTEST_MAIN(tst_QScene2D)

#include "tst_qscene2d.moc"
