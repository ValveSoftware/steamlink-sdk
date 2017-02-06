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
#include <QObject>
#include <QMetaObject>
#include <Qt3DRender/qrendertargetoutput.h>
#include <Qt3DRender/private/qrendertargetoutput_p.h>
#include <Qt3DRender/qabstracttexture.h>
#include <Qt3DRender/qtexture.h>
#include <QSignalSpy>
#include <Qt3DCore/qpropertyupdatedchange.h>
#include <Qt3DCore/private/qnodecreatedchangegenerator_p.h>
#include <Qt3DCore/qnodecreatedchange.h>
#include "testpostmanarbiter.h"

class tst_QRenderTargetOutput : public QObject
{
    Q_OBJECT

private Q_SLOTS:

    void initTestCase()
    {
        qRegisterMetaType<Qt3DRender::QRenderTargetOutput::AttachmentPoint>("AttachmentPoint");
        qRegisterMetaType<Qt3DRender::QAbstractTexture::CubeMapFace>("QAbstractTexture::CubeMapFace");
    }

    void checkDefaultConstruction()
    {
        // GIVEN
        Qt3DRender::QRenderTargetOutput renderTargetOutput;

        // THEN
        QCOMPARE(renderTargetOutput.attachmentPoint(), Qt3DRender::QRenderTargetOutput::Color0);
        QVERIFY(renderTargetOutput.texture() == nullptr);
        QCOMPARE(renderTargetOutput.mipLevel(), 0);
        QCOMPARE(renderTargetOutput.layer(), 0);
        QCOMPARE(renderTargetOutput.face(), Qt3DRender::QAbstractTexture::CubeMapNegativeX);
    }

    void checkPropertyChanges()
    {
        // GIVEN
        Qt3DRender::QRenderTargetOutput renderTargetOutput;

        {
            // WHEN
            QSignalSpy spy(&renderTargetOutput, SIGNAL(attachmentPointChanged(AttachmentPoint)));
            const Qt3DRender::QRenderTargetOutput::AttachmentPoint newValue = Qt3DRender::QRenderTargetOutput::Color15;
            renderTargetOutput.setAttachmentPoint(newValue);

            // THEN
            QVERIFY(spy.isValid());
            QCOMPARE(renderTargetOutput.attachmentPoint(), newValue);
            QCOMPARE(spy.count(), 1);

            // WHEN
            spy.clear();
            renderTargetOutput.setAttachmentPoint(newValue);

            // THEN
            QCOMPARE(renderTargetOutput.attachmentPoint(), newValue);
            QCOMPARE(spy.count(), 0);

        }
        {
            // WHEN
            QSignalSpy spy(&renderTargetOutput, SIGNAL(textureChanged(QAbstractTexture *)));
            Qt3DRender::QTexture3D newValue;
            renderTargetOutput.setTexture(&newValue);

            // THEN
            QVERIFY(spy.isValid());
            QCOMPARE(renderTargetOutput.texture(), &newValue);
            QCOMPARE(spy.count(), 1);

            // WHEN
            spy.clear();
            renderTargetOutput.setTexture(&newValue);

            // THEN
            QCOMPARE(renderTargetOutput.texture(), &newValue);
            QCOMPARE(spy.count(), 0);

        }
        {
            // WHEN
            QSignalSpy spy(&renderTargetOutput, SIGNAL(mipLevelChanged(int)));
            const int newValue = 5;
            renderTargetOutput.setMipLevel(newValue);

            // THEN
            QVERIFY(spy.isValid());
            QCOMPARE(renderTargetOutput.mipLevel(), newValue);
            QCOMPARE(spy.count(), 1);

            // WHEN
            spy.clear();
            renderTargetOutput.setMipLevel(newValue);

            // THEN
            QCOMPARE(renderTargetOutput.mipLevel(), newValue);
            QCOMPARE(spy.count(), 0);

        }
        {
            // WHEN
            QSignalSpy spy(&renderTargetOutput, SIGNAL(layerChanged(int)));
            const int newValue = 300;
            renderTargetOutput.setLayer(newValue);

            // THEN
            QVERIFY(spy.isValid());
            QCOMPARE(renderTargetOutput.layer(), newValue);
            QCOMPARE(spy.count(), 1);

            // WHEN
            spy.clear();
            renderTargetOutput.setLayer(newValue);

            // THEN
            QCOMPARE(renderTargetOutput.layer(), newValue);
            QCOMPARE(spy.count(), 0);

        }
        {
            // WHEN
            QSignalSpy spy(&renderTargetOutput, SIGNAL(faceChanged(QAbstractTexture::CubeMapFace)));
            const Qt3DRender::QAbstractTexture::CubeMapFace newValue = Qt3DRender::QAbstractTexture::CubeMapNegativeZ;
            renderTargetOutput.setFace(newValue);

            // THEN
            QVERIFY(spy.isValid());
            QCOMPARE(renderTargetOutput.face(), newValue);
            QCOMPARE(spy.count(), 1);

            // WHEN
            spy.clear();
            renderTargetOutput.setFace(newValue);

            // THEN
            QCOMPARE(renderTargetOutput.face(), newValue);
            QCOMPARE(spy.count(), 0);

        }
    }

