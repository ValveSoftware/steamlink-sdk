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
#include <Qt3DRender/qabstracttexture.h>
#include <Qt3DRender/private/qabstracttexture_p.h>
#include <QObject>
#include <QSignalSpy>
#include <Qt3DRender/qabstracttextureimage.h>
#include <Qt3DCore/qpropertyupdatedchange.h>
#include <Qt3DCore/qpropertynodeaddedchange.h>
#include <Qt3DCore/qpropertynoderemovedchange.h>
#include <Qt3DCore/private/qnodecreatedchangegenerator_p.h>
#include <Qt3DCore/qnodecreatedchange.h>
#include "testpostmanarbiter.h"

class FakeTexture : public Qt3DRender::QAbstractTexture
{
};

class FakeTextureImage : public Qt3DRender::QAbstractTextureImage
{
protected:
    Qt3DRender::QTextureImageDataGeneratorPtr dataGenerator() const Q_DECL_OVERRIDE
    {
        return Qt3DRender::QTextureImageDataGeneratorPtr();
    }
};

class tst_QAbstractTexture : public QObject
{
    Q_OBJECT

private Q_SLOTS:

    void initTestCase()
    {
        qRegisterMetaType<Qt3DRender::QAbstractTexture::TextureFormat>("TextureFormat");
        qRegisterMetaType<Qt3DRender::QAbstractTexture::Filter>("Filter");
        qRegisterMetaType<Qt3DRender::QAbstractTexture::ComparisonFunction>("ComparisonFunction");
        qRegisterMetaType<Qt3DRender::QAbstractTexture::ComparisonMode>("ComparisonMode");
    }

    void checkDefaultConstruction()
    {
        // GIVEN
        FakeTexture abstractTexture;

        // THEN
        QCOMPARE(abstractTexture.target(), Qt3DRender::QAbstractTexture::Target2D);
        QCOMPARE(abstractTexture.format(), Qt3DRender::QAbstractTexture::Automatic);
        QCOMPARE(abstractTexture.generateMipMaps(), false);
        QCOMPARE(abstractTexture.wrapMode()->x(), Qt3DRender::QTextureWrapMode::ClampToEdge);
        QCOMPARE(abstractTexture.wrapMode()->y(), Qt3DRender::QTextureWrapMode::ClampToEdge);
        QCOMPARE(abstractTexture.wrapMode()->z(), Qt3DRender::QTextureWrapMode::ClampToEdge);
        QCOMPARE(abstractTexture.status(), Qt3DRender::QAbstractTexture::None);
        QCOMPARE(abstractTexture.width(), 1);
        QCOMPARE(abstractTexture.height(), 1);
        QCOMPARE(abstractTexture.depth(), 1);
        QCOMPARE(abstractTexture.magnificationFilter(), Qt3DRender::QAbstractTexture::Nearest);
        QCOMPARE(abstractTexture.minificationFilter(),  Qt3DRender::QAbstractTexture::Nearest);
        QCOMPARE(abstractTexture.maximumAnisotropy(), 1.0f);
        QCOMPARE(abstractTexture.comparisonFunction(), Qt3DRender::QAbstractTexture::CompareLessEqual);
        QCOMPARE(abstractTexture.comparisonMode(), Qt3DRender::QAbstractTexture::CompareNone);
        QCOMPARE(abstractTexture.layers(), 1);
        QCOMPARE(abstractTexture.samples(), 1);
        QCOMPARE(abstractTexture.textureImages().size(), 0);
    }

