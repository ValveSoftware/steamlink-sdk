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
#include <Qt3DRender/qabstracttextureimage.h>
#include <Qt3DRender/private/qabstracttextureimage_p.h>
#include <QObject>
#include <QSignalSpy>
#include <Qt3DCore/qpropertyupdatedchange.h>
#include <Qt3DCore/private/qnodecreatedchangegenerator_p.h>
#include <Qt3DCore/qnodecreatedchange.h>
#include "testpostmanarbiter.h"

class FakeTextureImage : public Qt3DRender::QAbstractTextureImage
{
protected:
    Qt3DRender::QTextureImageDataGeneratorPtr dataGenerator() const Q_DECL_OVERRIDE
    {
        return Qt3DRender::QTextureImageDataGeneratorPtr();
    }
};

class tst_QAbstractTextureImage : public QObject
{
    Q_OBJECT

private Q_SLOTS:

    void initTestCase()
    {
        qRegisterMetaType<Qt3DRender::QAbstractTexture::CubeMapFace>("QAbstractTexture::CubeMapFace");
    }

    void checkDefaultConstruction()
    {
        // GIVEN
        FakeTextureImage abstractTextureImage;

        // THEN
        QCOMPARE(abstractTextureImage.mipLevel(), 0);
        QCOMPARE(abstractTextureImage.layer(), 0);
        QCOMPARE(abstractTextureImage.face(), Qt3DRender::QAbstractTexture::CubeMapPositiveX);
    }

    void checkPropertyChanges()
    {
        // GIVEN
        FakeTextureImage abstractTextureImage;

        {
            // WHEN
            QSignalSpy spy(&abstractTextureImage, SIGNAL(mipLevelChanged(int)));
            const int newValue = 5;
            abstractTextureImage.setMipLevel(newValue);

            // THEN
            QVERIFY(spy.isValid());
            QCOMPARE(abstractTextureImage.mipLevel(), newValue);
            QCOMPARE(spy.count(), 1);

            // WHEN
            spy.clear();
            abstractTextureImage.setMipLevel(newValue);

            // THEN
            QCOMPARE(abstractTextureImage.mipLevel(), newValue);
            QCOMPARE(spy.count(), 0);
        }
        {
            // WHEN
            QSignalSpy spy(&abstractTextureImage, SIGNAL(layerChanged(int)));
            const int newValue = 12;
            abstractTextureImage.setLayer(newValue);

            // THEN
            QVERIFY(spy.isValid());
            QCOMPARE(abstractTextureImage.layer(), newValue);
            QCOMPARE(spy.count(), 1);

            // WHEN
            spy.clear();
            abstractTextureImage.setLayer(newValue);

            // THEN
            QCOMPARE(abstractTextureImage.layer(), newValue);
            QCOMPARE(spy.count(), 0);
        }
        {
            // WHEN
            QSignalSpy spy(&abstractTextureImage, SIGNAL(faceChanged(Qt3DRender::QAbstractTexture::CubeMapFace)));
            const Qt3DRender::QAbstractTexture::CubeMapFace newValue =  Qt3DRender::QAbstractTexture::CubeMapNegativeZ;
            abstractTextureImage.setFace(newValue);

            // THEN
            QVERIFY(spy.isValid());
            QCOMPARE(abstractTextureImage.face(), newValue);
            QCOMPARE(spy.count(), 1);

            // WHEN
            spy.clear();
            abstractTextureImage.setFace(newValue);

            // THEN
            QCOMPARE(abstractTextureImage.face(), newValue);
            QCOMPARE(spy.count(), 0);
        }
    }

