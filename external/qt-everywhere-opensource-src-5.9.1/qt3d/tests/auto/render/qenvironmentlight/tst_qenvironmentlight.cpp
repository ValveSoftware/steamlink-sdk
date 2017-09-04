/****************************************************************************
**
** Copyright (C) 2016 The Qt Company Ltd.
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
#include <QSignalSpy>

#include <Qt3DRender/qtexture.h>
#include <Qt3DRender/qenvironmentlight.h>
#include <Qt3DRender/private/qenvironmentlight_p.h>
#include <Qt3DRender/private/qshaderdata_p.h>

#include <Qt3DCore/private/qnodecreatedchangegenerator_p.h>
#include <Qt3DCore/qnodecreatedchange.h>
#include "testpostmanarbiter.h"

class tst_QEnvironmentLight: public QObject
{
    Q_OBJECT
private Q_SLOTS:
    void checkDefaultConstruction()
    {
        // GIVEN
        Qt3DRender::QEnvironmentLight light;

        // THEN
        QVERIFY(light.findChild<Qt3DRender::QShaderData*>());
        QCOMPARE(light.irradiance(), nullptr);
        QCOMPARE(light.specular(), nullptr);
    }

    void shouldTakeOwnershipOfParentlessTextures()
    {
        // GIVEN
        Qt3DRender::QEnvironmentLight light;
        auto irradiance = new Qt3DRender::QTexture2D;
        auto specular = new Qt3DRender::QTexture2D;

        // WHEN
        light.setIrradiance(irradiance);
        light.setSpecular(specular);

        // THEN
        QCOMPARE(irradiance->parent(), &light);
        QCOMPARE(specular->parent(), &light);
    }

    void shouldNotChangeOwnershipOfParentedTextures()
    {
        // GIVEN
        Qt3DCore::QNode node;
        Qt3DRender::QEnvironmentLight light;
        auto irradiance = new Qt3DRender::QTexture2D(&node);
        auto specular = new Qt3DRender::QTexture2D(&node);

        // WHEN
        light.setIrradiance(irradiance);
        light.setSpecular(specular);

        // WHEN
        delete irradiance;
        delete specular;

        // THEN
    }

    void checkPropertyChanges()
    {
        // GIVEN
        Qt3DRender::QEnvironmentLight light;
        auto shaderData = light.findChild<Qt3DRender::QShaderData*>();

        {
            auto texture = new Qt3DRender::QTexture2D(&light);
            QSignalSpy spy(&light, &Qt3DRender::QEnvironmentLight::irradianceChanged);

            // WHEN
            light.setIrradiance(texture);

            // THEN
            QCOMPARE(light.irradiance(), texture);
            QCOMPARE(shaderData->property("irradiance").value<Qt3DRender::QAbstractTexture*>(), texture);
            QCOMPARE(spy.count(), 1);
            QCOMPARE(spy.takeFirst().first().value<Qt3DRender::QAbstractTexture*>(), texture);

            // WHEN
            light.setIrradiance(texture);

            // THEN
            QCOMPARE(light.irradiance(), texture);
            QCOMPARE(shaderData->property("irradiance").value<Qt3DRender::QAbstractTexture*>(), texture);
            QCOMPARE(spy.count(), 0);

            // WHEN
            light.setIrradiance(nullptr);

            // THEN
            QCOMPARE(light.irradiance(), nullptr);
            QCOMPARE(shaderData->property("irradiance").value<Qt3DRender::QAbstractTexture*>(), nullptr);
            QCOMPARE(spy.count(), 1);
            QCOMPARE(spy.takeFirst().first().value<Qt3DRender::QAbstractTexture*>(), nullptr);
        }
        {
            auto texture = new Qt3DRender::QTexture2D(&light);
            QSignalSpy spy(&light, &Qt3DRender::QEnvironmentLight::irradianceChanged);

            // WHEN
            light.setIrradiance(texture);

            // THEN
            QCOMPARE(light.irradiance(), texture);
            QCOMPARE(shaderData->property("irradiance").value<Qt3DRender::QAbstractTexture*>(), texture);
            QCOMPARE(spy.count(), 1);
            QCOMPARE(spy.takeFirst().first().value<Qt3DRender::QAbstractTexture*>(), texture);

            // WHEN
            delete texture;

            // THEN
            QCOMPARE(light.irradiance(), nullptr);
            QCOMPARE(shaderData->property("irradiance").value<Qt3DRender::QAbstractTexture*>(), nullptr);
            QCOMPARE(spy.count(), 1);
            QCOMPARE(spy.takeFirst().first().value<Qt3DRender::QAbstractTexture*>(), nullptr);
        }
        {
            auto texture = new Qt3DRender::QTexture2D;
            QSignalSpy spy(&light, &Qt3DRender::QEnvironmentLight::irradianceChanged);

            // WHEN
            light.setIrradiance(texture);

            // THEN
            QCOMPARE(light.irradiance(), texture);
            QCOMPARE(shaderData->property("irradiance").value<Qt3DRender::QAbstractTexture*>(), texture);
            QCOMPARE(spy.count(), 1);
            QCOMPARE(spy.takeFirst().first().value<Qt3DRender::QAbstractTexture*>(), texture);

            // WHEN
            delete texture;

            // THEN
            QCOMPARE(light.irradiance(), nullptr);
            QCOMPARE(shaderData->property("irradiance").value<Qt3DRender::QAbstractTexture*>(), nullptr);
            QCOMPARE(spy.count(), 1);
            QCOMPARE(spy.takeFirst().first().value<Qt3DRender::QAbstractTexture*>(), nullptr);
        }
        {
            auto texture = new Qt3DRender::QTexture2D(&light);
            QSignalSpy spy(&light, &Qt3DRender::QEnvironmentLight::specularChanged);

            // WHEN
            light.setSpecular(texture);

            // THEN
            QCOMPARE(light.specular(), texture);
            QCOMPARE(shaderData->property("specular").value<Qt3DRender::QAbstractTexture*>(), texture);
            QCOMPARE(spy.count(), 1);
            QCOMPARE(spy.takeFirst().first().value<Qt3DRender::QAbstractTexture*>(), texture);

            // WHEN
            light.setSpecular(texture);

            // THEN
            QCOMPARE(light.specular(), texture);
            QCOMPARE(shaderData->property("specular").value<Qt3DRender::QAbstractTexture*>(), texture);
            QCOMPARE(spy.count(), 0);

            // WHEN
            light.setSpecular(nullptr);

            // THEN
            QCOMPARE(light.specular(), nullptr);
            QCOMPARE(shaderData->property("specular").value<Qt3DRender::QAbstractTexture*>(), nullptr);
            QCOMPARE(spy.count(), 1);
            QCOMPARE(spy.takeFirst().first().value<Qt3DRender::QAbstractTexture*>(), nullptr);
        }
        {
            auto texture = new Qt3DRender::QTexture2D(&light);
            QSignalSpy spy(&light, &Qt3DRender::QEnvironmentLight::specularChanged);

            // WHEN
            light.setSpecular(texture);

            // THEN
            QCOMPARE(light.specular(), texture);
            QCOMPARE(shaderData->property("specular").value<Qt3DRender::QAbstractTexture*>(), texture);
            QCOMPARE(spy.count(), 1);
            QCOMPARE(spy.takeFirst().first().value<Qt3DRender::QAbstractTexture*>(), texture);

            // WHEN
            delete texture;

            // THEN
            QCOMPARE(light.specular(), nullptr);
            QCOMPARE(shaderData->property("specular").value<Qt3DRender::QAbstractTexture*>(), nullptr);
            QCOMPARE(spy.count(), 1);
            QCOMPARE(spy.takeFirst().first().value<Qt3DRender::QAbstractTexture*>(), nullptr);
        }
        {
            auto texture = new Qt3DRender::QTexture2D;
            QSignalSpy spy(&light, &Qt3DRender::QEnvironmentLight::specularChanged);

            // WHEN
            light.setSpecular(texture);

            // THEN
            QCOMPARE(light.specular(), texture);
            QCOMPARE(shaderData->property("specular").value<Qt3DRender::QAbstractTexture*>(), texture);
            QCOMPARE(spy.count(), 1);
            QCOMPARE(spy.takeFirst().first().value<Qt3DRender::QAbstractTexture*>(), texture);

            // WHEN
            delete texture;

            // THEN
            QCOMPARE(light.specular(), nullptr);
            QCOMPARE(shaderData->property("specular").value<Qt3DRender::QAbstractTexture*>(), nullptr);
            QCOMPARE(spy.count(), 1);
            QCOMPARE(spy.takeFirst().first().value<Qt3DRender::QAbstractTexture*>(), nullptr);
        }
    }

    void checkCreationData()
    {
        // GIVEN
        Qt3DRender::QEnvironmentLight light;
        auto shaderData = light.findChild<Qt3DRender::QShaderData*>();

        // WHEN
        QVector<Qt3DCore::QNodeCreatedChangeBasePtr> creationChanges;

        {
            Qt3DCore::QNodeCreatedChangeGenerator creationChangeGenerator(&light);
            creationChanges = creationChangeGenerator.creationChanges();
        }

        // THEN
        {
            QCOMPARE(creationChanges.size(), 2); // EnvironmentLight + ShaderData

            const auto creationChangeData = qSharedPointerCast<Qt3DCore::QNodeCreatedChange<Qt3DRender::QEnvironmentLightData>>(creationChanges.first());
            const Qt3DRender::QEnvironmentLightData cloneData = creationChangeData->data;

            QCOMPARE(cloneData.shaderDataId, shaderData->id());
            QCOMPARE(light.id(), creationChangeData->subjectId());
            QCOMPARE(light.isEnabled(), true);
            QCOMPARE(light.isEnabled(), creationChangeData->isNodeEnabled());
            QCOMPARE(light.metaObject(), creationChangeData->metaObject());
        }

        // WHEN
        light.setEnabled(false);

        {
            Qt3DCore::QNodeCreatedChangeGenerator creationChangeGenerator(&light);
            creationChanges = creationChangeGenerator.creationChanges();
        }

        // THEN
        {
            QCOMPARE(creationChanges.size(), 2); // EnvironmentLight + ShaderData

            const auto creationChangeData = qSharedPointerCast<Qt3DCore::QNodeCreatedChange<Qt3DRender::QEnvironmentLightData>>(creationChanges.first());
            const Qt3DRender::QEnvironmentLightData cloneData = creationChangeData->data;

            QCOMPARE(cloneData.shaderDataId, shaderData->id());
            QCOMPARE(light.id(), creationChangeData->subjectId());
            QCOMPARE(light.isEnabled(), false);
            QCOMPARE(light.isEnabled(), creationChangeData->isNodeEnabled());
            QCOMPARE(light.metaObject(), creationChangeData->metaObject());
        }
    }
};

QTEST_MAIN(tst_QEnvironmentLight)

#include "tst_qenvironmentlight.moc"