    void checkPropertyChanges()
    {
        // GIVEN
        FakeTexture abstractTexture;

        {
            // WHEN
            QSignalSpy spy(&abstractTexture, SIGNAL(formatChanged(TextureFormat)));
            const Qt3DRender::QAbstractTexture::TextureFormat newValue = Qt3DRender::QAbstractTexture::R8I;
            abstractTexture.setFormat(newValue);

            // THEN
            QVERIFY(spy.isValid());
            QCOMPARE(abstractTexture.format(), newValue);
            QCOMPARE(spy.count(), 1);

            // WHEN
            spy.clear();
            abstractTexture.setFormat(newValue);

            // THEN
            QCOMPARE(abstractTexture.format(), newValue);
            QCOMPARE(spy.count(), 0);
        }
        {
            // WHEN
            QSignalSpy spy(&abstractTexture, SIGNAL(generateMipMapsChanged(bool)));
            const bool newValue = true;
            abstractTexture.setGenerateMipMaps(newValue);

            // THEN
            QVERIFY(spy.isValid());
            QCOMPARE(abstractTexture.generateMipMaps(), newValue);
            QCOMPARE(spy.count(), 1);

            // WHEN
            spy.clear();
            abstractTexture.setGenerateMipMaps(newValue);

            // THEN
            QCOMPARE(abstractTexture.generateMipMaps(), newValue);
            QCOMPARE(spy.count(), 0);
        }
        {
            // WHEN
            QSignalSpy spy(&abstractTexture, SIGNAL(widthChanged(int)));
            const int newValue = 383;
            abstractTexture.setWidth(newValue);

            // THEN
            QVERIFY(spy.isValid());
            QCOMPARE(abstractTexture.width(), newValue);
            QCOMPARE(spy.count(), 1);

            // WHEN
            spy.clear();
            abstractTexture.setWidth(newValue);

            // THEN
            QCOMPARE(abstractTexture.width(), newValue);
            QCOMPARE(spy.count(), 0);
        }
        {
            // WHEN
            QSignalSpy spy(&abstractTexture, SIGNAL(heightChanged(int)));
            const int newValue = 427;
            abstractTexture.setHeight(newValue);

            // THEN
            QVERIFY(spy.isValid());
            QCOMPARE(abstractTexture.height(), newValue);
            QCOMPARE(spy.count(), 1);

            // WHEN
            spy.clear();
            abstractTexture.setHeight(newValue);

            // THEN
            QCOMPARE(abstractTexture.height(), newValue);
            QCOMPARE(spy.count(), 0);
        }
        {
            // WHEN
            QSignalSpy spy(&abstractTexture, SIGNAL(depthChanged(int)));
            const int newValue = 454;
            abstractTexture.setDepth(newValue);

            // THEN
            QVERIFY(spy.isValid());
            QCOMPARE(abstractTexture.depth(), newValue);
            QCOMPARE(spy.count(), 1);

            // WHEN
            spy.clear();
            abstractTexture.setDepth(newValue);

            // THEN
            QCOMPARE(abstractTexture.depth(), newValue);
            QCOMPARE(spy.count(), 0);
        }
        {
            // WHEN
            QSignalSpy spy(&abstractTexture, SIGNAL(magnificationFilterChanged(Qt3DRender::QAbstractTexture::Filter)));
            const Qt3DRender::QAbstractTexture::Filter newValue = Qt3DRender::QAbstractTexture::LinearMipMapLinear;
            abstractTexture.setMagnificationFilter(newValue);

            // THEN
            QVERIFY(spy.isValid());
            QCOMPARE(abstractTexture.magnificationFilter(), newValue);
            QCOMPARE(spy.count(), 1);

            // WHEN
            spy.clear();
            abstractTexture.setMagnificationFilter(newValue);

            // THEN
            QCOMPARE(abstractTexture.magnificationFilter(), newValue);
            QCOMPARE(spy.count(), 0);
        }
        {
            // WHEN
            QSignalSpy spy(&abstractTexture, SIGNAL(minificationFilterChanged(Filter)));
            const Qt3DRender::QAbstractTexture::Filter newValue = Qt3DRender::QAbstractTexture::LinearMipMapNearest;
            abstractTexture.setMinificationFilter(newValue);

            // THEN
            QVERIFY(spy.isValid());
            QCOMPARE(abstractTexture.minificationFilter(), newValue);
            QCOMPARE(spy.count(), 1);

            // WHEN
            spy.clear();
            abstractTexture.setMinificationFilter(newValue);

            // THEN
            QCOMPARE(abstractTexture.minificationFilter(), newValue);
            QCOMPARE(spy.count(), 0);
        }
        {
            // WHEN
            QSignalSpy spy(&abstractTexture, SIGNAL(maximumAnisotropyChanged(float)));
            const float newValue = 100.0f;
            abstractTexture.setMaximumAnisotropy(newValue);

            // THEN
            QVERIFY(spy.isValid());
            QCOMPARE(abstractTexture.maximumAnisotropy(), newValue);
            QCOMPARE(spy.count(), 1);

            // WHEN
            spy.clear();
            abstractTexture.setMaximumAnisotropy(newValue);

            // THEN
            QCOMPARE(abstractTexture.maximumAnisotropy(), newValue);
            QCOMPARE(spy.count(), 0);
        }
        {
            // WHEN
            QSignalSpy spy(&abstractTexture, SIGNAL(comparisonFunctionChanged(Qt3DRender::QAbstractTexture::ComparisonFunction)));
            const Qt3DRender::QAbstractTexture::ComparisonFunction newValue = Qt3DRender::QAbstractTexture::CompareGreaterEqual;
            abstractTexture.setComparisonFunction(newValue);

            // THEN
            QVERIFY(spy.isValid());
            QCOMPARE(abstractTexture.comparisonFunction(), newValue);
            QCOMPARE(spy.count(), 1);

            // WHEN
            spy.clear();
            abstractTexture.setComparisonFunction(newValue);

            // THEN
            QCOMPARE(abstractTexture.comparisonFunction(), newValue);
            QCOMPARE(spy.count(), 0);
        }
        {
            // WHEN
            QSignalSpy spy(&abstractTexture, SIGNAL(comparisonModeChanged(Qt3DRender::QAbstractTexture::ComparisonMode)));
            const Qt3DRender::QAbstractTexture::ComparisonMode newValue = Qt3DRender::QAbstractTexture::CompareRefToTexture;
            abstractTexture.setComparisonMode(newValue);

            // THEN
            QVERIFY(spy.isValid());
            QCOMPARE(abstractTexture.comparisonMode(), newValue);
            QCOMPARE(spy.count(), 1);

            // WHEN
            spy.clear();
            abstractTexture.setComparisonMode(newValue);

            // THEN
            QCOMPARE(abstractTexture.comparisonMode(), newValue);
            QCOMPARE(spy.count(), 0);
        }
        {
            // WHEN
            QSignalSpy spy(&abstractTexture, SIGNAL(layersChanged(int)));
            const int newValue = 512;
            abstractTexture.setLayers(newValue);

            // THEN
            QVERIFY(spy.isValid());
            QCOMPARE(abstractTexture.layers(), newValue);
            QCOMPARE(spy.count(), 1);

            // WHEN
            spy.clear();
            abstractTexture.setLayers(newValue);

            // THEN
            QCOMPARE(abstractTexture.layers(), newValue);
            QCOMPARE(spy.count(), 0);
        }
        {
            // WHEN
            QSignalSpy spy(&abstractTexture, SIGNAL(samplesChanged(int)));
            const int newValue = 1024;
            abstractTexture.setSamples(newValue);

            // THEN
            QVERIFY(spy.isValid());
            QCOMPARE(abstractTexture.samples(), newValue);
            QCOMPARE(spy.count(), 1);

            // WHEN
            spy.clear();
            abstractTexture.setSamples(newValue);

            // THEN
            QCOMPARE(abstractTexture.samples(), newValue);
            QCOMPARE(spy.count(), 0);
        }
    }