    void checkCreationData()
    {
        // GIVEN
        FakeTextureImage abstractTextureImage;

        abstractTextureImage.setMipLevel(32);
        abstractTextureImage.setLayer(56);
        abstractTextureImage.setFace(Qt3DRender::QAbstractTexture::CubeMapNegativeY);

        // WHEN
        QVector<Qt3DCore::QNodeCreatedChangeBasePtr> creationChanges;

        {
            Qt3DCore::QNodeCreatedChangeGenerator creationChangeGenerator(&abstractTextureImage);
            creationChanges = creationChangeGenerator.creationChanges();
        }

        // THEN
        {
            QCOMPARE(creationChanges.size(), 1);

            const auto creationChangeData = qSharedPointerCast<Qt3DCore::QNodeCreatedChange<Qt3DRender::QAbstractTextureImageData>>(creationChanges.first());
            const Qt3DRender::QAbstractTextureImageData cloneData = creationChangeData->data;

            QCOMPARE(abstractTextureImage.mipLevel(), cloneData.mipLevel);
            QCOMPARE(abstractTextureImage.layer(), cloneData.layer);
            QCOMPARE(abstractTextureImage.face(), cloneData.face);
            QCOMPARE(abstractTextureImage.id(), creationChangeData->subjectId());
            QCOMPARE(abstractTextureImage.isEnabled(), true);
            QCOMPARE(abstractTextureImage.isEnabled(), creationChangeData->isNodeEnabled());
            QCOMPARE(abstractTextureImage.metaObject(), creationChangeData->metaObject());
        }

        // WHEN
        abstractTextureImage.setEnabled(false);

        {
            Qt3DCore::QNodeCreatedChangeGenerator creationChangeGenerator(&abstractTextureImage);
            creationChanges = creationChangeGenerator.creationChanges();
        }

        // THEN
        {
            QCOMPARE(creationChanges.size(), 1);

            const auto creationChangeData = qSharedPointerCast<Qt3DCore::QNodeCreatedChange<Qt3DRender::QAbstractTextureImageData>>(creationChanges.first());
            const Qt3DRender::QAbstractTextureImageData cloneData = creationChangeData->data;

            QCOMPARE(abstractTextureImage.mipLevel(), cloneData.mipLevel);
            QCOMPARE(abstractTextureImage.layer(), cloneData.layer);
            QCOMPARE(abstractTextureImage.face(), cloneData.face);
            QCOMPARE(abstractTextureImage.id(), creationChangeData->subjectId());
            QCOMPARE(abstractTextureImage.isEnabled(), false);
            QCOMPARE(abstractTextureImage.isEnabled(), creationChangeData->isNodeEnabled());
            QCOMPARE(abstractTextureImage.metaObject(), creationChangeData->metaObject());
        }
    }

    void checkMipLevelUpdate()
    {
        // GIVEN
        TestArbiter arbiter;
        FakeTextureImage abstractTextureImage;
        arbiter.setArbiterOnNode(&abstractTextureImage);

        {
            // WHEN
            abstractTextureImage.setMipLevel(9);
            QCoreApplication::processEvents();

            // THEN
            QCOMPARE(arbiter.events.size(), 1);
            auto change = arbiter.events.first().staticCast<Qt3DCore::QPropertyUpdatedChange>();
            QCOMPARE(change->propertyName(), "mipLevel");
            QCOMPARE(change->value().value<int>(), abstractTextureImage.mipLevel());
            QCOMPARE(change->type(), Qt3DCore::PropertyUpdated);

            arbiter.events.clear();
        }

        {
            // WHEN
            abstractTextureImage.setMipLevel(9);
            QCoreApplication::processEvents();

            // THEN
            QCOMPARE(arbiter.events.size(), 0);
        }

    }

    void checkLayerUpdate()
    {
        // GIVEN
        TestArbiter arbiter;
        FakeTextureImage abstractTextureImage;
        arbiter.setArbiterOnNode(&abstractTextureImage);

        {
            // WHEN
            abstractTextureImage.setLayer(12);
            QCoreApplication::processEvents();

            // THEN
            QCOMPARE(arbiter.events.size(), 1);
            auto change = arbiter.events.first().staticCast<Qt3DCore::QPropertyUpdatedChange>();
            QCOMPARE(change->propertyName(), "layer");
            QCOMPARE(change->value().value<int>(), abstractTextureImage.layer());
            QCOMPARE(change->type(), Qt3DCore::PropertyUpdated);

            arbiter.events.clear();
        }

        {
            // WHEN
            abstractTextureImage.setLayer(12);
            QCoreApplication::processEvents();

            // THEN
            QCOMPARE(arbiter.events.size(), 0);
        }

    }

    void checkFaceUpdate()
    {
        // GIVEN
        TestArbiter arbiter;
        FakeTextureImage abstractTextureImage;
        arbiter.setArbiterOnNode(&abstractTextureImage);

        {
            // WHEN
            abstractTextureImage.setFace(Qt3DRender::QAbstractTexture::CubeMapPositiveY);
            QCoreApplication::processEvents();

            // THEN
            QCOMPARE(arbiter.events.size(), 1);
            auto change = arbiter.events.first().staticCast<Qt3DCore::QPropertyUpdatedChange>();
            QCOMPARE(change->propertyName(), "face");
            QCOMPARE(change->value().value<Qt3DRender::QAbstractTexture::CubeMapFace>(), abstractTextureImage.face());
            QCOMPARE(change->type(), Qt3DCore::PropertyUpdated);

            arbiter.events.clear();
        }

        {
            // WHEN
            abstractTextureImage.setFace(Qt3DRender::QAbstractTexture::CubeMapPositiveY);
            QCoreApplication::processEvents();

            // THEN
            QCOMPARE(arbiter.events.size(), 0);
        }

    }

};

QTEST_MAIN(tst_QAbstractTextureImage)

#include "tst_qabstracttextureimage.moc"