    void checkCreationData()
    {
        // GIVEN
        Qt3DRender::QRenderTargetOutput renderTargetOutput;

        renderTargetOutput.setAttachmentPoint(Qt3DRender::QRenderTargetOutput::Color5);
        renderTargetOutput.setMipLevel(10);
        renderTargetOutput.setLayer(2);
        renderTargetOutput.setFace(Qt3DRender::QAbstractTexture::CubeMapNegativeY);

        // WHEN
        QVector<Qt3DCore::QNodeCreatedChangeBasePtr> creationChanges;

        {
            Qt3DCore::QNodeCreatedChangeGenerator creationChangeGenerator(&renderTargetOutput);
            creationChanges = creationChangeGenerator.creationChanges();
        }

        // THEN
        {
            QCOMPARE(creationChanges.size(), 1);

            const auto creationChangeData = qSharedPointerCast<Qt3DCore::QNodeCreatedChange<Qt3DRender::QRenderTargetOutputData>>(creationChanges.first());
            const Qt3DRender::QRenderTargetOutputData cloneData = creationChangeData->data;

            QCOMPARE(renderTargetOutput.attachmentPoint(), cloneData.attachmentPoint);
            QCOMPARE(Qt3DCore::QNodeId(), cloneData.textureId);
            QCOMPARE(renderTargetOutput.mipLevel(), cloneData.mipLevel);
            QCOMPARE(renderTargetOutput.layer(), cloneData.layer);
            QCOMPARE(renderTargetOutput.face(), cloneData.face);
            QCOMPARE(renderTargetOutput.id(), creationChangeData->subjectId());
            QCOMPARE(renderTargetOutput.isEnabled(), true);
            QCOMPARE(renderTargetOutput.isEnabled(), creationChangeData->isNodeEnabled());
            QCOMPARE(renderTargetOutput.metaObject(), creationChangeData->metaObject());
        }

        // WHEN
        renderTargetOutput.setEnabled(false);

        {
            Qt3DCore::QNodeCreatedChangeGenerator creationChangeGenerator(&renderTargetOutput);
            creationChanges = creationChangeGenerator.creationChanges();
        }

        // THEN
        {
            QCOMPARE(creationChanges.size(), 1);

            const auto creationChangeData = qSharedPointerCast<Qt3DCore::QNodeCreatedChange<Qt3DRender::QRenderTargetOutputData>>(creationChanges.first());
            const Qt3DRender::QRenderTargetOutputData cloneData = creationChangeData->data;

            QCOMPARE(renderTargetOutput.attachmentPoint(), cloneData.attachmentPoint);
            QCOMPARE(Qt3DCore::QNodeId(), cloneData.textureId);
            QCOMPARE(renderTargetOutput.mipLevel(), cloneData.mipLevel);
            QCOMPARE(renderTargetOutput.layer(), cloneData.layer);
            QCOMPARE(renderTargetOutput.face(), cloneData.face);
            QCOMPARE(renderTargetOutput.id(), creationChangeData->subjectId());
            QCOMPARE(renderTargetOutput.isEnabled(), false);
            QCOMPARE(renderTargetOutput.isEnabled(), creationChangeData->isNodeEnabled());
            QCOMPARE(renderTargetOutput.metaObject(), creationChangeData->metaObject());
        }
    }

    void checkAttachmentPointUpdate()
    {
        // GIVEN
        TestArbiter arbiter;
        Qt3DRender::QRenderTargetOutput renderTargetOutput;
        arbiter.setArbiterOnNode(&renderTargetOutput);

        {
            // WHEN
            renderTargetOutput.setAttachmentPoint(Qt3DRender::QRenderTargetOutput::Color1);
            QCoreApplication::processEvents();

            // THEN
            QCOMPARE(arbiter.events.size(), 1);
            auto change = arbiter.events.first().staticCast<Qt3DCore::QPropertyUpdatedChange>();
            QCOMPARE(change->propertyName(), "attachmentPoint");
            QCOMPARE(change->value().value<Qt3DRender::QRenderTargetOutput::AttachmentPoint>(), renderTargetOutput.attachmentPoint());
            QCOMPARE(change->type(), Qt3DCore::PropertyUpdated);

            arbiter.events.clear();
        }

        {
            // WHEN
            renderTargetOutput.setAttachmentPoint(Qt3DRender::QRenderTargetOutput::Color1);
            QCoreApplication::processEvents();

            // THEN
            QCOMPARE(arbiter.events.size(), 0);
        }

    }