    void checkCreationData()
    {
        // GIVEN
        FakeTexture abstractTexture;

        abstractTexture.setFormat(Qt3DRender::QAbstractTexture::RG3B2);
        abstractTexture.setGenerateMipMaps(true);
        abstractTexture.setWidth(350);
        abstractTexture.setHeight(383);
        abstractTexture.setDepth(396);
        abstractTexture.setMagnificationFilter(Qt3DRender::QAbstractTexture::NearestMipMapLinear);
        abstractTexture.setMinificationFilter(Qt3DRender::QAbstractTexture::NearestMipMapNearest);
        abstractTexture.setMaximumAnisotropy(12.0f);
        abstractTexture.setComparisonFunction(Qt3DRender::QAbstractTexture::CommpareNotEqual);
        abstractTexture.setComparisonMode(Qt3DRender::QAbstractTexture::CompareRefToTexture);
        abstractTexture.setLayers(128);
        abstractTexture.setSamples(256);
        abstractTexture.setWrapMode(Qt3DRender::QTextureWrapMode(Qt3DRender::QTextureWrapMode::ClampToBorder));

        FakeTextureImage image;
        FakeTextureImage image2;
        abstractTexture.addTextureImage(&image);
        abstractTexture.addTextureImage(&image2);

        // WHEN
        QVector<Qt3DCore::QNodeCreatedChangeBasePtr> creationChanges;

        {
            Qt3DCore::QNodeCreatedChangeGenerator creationChangeGenerator(&abstractTexture);
            creationChanges = creationChangeGenerator.creationChanges();
        }

        // THEN
        {
            QCOMPARE(creationChanges.size(), 3); // Texture + Images

            const auto creationChangeData = qSharedPointerCast<Qt3DCore::QNodeCreatedChange<Qt3DRender::QAbstractTextureData>>(creationChanges.first());
            const Qt3DRender::QAbstractTextureData cloneData = creationChangeData->data;

            QCOMPARE(abstractTexture.target(), cloneData.target);
            QCOMPARE(abstractTexture.format(), cloneData.format);
            QCOMPARE(abstractTexture.generateMipMaps(), cloneData.autoMipMap);
            QCOMPARE(abstractTexture.wrapMode()->x(), cloneData.wrapModeX);
            QCOMPARE(abstractTexture.wrapMode()->x(), cloneData.wrapModeY);
            QCOMPARE(abstractTexture.wrapMode()->x(), cloneData.wrapModeZ);
            QCOMPARE(abstractTexture.width(), cloneData.width);
            QCOMPARE(abstractTexture.height(), cloneData.height);
            QCOMPARE(abstractTexture.depth(), cloneData.depth);
            QCOMPARE(abstractTexture.magnificationFilter(), cloneData.magFilter);
            QCOMPARE(abstractTexture.minificationFilter(), cloneData.minFilter);
            QCOMPARE(abstractTexture.maximumAnisotropy(), cloneData.maximumAnisotropy);
            QCOMPARE(abstractTexture.comparisonFunction(), cloneData.comparisonFunction);
            QCOMPARE(abstractTexture.comparisonMode(), cloneData.comparisonMode);
            QCOMPARE(abstractTexture.layers(), cloneData.layers);
            QCOMPARE(abstractTexture.samples(), cloneData.samples);
            QCOMPARE(abstractTexture.textureImages().size(), cloneData.textureImageIds.size());

            for (int i = 0, m = abstractTexture.textureImages().size(); i < m; ++i)
                QCOMPARE(abstractTexture.textureImages().at(i)->id(), cloneData.textureImageIds.at(i));

            QCOMPARE(abstractTexture.id(), creationChangeData->subjectId());
            QCOMPARE(abstractTexture.isEnabled(), true);
            QCOMPARE(abstractTexture.isEnabled(), creationChangeData->isNodeEnabled());
            QCOMPARE(abstractTexture.metaObject(), creationChangeData->metaObject());
        }

        // WHEN
        abstractTexture.setEnabled(false);

        {
            Qt3DCore::QNodeCreatedChangeGenerator creationChangeGenerator(&abstractTexture);
            creationChanges = creationChangeGenerator.creationChanges();
        }

        // THEN
        {
            QCOMPARE(creationChanges.size(), 3); // Texture + Images

            const auto creationChangeData = qSharedPointerCast<Qt3DCore::QNodeCreatedChange<Qt3DRender::QAbstractTextureData>>(creationChanges.first());
            const Qt3DRender::QAbstractTextureData cloneData = creationChangeData->data;

            QCOMPARE(abstractTexture.target(), cloneData.target);
            QCOMPARE(abstractTexture.format(), cloneData.format);
            QCOMPARE(abstractTexture.generateMipMaps(), cloneData.autoMipMap);
            QCOMPARE(abstractTexture.wrapMode()->x(), cloneData.wrapModeX);
            QCOMPARE(abstractTexture.wrapMode()->x(), cloneData.wrapModeY);
            QCOMPARE(abstractTexture.wrapMode()->x(), cloneData.wrapModeZ);
            QCOMPARE(abstractTexture.width(), cloneData.width);
            QCOMPARE(abstractTexture.height(), cloneData.height);
            QCOMPARE(abstractTexture.depth(), cloneData.depth);
            QCOMPARE(abstractTexture.magnificationFilter(), cloneData.magFilter);
            QCOMPARE(abstractTexture.minificationFilter(), cloneData.minFilter);
            QCOMPARE(abstractTexture.maximumAnisotropy(), cloneData.maximumAnisotropy);
            QCOMPARE(abstractTexture.comparisonFunction(), cloneData.comparisonFunction);
            QCOMPARE(abstractTexture.comparisonMode(), cloneData.comparisonMode);
            QCOMPARE(abstractTexture.layers(), cloneData.layers);
            QCOMPARE(abstractTexture.samples(), cloneData.samples);
            QCOMPARE(abstractTexture.textureImages().size(), cloneData.textureImageIds.size());

            for (int i = 0, m = abstractTexture.textureImages().size(); i < m; ++i)
                QCOMPARE(abstractTexture.textureImages().at(i)->id(), cloneData.textureImageIds.at(i));

            QCOMPARE(abstractTexture.id(), creationChangeData->subjectId());
            QCOMPARE(abstractTexture.isEnabled(), false);
            QCOMPARE(abstractTexture.isEnabled(), creationChangeData->isNodeEnabled());
            QCOMPARE(abstractTexture.metaObject(), creationChangeData->metaObject());
        }
    }

    void checkFormatUpdate()
    {
        // GIVEN
        TestArbiter arbiter;
        FakeTexture abstractTexture;
        arbiter.setArbiterOnNode(&abstractTexture);

        {
            // WHEN
            abstractTexture.setFormat(Qt3DRender::QAbstractTexture::RG8_UNorm);
            QCoreApplication::processEvents();

            // THEN
            QCOMPARE(arbiter.events.size(), 1);
            auto change = arbiter.events.first().staticCast<Qt3DCore::QPropertyUpdatedChange>();
            QCOMPARE(change->propertyName(), "format");
            QCOMPARE(change->value().value<Qt3DRender::QAbstractTexture::TextureFormat>(), abstractTexture.format());
            QCOMPARE(change->type(), Qt3DCore::PropertyUpdated);

            arbiter.events.clear();
        }

        {
            // WHEN
            abstractTexture.setFormat(Qt3DRender::QAbstractTexture::RG8_UNorm);
            QCoreApplication::processEvents();

            // THEN
            QCOMPARE(arbiter.events.size(), 0);
        }

    }

    void checkGenerateMipMapsUpdate()
    {
        // GIVEN
        TestArbiter arbiter;
        FakeTexture abstractTexture;
        arbiter.setArbiterOnNode(&abstractTexture);

        {
            // WHEN
            abstractTexture.setGenerateMipMaps(true);
            QCoreApplication::processEvents();

            // THEN
            QCOMPARE(arbiter.events.size(), 1);
            auto change = arbiter.events.first().staticCast<Qt3DCore::QPropertyUpdatedChange>();
            QCOMPARE(change->propertyName(), "generateMipMaps");
            QCOMPARE(change->value().value<bool>(), abstractTexture.generateMipMaps());
            QCOMPARE(change->type(), Qt3DCore::PropertyUpdated);

            arbiter.events.clear();
        }

        {
            // WHEN
            abstractTexture.setGenerateMipMaps(true);
            QCoreApplication::processEvents();

            // THEN
            QCOMPARE(arbiter.events.size(), 0);
        }

    }