    void checkTextureUpdate()
    {
        // GIVEN
        TestArbiter arbiter;
        Qt3DRender::QRenderTargetOutput renderTargetOutput;
        Qt3DRender::QTexture3D texture;
        arbiter.setArbiterOnNode(&renderTargetOutput);

        {
            // WHEN
            renderTargetOutput.setTexture(&texture);
            QCoreApplication::processEvents();

            // THEN
            QCOMPARE(arbiter.events.size(), 1);
            auto change = arbiter.events.first().staticCast<Qt3DCore::QPropertyUpdatedChange>();
            QCOMPARE(change->propertyName(), "texture");
            QCOMPARE(change->value().value<Qt3DCore::QNodeId>(), renderTargetOutput.texture()->id());
            QCOMPARE(change->type(), Qt3DCore::PropertyUpdated);

            arbiter.events.clear();
        }

        {
            // WHEN
            renderTargetOutput.setTexture(&texture);
            QCoreApplication::processEvents();

            // THEN
            QCOMPARE(arbiter.events.size(), 0);
        }

    }

    void checkMipLevelUpdate()
    {
        // GIVEN
        TestArbiter arbiter;
        Qt3DRender::QRenderTargetOutput renderTargetOutput;
        arbiter.setArbiterOnNode(&renderTargetOutput);

        {
            // WHEN
            renderTargetOutput.setMipLevel(6);
            QCoreApplication::processEvents();

            // THEN
            QCOMPARE(arbiter.events.size(), 1);
            auto change = arbiter.events.first().staticCast<Qt3DCore::QPropertyUpdatedChange>();
            QCOMPARE(change->propertyName(), "mipLevel");
            QCOMPARE(change->value().value<int>(), renderTargetOutput.mipLevel());
            QCOMPARE(change->type(), Qt3DCore::PropertyUpdated);

            arbiter.events.clear();
        }

        {
            // WHEN
            renderTargetOutput.setMipLevel(6);
            QCoreApplication::processEvents();

            // THEN
            QCOMPARE(arbiter.events.size(), 0);
        }

    }

    void checkLayerUpdate()
    {
        // GIVEN
        TestArbiter arbiter;
        Qt3DRender::QRenderTargetOutput renderTargetOutput;
        arbiter.setArbiterOnNode(&renderTargetOutput);

        {
            // WHEN
            renderTargetOutput.setLayer(8);
            QCoreApplication::processEvents();

            // THEN
            QCOMPARE(arbiter.events.size(), 1);
            auto change = arbiter.events.first().staticCast<Qt3DCore::QPropertyUpdatedChange>();
            QCOMPARE(change->propertyName(), "layer");
            QCOMPARE(change->value().value<int>(), renderTargetOutput.layer());
            QCOMPARE(change->type(), Qt3DCore::PropertyUpdated);

            arbiter.events.clear();
        }

        {
            // WHEN
            renderTargetOutput.setLayer(8);
            QCoreApplication::processEvents();

            // THEN
            QCOMPARE(arbiter.events.size(), 0);
        }

    }

    void checkFaceUpdate()
    {
        // GIVEN
        TestArbiter arbiter;
        Qt3DRender::QRenderTargetOutput renderTargetOutput;
        arbiter.setArbiterOnNode(&renderTargetOutput);

        {
            // WHEN
            renderTargetOutput.setFace(Qt3DRender::QAbstractTexture::CubeMapPositiveY);
            QCoreApplication::processEvents();

            // THEN
            QCOMPARE(arbiter.events.size(), 1);
            auto change = arbiter.events.first().staticCast<Qt3DCore::QPropertyUpdatedChange>();
            QCOMPARE(change->propertyName(), "face");
            QCOMPARE(change->value().value<Qt3DRender::QAbstractTexture::CubeMapFace>(), renderTargetOutput.face());
            QCOMPARE(change->type(), Qt3DCore::PropertyUpdated);

            arbiter.events.clear();
        }

        {
            // WHEN
            renderTargetOutput.setFace(Qt3DRender::QAbstractTexture::CubeMapPositiveY);
            QCoreApplication::processEvents();

            // THEN
            QCOMPARE(arbiter.events.size(), 0);
        }

    }

};

QTEST_MAIN(tst_QRenderTargetOutput)

#include "tst_qrendertargetoutput.moc"