    void checkWidthUpdate()
    {
        // GIVEN
        TestArbiter arbiter;
        FakeTexture abstractTexture;
        arbiter.setArbiterOnNode(&abstractTexture);

        {
            // WHEN
            abstractTexture.setWidth(1024);
            QCoreApplication::processEvents();

            // THEN
            QCOMPARE(arbiter.events.size(), 1);
            auto change = arbiter.events.first().staticCast<Qt3DCore::QPropertyUpdatedChange>();
            QCOMPARE(change->propertyName(), "width");
            QCOMPARE(change->value().value<int>(), abstractTexture.width());
            QCOMPARE(change->type(), Qt3DCore::PropertyUpdated);

            arbiter.events.clear();
        }

        {
            // WHEN
            abstractTexture.setWidth(1024);
            QCoreApplication::processEvents();

            // THEN
            QCOMPARE(arbiter.events.size(), 0);
        }

    }

    void checkHeightUpdate()
    {
        // GIVEN
        TestArbiter arbiter;
        FakeTexture abstractTexture;
        arbiter.setArbiterOnNode(&abstractTexture);

        {
            // WHEN
            abstractTexture.setHeight(256);
            QCoreApplication::processEvents();

            // THEN
            QCOMPARE(arbiter.events.size(), 1);
            auto change = arbiter.events.first().staticCast<Qt3DCore::QPropertyUpdatedChange>();
            QCOMPARE(change->propertyName(), "height");
            QCOMPARE(change->value().value<int>(), abstractTexture.height());
            QCOMPARE(change->type(), Qt3DCore::PropertyUpdated);

            arbiter.events.clear();
        }

        {
            // WHEN
            abstractTexture.setHeight(256);
            QCoreApplication::processEvents();

            // THEN
            QCOMPARE(arbiter.events.size(), 0);
        }

    }

    void checkDepthUpdate()
    {
        // GIVEN
        TestArbiter arbiter;
        FakeTexture abstractTexture;
        arbiter.setArbiterOnNode(&abstractTexture);

        {
            // WHEN
            abstractTexture.setDepth(512);
            QCoreApplication::processEvents();

            // THEN
            QCOMPARE(arbiter.events.size(), 1);
            auto change = arbiter.events.first().staticCast<Qt3DCore::QPropertyUpdatedChange>();
            QCOMPARE(change->propertyName(), "depth");
            QCOMPARE(change->value().value<int>(), abstractTexture.depth());
            QCOMPARE(change->type(), Qt3DCore::PropertyUpdated);

            arbiter.events.clear();
        }

        {
            // WHEN
            abstractTexture.setDepth(512);
            QCoreApplication::processEvents();

            // THEN
            QCOMPARE(arbiter.events.size(), 0);
        }

    }

    void checkMagnificationFilterUpdate()
    {
        // GIVEN
        TestArbiter arbiter;
        FakeTexture abstractTexture;
        arbiter.setArbiterOnNode(&abstractTexture);

        {
            // WHEN
            abstractTexture.setMagnificationFilter(Qt3DRender::QAbstractTexture::NearestMipMapLinear);
            QCoreApplication::processEvents();

            // THEN
            QCOMPARE(arbiter.events.size(), 1);
            auto change = arbiter.events.first().staticCast<Qt3DCore::QPropertyUpdatedChange>();
            QCOMPARE(change->propertyName(), "magnificationFilter");
            QCOMPARE(change->value().value<Qt3DRender::QAbstractTexture::Filter>(), abstractTexture.magnificationFilter());
            QCOMPARE(change->type(), Qt3DCore::PropertyUpdated);

            arbiter.events.clear();
        }

        {
            // WHEN
            abstractTexture.setMagnificationFilter(Qt3DRender::QAbstractTexture::NearestMipMapLinear);
            QCoreApplication::processEvents();

            // THEN
            QCOMPARE(arbiter.events.size(), 0);
        }

    }

    void checkMinificationFilterUpdate()
    {
        // GIVEN
        TestArbiter arbiter;
        FakeTexture abstractTexture;
        arbiter.setArbiterOnNode(&abstractTexture);

        {
            // WHEN
            abstractTexture.setMinificationFilter(Qt3DRender::QAbstractTexture::NearestMipMapLinear);
            QCoreApplication::processEvents();

            // THEN
            QCOMPARE(arbiter.events.size(), 1);
            auto change = arbiter.events.first().staticCast<Qt3DCore::QPropertyUpdatedChange>();
            QCOMPARE(change->propertyName(), "minificationFilter");
            QCOMPARE(change->value().value<Qt3DRender::QAbstractTexture::Filter>(), abstractTexture.minificationFilter());
            QCOMPARE(change->type(), Qt3DCore::PropertyUpdated);

            arbiter.events.clear();
        }

        {
            // WHEN
            abstractTexture.setMinificationFilter(Qt3DRender::QAbstractTexture::NearestMipMapLinear);
            QCoreApplication::processEvents();

            // THEN
            QCOMPARE(arbiter.events.size(), 0);
        }

    }

    void checkMaximumAnisotropyUpdate()
    {
        // GIVEN
        TestArbiter arbiter;
        FakeTexture abstractTexture;
        arbiter.setArbiterOnNode(&abstractTexture);

        {
            // WHEN
            abstractTexture.setMaximumAnisotropy(327.0f);
            QCoreApplication::processEvents();

            // THEN
            QCOMPARE(arbiter.events.size(), 1);
            auto change = arbiter.events.first().staticCast<Qt3DCore::QPropertyUpdatedChange>();
            QCOMPARE(change->propertyName(), "maximumAnisotropy");
            QCOMPARE(change->value().value<float>(), abstractTexture.maximumAnisotropy());
            QCOMPARE(change->type(), Qt3DCore::PropertyUpdated);

            arbiter.events.clear();
        }

        {
            // WHEN
            abstractTexture.setMaximumAnisotropy(327.0f);
            QCoreApplication::processEvents();

            // THEN
            QCOMPARE(arbiter.events.size(), 0);
        }

    }

    void checkComparisonFunctionUpdate()
    {
        // GIVEN
        TestArbiter arbiter;
        FakeTexture abstractTexture;
        arbiter.setArbiterOnNode(&abstractTexture);

        {
            // WHEN
            abstractTexture.setComparisonFunction(Qt3DRender::QAbstractTexture::CompareAlways);
            QCoreApplication::processEvents();

            // THEN
            QCOMPARE(arbiter.events.size(), 1);
            auto change = arbiter.events.first().staticCast<Qt3DCore::QPropertyUpdatedChange>();
            QCOMPARE(change->propertyName(), "comparisonFunction");
            QCOMPARE(change->value().value<Qt3DRender::QAbstractTexture::ComparisonFunction>(), abstractTexture.comparisonFunction());
            QCOMPARE(change->type(), Qt3DCore::PropertyUpdated);

            arbiter.events.clear();
        }

        {
            // WHEN
            abstractTexture.setComparisonFunction(Qt3DRender::QAbstractTexture::CompareAlways);
            QCoreApplication::processEvents();

            // THEN
            QCOMPARE(arbiter.events.size(), 0);
        }

    }

    void checkComparisonModeUpdate()
    {
        // GIVEN
        TestArbiter arbiter;
        FakeTexture abstractTexture;
        arbiter.setArbiterOnNode(&abstractTexture);

        {
            // WHEN
            abstractTexture.setComparisonMode(Qt3DRender::QAbstractTexture::CompareRefToTexture);
            QCoreApplication::processEvents();

            // THEN
            QCOMPARE(arbiter.events.size(), 1);
            auto change = arbiter.events.first().staticCast<Qt3DCore::QPropertyUpdatedChange>();
            QCOMPARE(change->propertyName(), "comparisonMode");
            QCOMPARE(change->value().value<Qt3DRender::QAbstractTexture::ComparisonMode>(), abstractTexture.comparisonMode());
            QCOMPARE(change->type(), Qt3DCore::PropertyUpdated);

            arbiter.events.clear();
        }

        {
            // WHEN
            abstractTexture.setComparisonMode(Qt3DRender::QAbstractTexture::CompareRefToTexture);
            QCoreApplication::processEvents();

            // THEN
            QCOMPARE(arbiter.events.size(), 0);
        }

    }

    void checkLayersUpdate()
    {
        // GIVEN
        TestArbiter arbiter;
        FakeTexture abstractTexture;
        arbiter.setArbiterOnNode(&abstractTexture);

        {
            // WHEN
            abstractTexture.setLayers(64);
            QCoreApplication::processEvents();

            // THEN
            QCOMPARE(arbiter.events.size(), 1);
            auto change = arbiter.events.first().staticCast<Qt3DCore::QPropertyUpdatedChange>();
            QCOMPARE(change->propertyName(), "layers");
            QCOMPARE(change->value().value<int>(), abstractTexture.layers());
            QCOMPARE(change->type(), Qt3DCore::PropertyUpdated);

            arbiter.events.clear();
        }

        {
            // WHEN
            abstractTexture.setLayers(64);
            QCoreApplication::processEvents();

            // THEN
            QCOMPARE(arbiter.events.size(), 0);
        }

    }

    void checkSamplesUpdate()
    {
        // GIVEN
        TestArbiter arbiter;
        FakeTexture abstractTexture;
        arbiter.setArbiterOnNode(&abstractTexture);

        {
            // WHEN
            abstractTexture.setSamples(16);
            QCoreApplication::processEvents();

            // THEN
            QCOMPARE(arbiter.events.size(), 1);
            auto change = arbiter.events.first().staticCast<Qt3DCore::QPropertyUpdatedChange>();
            QCOMPARE(change->propertyName(), "samples");
            QCOMPARE(change->value().value<int>(), abstractTexture.samples());
            QCOMPARE(change->type(), Qt3DCore::PropertyUpdated);

            arbiter.events.clear();
        }

        {
            // WHEN
            abstractTexture.setSamples(16);
            QCoreApplication::processEvents();

            // THEN
            QCOMPARE(arbiter.events.size(), 0);
        }

    }

    void checkTextureImageAdded()
    {
        // GIVEN
        TestArbiter arbiter;
        FakeTexture abstractTexture;
        arbiter.setArbiterOnNode(&abstractTexture);

        {
            // WHEN
            FakeTextureImage image;
            abstractTexture.addTextureImage(&image);
            QCoreApplication::processEvents();

            // THEN
            QCOMPARE(arbiter.events.size(), 1);
            auto change = arbiter.events.last().staticCast<Qt3DCore::QPropertyNodeAddedChange>();
            QCOMPARE(change->type(), Qt3DCore::PropertyValueAdded);
            QCOMPARE(change->propertyName(), "textureImage");
            QCOMPARE(change->addedNodeId(), image.id());

            arbiter.events.clear();
        }
    }

    void checkTextureImageRemoved()
    {
        // GIVEN
        TestArbiter arbiter;
        FakeTexture abstractTexture;
        FakeTextureImage image;
        abstractTexture.addTextureImage(&image);
        arbiter.setArbiterOnNode(&abstractTexture);

        {
            // WHEN
            abstractTexture.removeTextureImage(&image);
            QCoreApplication::processEvents();

            // THEN
            QCOMPARE(arbiter.events.size(), 1);
            auto change = arbiter.events.last().staticCast<Qt3DCore::QPropertyNodeRemovedChange>();
            QCOMPARE(change->type(), Qt3DCore::PropertyValueRemoved);
            QCOMPARE(change->propertyName(), "textureImage");
            QCOMPARE(change->removedNodeId(), image.id());

            arbiter.events.clear();
        }
    }

};

QTEST_MAIN(tst_QAbstractTexture)

#include "tst_qabstracttexture.moc"
